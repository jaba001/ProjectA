#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Game/Run/RunSaveFormat.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridTile.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SViewport.h"

namespace ExistingSaveContinueReview
{
    FString AbsolutePath(const FString& Path)
    {
        FString Result = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeDirectoryName(Result);
        return Result;
    }

    bool IsSHA256Literal(const FString& Value)
    {
        if (Value.Len() != 64) return false;
        for (TCHAR Character : Value) if (!FChar::IsHexDigit(Character)) return false;
        return true;
    }

    struct FObservation
    {
        FString Root;
        FString Slot;
        FString SourcePath;
        TArray<uint8> OriginalBytes;
        TStrongObjectPtr<URunSaveGame> Raw;
        TStrongObjectPtr<URunSaveGame> Expected;
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    };

    // Exercise the authored Continue delegate and public restoration; only the externally owned clone can be saved.
    // 작성된 Continue delegate와 공개 복원 경로를 실행하며 외부에서 소유한 사본만 저장할 수 있습니다.
    class FReview : public IAutomationLatentCommand
    {
    public:
        FReview(FAutomationTestBase* InTest, TSharedRef<FObservation> InObservation) : Test(InTest), Observation(MoveTemp(InObservation)) {}

        virtual ~FReview() override
        {
            ReleaseViewport();
        }

        virtual bool Update() override
        {
            if (Started == 0.0) Started = FPlatformTime::Seconds();
            if (Stage == 99)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.WorldType != EWorldType::PIE) continue;
                    if (FPlatformTime::Seconds() - Started < 30.0) return false;
                    Check(false, TEXT("The isolated Continue PIE closes before its report is finalized."));
                }
                ReleaseViewport();
                TArray<uint8> OriginalAfter;
                const bool bUnchanged = FFileHelper::LoadFileToArray(OriginalAfter, *Observation->SourcePath) && OriginalAfter == Observation->OriginalBytes;
                Check(bUnchanged, TEXT("The original source bytes remain unchanged after clone-only production Continue."));
                Observation->Report->SetBoolField(TEXT("original_bytes_unchanged"), bUnchanged);
                if (bUnchanged) Observation->Report->SetStringField(TEXT("cpp_original_sha1_after"), FSHA1::HashBuffer(OriginalAfter.GetData(), OriginalAfter.Num()).ToString());
                Observation->Report->SetBoolField(TEXT("passed"), bChecksPassed && bGameplayRestored);
                Observation->Report->SetBoolField(TEXT("observation_completed"), bObservationCompleted);
                FString Json;
                Check(FJsonSerializer::Serialize(Observation->Report, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Observation->Root / TEXT("Saved/Automation/TodoReview/SavedOriginalContinueReview.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM), TEXT("The isolated Continue result is written to Saved only."));
                return true;
            }
            if (FPlatformTime::Seconds() - Started > 60.0)
            {
                Observation->Report->SetStringField(TEXT("outcome"), TEXT("TimedOut"));
                Check(false, FString::Printf(TEXT("Original-save clone Continue timed out at stage %d; no eligibility repair was applied."), Stage));
                return End();
            }
            if (Stage == 0)
            {
                Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
                Settings->SetPlayNetMode(PIE_Standalone);
                Settings->SetPlayNumberOfClients(1);
                Settings->SetRunUnderOneProcess(true);
                Settings->bLaunchSeparateServer = false;
                Settings->NewWindowWidth = 1280;
                Settings->NewWindowHeight = 720;
                Settings->SetClientWindowSize(FIntPoint(1280, 720));
                FRequestPlaySessionParams Params;
                Params.EditorPlaySettings = Settings.Get();
                Params.SessionDestination = EPlaySessionDestinationType::InProcess;
                Params.WorldType = EPlaySessionWorldType::PlayInEditor;
                Params.bAllowOnlineSubsystem = false;
                Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu");
                GEditor->RequestPlaySession(Params);
                Stage = 1;
                return false;
            }
            UWorld* World = GEditor->PlayWorld;
            URunStateSubsystem* Run = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
            if (!World || !Run) return false;
            if (Stage == 1)
            {
                AMainMenuPlayerController* MenuController = Cast<AMainMenuPlayerController>(World->GetFirstPlayerController());
                if (!MenuController) return false;
                TArray<UUserWidget*> Widgets;
                UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UMainMenuScreenWidget::StaticClass(), false);
                for (UUserWidget* Widget : Widgets)
                {
                    UMainMenuScreenWidget* Menu = Cast<UMainMenuScreenWidget>(Widget);
                    if (!Menu || !Menu->IsActivated() || Menu->GetOwningPlayer() != MenuController) continue;
                    UButton* Continue = Cast<UButton>(Menu->GetWidgetFromName(TEXT("Button_Continue")));
                    if (!Check(Continue != nullptr, TEXT("The actual active main menu has its authored Continue button."))) return End();
                    FText EligibilityError;
                    const bool bEligible = Run->CanContinueStandaloneSavedRun(EligibilityError);
                    Observation->Report->SetBoolField(TEXT("eligible"), bEligible);
                    Observation->Report->SetStringField(TEXT("eligibility_error"), EligibilityError.ToString());
                    Observation->Report->SetBoolField(TEXT("actual_button_enabled"), Continue->GetIsEnabled());
                    const UTextBlock* SaveStatus = Cast<UTextBlock>(Menu->GetWidgetFromName(TEXT("SaveStatus")));
                    Observation->Report->SetStringField(TEXT("visible_save_status"), SaveStatus ? SaveStatus->GetText().ToString() : TEXT("missing"));
                    if (!bEligible)
                    {
                        Check(!Continue->GetIsEnabled() && !EligibilityError.IsEmpty() && SaveStatus && SaveStatus->GetText().ToString() == EligibilityError.ToString(), TEXT("A refused original-save clone remains disabled and exposes its actual eligibility reason in the menu."));
                        Observation->Report->SetStringField(TEXT("outcome"), TEXT("EligibilityRefused"));
                        bObservationCompleted = true;
                        Check(false, TEXT("Original-save clone Continue was refused by production eligibility: ") + EligibilityError.ToString());
                        return End();
                    }
                    if (!Check(Continue->GetIsEnabled(), TEXT("The eligible existing-save clone enables the actual Continue button."))) return End();
                    if (!Check(Observation->Expected.IsValid(), TEXT("The official clone migration supplies the expected restore data before an eligible Continue."))) return End();
                    Observation->Report->SetBoolField(TEXT("actual_continue_delegate_invoked"), true);
                    Continue->OnClicked.Broadcast();
                    Stage = 2;
                    Started = FPlatformTime::Seconds();
                    return false;
                }
                return false;
            }
            if (!Cast<AGameplayPlayerController>(World->GetFirstPlayerController())) return false;
            ACombatManager* Combat = nullptr;
            for (TActorIterator<ACombatManager> It(World); It; ++It)
            {
                if (!It->GetRoundCoordinator()) continue;
                Combat = *It;
                break;
            }
            ACombatRoundCoordinator* Round = Combat ? Combat->GetRoundCoordinator() : nullptr;
            if (!Round || Round->GetView().Phase != ECombatRoundPhase::Planning) return false;
            if (!VerifyRestored(Run, Combat, Round)) return End();
            if (++WarmFrames < 12) return false;
            if (!Capture(World)) return End();
            bGameplayRestored = bObservationCompleted = true;
            Observation->Report->SetStringField(TEXT("outcome"), TEXT("GameplayRestored"));
            return End();
        }

    private:
        bool Check(bool bCondition, const FString& Message)
        {
            bChecksPassed = Test->TestTrue(*Message, bCondition) && bChecksPassed;
            return bCondition;
        }

        bool VerifyRestored(URunStateSubsystem* Run, ACombatManager* Combat, ACombatRoundCoordinator* Round)
        {
            const URunSaveGame& Expected = *Observation->Expected.Get();
            const FCombatCheckpointData& Checkpoint = Expected.CombatCheckpoint;
            if (!Check(!Run->IsManagedRun() && Run->GetPhase() == Expected.Phase && Run->GetLastResult() == Expected.Result && Run->GetCurrentNodeId() == Expected.CurrentNode && Run->GetCurrentEncounterId() == Expected.CurrentEncounter && Run->GetCompletedNodes() == Expected.CompletedNodes, TEXT("The actual Continue restores the original phase, result and route without starting a new Run."))) return false;
            if (!Check(FRunIdentityData::StaticStruct()->CompareScriptStruct(&Run->GetRunIdentity(), &Expected.Identity, 0) && FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Run->GetCombatCheckpoint(), &Checkpoint, 0), TEXT("Production Continue retains the migrated clone's complete owning identity and saved planning checkpoint."))) return false;
            if (!Check(FRunItemShopState::StaticStruct()->CompareScriptStruct(&Run->GetItemShopState(), &Expected.ItemShopState, 0) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Run->GetSkillShopState(), &Expected.SkillShopState, 0), TEXT("Continue preserves the frozen item catalog and officially migrated skill catalog rather than replacing them with current CSV data."))) return false;
            TArray<FRunPartyMember> ExpectedParty = Expected.Party;
            if (Expected.Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Expected.Identity.OriginalParticipants.Num() == 1)
            {
                int32 PlayerSlot = INDEX_NONE;
                FText Error;
                if (!Check(URunParticipationLibrary::ResolveStandalonePlayerSlot(ExpectedParty, PlayerSlot, Error), TEXT("The saved standalone selection resolves without changing ownership: ") + Error.ToString())) return false;
                for (FRunPartyMember& Member : ExpectedParty) Member.bPlayerControlled = Member.bCreated && Member.SlotIndex == PlayerSlot;
            }
            if (!Check(Run->GetPartyMembers().Num() == ExpectedParty.Num(), TEXT("Actual Continue retains all original party slots."))) return false;
            for (int32 Index = 0; Index < ExpectedParty.Num(); ++Index) if (!Check(FRunPartyMember::StaticStruct()->CompareScriptStruct(&Run->GetPartyMembers()[Index], &ExpectedParty[Index], 0), FString::Printf(TEXT("Restored party %d retains its identity, class, HP, gold, appearance, equipment and migrated skill loadout."), Index))) return false;
            const FCombatRoundView& View = Round->GetView();
            if (!Check(Combat->IsCombatActive() && View.RoundNumber == Checkpoint.RoundNumber && View.PlanRevision == Checkpoint.PlanRevision && View.Units.Num() == Checkpoint.Units.Num() && View.PendingEffects == 0 && View.PendingProjectiles == 0 && !Round->IsSAPMovementInProgress(), TEXT("The public restored round is planning with the exact saved revision and no fabricated action or SAP progress."))) return false;
            for (const FCombatCheckpointUnit& Saved : Checkpoint.Units)
            {
                const FCombatRoundUnitView* Entry = View.Units.FindByPredicate([&Saved](const FCombatRoundUnitView& Value) { return Value.UnitId == Saved.RoundUnitId; });
                const FCombatCheckpointRoundPlan* Plan = Checkpoint.RoundPlans.FindByPredicate([&Saved](const FCombatCheckpointRoundPlan& Value) { return Value.UnitId == Saved.RoundUnitId; });
                AUnitBase* Unit = Entry ? Entry->Unit.Get() : nullptr;
                if (!Check(Unit && Unit->GetAttributeSet() && Plan && FSoftObjectPath(Unit->GetClass()) == Saved.UnitClass && Unit->GetTeam() == Saved.Team && Unit->IsUnitAlive() != Saved.bDead && FMath::IsNearlyEqual(Unit->GetAttributeSet()->GetHP(), Saved.HP) && Unit->GetCurrentActionPoint() == Saved.AP && Unit->GetCurrentSubActionPoint() == Saved.SubAP, FString::Printf(TEXT("Saved unit %d restores its actual class, life state, HP, AP and SAP without a fixture override."), Saved.RoundUnitId))) return false;
                if (Saved.Team == ETeam::Player && !Check(Combat->GetCharacterId(Unit) == Saved.CharacterId && Combat->GetOwnerAccountId(Unit) == Saved.OwnerAccountId, TEXT("The live restored ally retains its original character and owner."))) return false;
                const ACombatGridTile* Tile = Unit->GetCurrentTile();
                if (!Check(Saved.bHasTile ? Tile && Tile->GridCoord == Saved.GridCoord && Tile->GetOccupyingUnit() == Unit : Tile == nullptr, TEXT("The saved unit restores its exact tile and grid occupancy."))) return false;
                FCombatRoundCommand ExpectedCommand = Plan->Command;
                ExpectedCommand.SkillId = UCombatCheckpointLibrary::ResolveSavedSkillId(ExpectedCommand.SkillId);
                if (!Check(FCombatRoundCommand::StaticStruct()->CompareScriptStruct(&Entry->Command, &ExpectedCommand, 0) && Entry->HomeCoord == Saved.GridCoord && Entry->bHasMovePlan == Plan->bHasMovePlan && Entry->MoveDestinationCoord == Plan->MoveDestinationCoord && Entry->bReady == (Saved.bDead || Entry->OwnerSlot == 0 || Plan->bReady), TEXT("The public restored view retains the saved command, move reservation and ready state."))) return false;
            }
            Observation->Report->SetNumberField(TEXT("restored_party_count"), ExpectedParty.Num());
            Observation->Report->SetNumberField(TEXT("restored_unit_count"), View.Units.Num());
            Observation->Report->SetNumberField(TEXT("restored_round_number"), View.RoundNumber);
            Observation->Report->SetNumberField(TEXT("restored_plan_revision"), View.PlanRevision);
            Observation->Report->SetBoolField(TEXT("public_restore_compared"), true);
            return true;
        }

        bool Capture(UWorld* World)
        {
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const FIntPoint Size = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
            TArray<FColor> Pixels;
            FIntVector Dimensions;
            if (!Check(Widget.IsValid() && Size.X > 0 && Size.Y > 0 && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Dimensions) && Dimensions.X == Size.X && Dimensions.Y == Size.Y && Pixels.Num() == Size.X * Size.Y, TEXT("A fresh complete Slate screenshot contains the actually restored gameplay."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
            const FString Path = Observation->Root / TEXT("Saved/Automation/TodoReview") / (Observation->Slot + TEXT(".png"));
            if (!Check(!PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *Path), TEXT("The restored gameplay screenshot is written to Saved only."))) return false;
            Observation->Report->SetStringField(TEXT("gameplay_png"), Path);
            Observation->Report->SetStringField(TEXT("actual_capture_size"), Size.ToString());
            return true;
        }

        bool End()
        {
            UGameViewportClient* Viewport = GEditor->PlayWorld ? GEditor->PlayWorld->GetGameViewport() : nullptr;
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
            GEditor->RequestEndPlayMap();
            Stage = 99;
            Started = FPlatformTime::Seconds();
            return false;
        }

        // Retain the viewport through PIE removal and drain rendering before its last game-thread reference is released.
        // PIE 제거 중 뷰포트를 유지하고 마지막 게임 스레드 참조를 해제하기 전에 렌더링을 비웁니다.
        void ReleaseViewport()
        {
            check(IsInGameThread());
            if (!RetainedViewport.IsValid()) return;
            FlushRenderingCommands();
            RetainedViewport.Reset();
        }

        FAutomationTestBase* Test;
        TSharedRef<FObservation> Observation;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TSharedPtr<ISlateViewport> RetainedViewport;
        double Started = 0.0;
        int32 Stage = 0;
        int32 WarmFrames = 0;
        bool bChecksPassed = true;
        bool bObservationCompleted = false;
        bool bGameplayRestored = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExistingSaveContinueReviewTest, "ProjectA.TodoReview.ExistingSaveContinue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExistingSaveContinueReviewTest::RunTest(const FString& Parameters)
{
    using namespace ExistingSaveContinueReview;
    TSharedRef<FObservation> Observation = MakeShared<FObservation>();
    Observation->Root = AbsolutePath(FPaths::ProjectDir());
    const FString Prefix = TEXT("ProjectA_Automation_ExistingContinue_");
    FGuid Guid;
    int32 ExpectedBytes = 0;
    FString ExpectedSHA256;
    if (!TestTrue(TEXT("Only the ProjectA Editor's own project-local Saved directory may be used."), GIsEditor && GEditor && !GEditor->PlayWorld && FPaths::GetCleanFilename(FPaths::GetProjectFilePath()) == TEXT("ProjectA.uproject") && AbsolutePath(FPaths::ProjectSavedDir()).Equals(Observation->Root / TEXT("Saved"), ESearchCase::IgnoreCase))) return false;
    if (!TestTrue(TEXT("An explicit fresh UUID Continue clone, positive source size and SHA256 are required."), FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueSlot="), Observation->Slot) && Observation->Slot.StartsWith(Prefix) && Observation->Slot.Len() == Prefix.Len() + 32 && FGuid::ParseExact(Observation->Slot.Right(32), EGuidFormats::Digits, Guid) && Guid.IsValid() && Observation->Slot == Prefix + Guid.ToString(EGuidFormats::Digits).ToLower() && URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()) == Observation->Slot && FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueBytes="), ExpectedBytes) && ExpectedBytes > 0 && FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueSHA256="), ExpectedSHA256) && IsSHA256Literal(ExpectedSHA256))) return false;
    Observation->SourcePath = Observation->Root / TEXT("Saved/SaveGames/ProjectA_Run.sav");
    TArray<uint8> Clone;
    FString BaselineText;
    FString BaselineSHA256;
    TSharedPtr<FJsonObject> Baseline;
    if (!TestTrue(TEXT("The original and disposable UUID clone exactly match the externally authorized original baseline."), FFileHelper::LoadFileToArray(Observation->OriginalBytes, *Observation->SourcePath) && Observation->OriginalBytes.Num() == ExpectedBytes && FFileHelper::LoadFileToArray(Clone, *(Observation->Root / TEXT("Saved/SaveGames") / (Observation->Slot + TEXT(".sav")))) && Clone == Observation->OriginalBytes && FFileHelper::LoadFileToString(BaselineText, *(Observation->Root / TEXT("Saved/Automation/TodoReview/BeforeUserState.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BaselineText), Baseline) && Baseline.IsValid() && Baseline->TryGetStringField(TEXT("Saved/SaveGames/ProjectA_Run.sav"), BaselineSHA256) && IsSHA256Literal(BaselineSHA256) && BaselineSHA256.Equals(ExpectedSHA256, ESearchCase::IgnoreCase))) return false;
    Observation->Raw.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Observation->Slot, 0)));
    FText Error;
    Observation->Expected.Reset(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Observation->Slot, Error)));
    if (!TestTrue(TEXT("The isolated clone raw load is an ordinary saved Combat Run, without acquiring any managed authority."), Observation->Raw.IsValid() && !FRunSaveFormat::IsManaged(Observation->Raw->Version) && Observation->Raw->Phase == ERunPhase::Combat)) return false;
    Observation->Report->SetStringField(TEXT("scope"), TEXT("Original-byte UUID clone only: actual main-menu Continue delegate, production in-memory migration and public Combat restoration. Original-slot Unreal load/save, managed authority mutation, normal gameplay actions and cooked compatibility are excluded. External driver checks all original files with SHA256; C++ checks source bytes/SHA1."));
    Observation->Report->SetStringField(TEXT("clone_slot"), Observation->Slot);
    Observation->Report->SetStringField(TEXT("external_baseline_sha256"), BaselineSHA256);
    Observation->Report->SetStringField(TEXT("cpp_original_sha1_before"), FSHA1::HashBuffer(Observation->OriginalBytes.GetData(), Observation->OriginalBytes.Num()).ToString());
    Observation->Report->SetNumberField(TEXT("raw_version"), Observation->Raw->Version);
    Observation->Report->SetNumberField(TEXT("raw_checkpoint_schema"), Observation->Raw->CombatCheckpoint.SchemaVersion);
    Observation->Report->SetNumberField(TEXT("raw_party_count"), Observation->Raw->Party.Num());
    Observation->Report->SetNumberField(TEXT("raw_skill_catalog_count"), Observation->Raw->SkillShopState.Catalog.Num());
    Observation->Report->SetNumberField(TEXT("raw_item_catalog_count"), Observation->Raw->ItemShopState.Catalog.Num());
    Observation->Report->SetBoolField(TEXT("official_migration_preload_succeeded"), Observation->Expected.IsValid());
    Observation->Report->SetStringField(TEXT("official_migration_preload_error"), Error.ToString());
    if (Observation->Expected.IsValid()) Observation->Report->SetNumberField(TEXT("migrated_skill_catalog_count"), Observation->Expected->SkillShopState.Catalog.Num());
    Observation->Report->SetStringField(TEXT("raw_identity_origin"), UEnum::GetValueAsString(Observation->Raw->Identity.Origin));
    Observation->Report->SetBoolField(TEXT("original_slot_loaded_or_saved_by_unreal"), false);
    Observation->Report->SetBoolField(TEXT("settings_or_managed_authority_mutated_by_fixture"), false);
    Observation->Report->SetBoolField(TEXT("actual_continue_delegate_invoked"), false);
    Observation->Report->SetBoolField(TEXT("public_restore_compared"), false);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReview>(this, Observation));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
