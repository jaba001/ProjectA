#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/CombatManager.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
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
#include "Misc/ScopeExit.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "TodoReviewWindowPlacement.h"
#include "TodoReviewGameplayPresentation.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
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
        FPaths::CollapseRelativeDirectories(Result);
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
        FString BaselinePath;
        FString OutputDirectory;
        TArray<uint8> OriginalBytes;
        TStrongObjectPtr<URunSaveGame> Raw;
        TStrongObjectPtr<URunSaveGame> Expected;
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        bool bOwnsClone = false;
    };

    bool CleanupClone(FAutomationTestBase* Test, const TSharedRef<FObservation>& Observation)
    {
        if (!Observation->bOwnsClone) return true;
        const bool bRemoved = !UGameplayStatics::DoesSaveGameExist(Observation->Slot, 0) || UGameplayStatics::DeleteGameInSlot(Observation->Slot, 0);
        const bool bCleaned = bRemoved && !UGameplayStatics::DoesSaveGameExist(Observation->Slot, 0);
        Observation->Report->SetBoolField(TEXT("owned_clone_cleaned"), bCleaned);
        Test->TestTrue(TEXT("Only the explicitly validated disposable UUID clone is removed after the observation."), bCleaned);
        if (bCleaned) Observation->bOwnsClone = false;
        return bCleaned;
    }

    template <typename T>
    T* ActiveScreen(AGameplayPlayerController* Controller)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller->GetWorld(), Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (T* Screen = Cast<T>(Widget); Screen && Screen->IsActivated() && Screen->GetOwningPlayer() == Controller) return Screen;
        return nullptr;
    }

    bool IsVerifiedPolicyRefusal(const URunSaveGame* Save, const FText& Error)
    {
        if (!Save || Error.IsEmpty()) return false;
        // These reasons are emitted only after production data validation succeeds; generic corruption is not accepted.
        // 이 사유들은 프로덕션 데이터 검증 성공 후에만 반환되며 일반 손상 오류는 정상 거부로 인정하지 않습니다.
        if (FRunSaveFormat::IsManaged(Save->Version)) return Error.EqualTo(NSLOCTEXT("RunCheckpoint", "ManagedResumeRequired", "관리 Run은 기준 저장소에서 실행 lease를 획득하는 명시적 재개를 사용해야 합니다."));
        if (Save->Phase == ERunPhase::Defeat || Save->Phase == ERunPhase::Complete) return Error.ToString() == TEXT("종료된 진행입니다. 새 게임을 시작해 주세요.");
        if (Save->Identity.Origin != ERunIdentityOrigin::LegacyOffline && (Save->Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Save->Identity.OriginalParticipants.Num() != 1)) return Error.EqualTo(NSLOCTEXT("RunCheckpoint", "SessionRequired", "계정 연결 또는 협동 세션이 필요한 저장입니다. 현재 싱글플레이 이어하기 대신 기존 Host와 원래 참가자가 연결된 세션에서 복원해야 합니다."));
        return false;
    }

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
                bool bPIEAlive = false;
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.WorldType == EWorldType::PIE) bPIEAlive = true;
                }
                if (bPIEAlive)
                {
                    if (FPlatformTime::Seconds() - Started < 30.0) return false;
                    Check(false, TEXT("The isolated Continue PIE closes before its report is finalized."));
                    // Close the owned session before deleting its clone so a live GameInstance cannot recreate it.
                    // 살아 있는 GameInstance가 사본을 다시 만들지 못하도록 사본 삭제 전에 소유한 세션을 닫습니다.
                    GEditor->EndPlayMap();
                    return false;
                }
                ReleaseViewport();
                bChecksPassed = CleanupClone(Test, Observation) && bChecksPassed;
                TArray<uint8> OriginalAfter;
                const bool bUnchanged = FFileHelper::LoadFileToArray(OriginalAfter, *Observation->SourcePath) && OriginalAfter == Observation->OriginalBytes;
                Check(bUnchanged, TEXT("The original source bytes remain unchanged after clone-only production Continue."));
                Observation->Report->SetBoolField(TEXT("original_bytes_unchanged"), bUnchanged);
                if (bUnchanged) Observation->Report->SetStringField(TEXT("cpp_original_sha1_after"), FSHA1::HashBuffer(OriginalAfter.GetData(), OriginalAfter.Num()).ToString());
                Observation->Report->SetBoolField(TEXT("passed"), bChecksPassed && (bGameplayRestored || bPolicyRefusalVerified));
                Observation->Report->SetBoolField(TEXT("gameplay_restored"), bGameplayRestored);
                Observation->Report->SetBoolField(TEXT("policy_refusal_verified"), bPolicyRefusalVerified);
                Observation->Report->SetBoolField(TEXT("observation_completed"), bObservationCompleted);
                FString Json;
                Check(FJsonSerializer::Serialize(Observation->Report, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Observation->OutputDirectory / (Observation->Slot + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM), TEXT("The isolated Continue result is written to the explicit current review directory in Saved only."));
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
                if (!TodoReviewWindowPlacement::Configure(Test, Settings.Get())) return End();
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
                if (!Check(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu"), TEXT("The actual Continue button belongs to the relocated Core/MainMenu World."))) return End();
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
                    Test->AddInfo(FString::Printf(TEXT("Original clone menu eligibility: eligible=%d buttonEnabled=%d reason=%s visibleStatus=%s."), bEligible, Continue->GetIsEnabled(), *EligibilityError.ToString(), SaveStatus ? *SaveStatus->GetText().ToString() : TEXT("missing")));
                    if (!bEligible)
                    {
                        const bool bRefusalVisible = Check(!Continue->GetIsEnabled() && !EligibilityError.IsEmpty() && SaveStatus && SaveStatus->GetText().ToString() == EligibilityError.ToString(), TEXT("A refused original-save clone remains disabled and exposes its actual eligibility reason in the menu."));
                        bPolicyRefusalVerified = bRefusalVisible && IsVerifiedPolicyRefusal(Observation->Expected.Get(), EligibilityError);
                        Observation->Report->SetStringField(TEXT("outcome"), bPolicyRefusalVerified ? TEXT("ContinuePolicyRefusalVerified") : TEXT("EligibilityRefused"));
                        bObservationCompleted = true;
                        if (bPolicyRefusalVerified) Test->AddInfo(TEXT("Correct menu refusal observed; gameplay was not restored and Continue was not invoked: ") + EligibilityError.ToString());
                        else Check(false, TEXT("Original-save clone Continue was refused by production data validation or an unverified policy: ") + EligibilityError.ToString());
                        Capture(World, true);
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
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            if (!Controller) return false;
            if (!bPublicStateCompared)
            {
                if (!VerifyCommonState(Run, World)) return End();
                bPublicStateCompared = true;
            }
            if (PresentationStarted == 0.0) PresentationStarted = FPlatformTime::Seconds();
            Observation->Report->SetObjectField(TEXT("presentation_readiness"), PresentationReport);
            if (Observation->Expected->Phase != ERunPhase::Combat)
            {
                if (!PollNonCombatPresentation(Run, Controller))
                {
                    if (!bChecksPassed) return End();
                    if (FPlatformTime::Seconds() - PresentationStarted < 30.0) return false;
                    Check(false, TEXT("The restored saved phase did not produce its matching visible UI and completed camera transition within thirty seconds; see presentation_readiness."));
                    Capture(World, true);
                    Observation->Report->SetStringField(TEXT("outcome"), TEXT("GameplayDataRestoredPresentationNotReady"));
                    bObservationCompleted = true;
                    return End();
                }
                if (!VerifyCommonState(Run, World) || !Capture(World)) return End();
                bGameplayRestored = bObservationCompleted = true;
                Observation->Report->SetStringField(TEXT("outcome"), TEXT("GameplayRestored"));
                return End();
            }
            ACombatManager* Combat = nullptr;
            for (TActorIterator<ACombatManager> It(World); It; ++It)
            {
                if (!It->GetRoundCoordinator()) continue;
                Combat = *It;
                break;
            }
            ACombatRoundCoordinator* Round = Combat ? Combat->GetRoundCoordinator() : nullptr;
            if (!Round || Round->GetView().Phase != ECombatRoundPhase::Planning) return false;
            if (!VerifyCombatRestored(Combat, Round)) return End();
            if (!Presentation.Poll(World, Round, PresentationReport))
            {
                if (FPlatformTime::Seconds() - PresentationStarted < 30.0) return false;
                Check(false, TEXT("Natural Continue frames did not produce the authored camera POV, visible unit meshes and rendered grid within thirty seconds; see presentation_readiness."));
                Capture(World, true);
                Observation->Report->SetStringField(TEXT("outcome"), TEXT("GameplayDataRestoredPresentationNotReady"));
                bObservationCompleted = true;
                return End();
            }
            if (!VerifyCommonState(Run, World) || !Capture(World)) return End();
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

        bool VerifyCommonState(URunStateSubsystem* Run, UWorld* World)
        {
            if (!Check(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TEXT("/Game/User_JeHoon/LEVEL/Core/Gameplay"), TEXT("The authored Continue arrives in the relocated Core/Gameplay World."))) return false;
            const URunSaveGame& Expected = *Observation->Expected.Get();
            const FCombatCheckpointData& Checkpoint = Expected.CombatCheckpoint;
            Observation->Report->SetStringField(TEXT("restored_phase"), UEnum::GetValueAsString(Run->GetPhase()));
            Observation->Report->SetBoolField(TEXT("restored_managed"), Run->IsManagedRun());
            if (!Check(!Run->IsManagedRun() && Run->GetPhase() == Expected.Phase && Run->GetLastResult() == Expected.Result && Run->GetCurrentNodeId() == Expected.CurrentNode && Run->GetCurrentEncounterId() == Expected.CurrentEncounter && Run->GetCompletedNodes() == Expected.CompletedNodes, TEXT("The actual Continue restores the original phase, result and route without starting a new Run."))) return false;
            if (!Check(FRunIdentityData::StaticStruct()->CompareScriptStruct(&Run->GetRunIdentity(), &Expected.Identity, 0) && FRunParticipationData::StaticStruct()->CompareScriptStruct(&Run->GetParticipation(), &Expected.Participation, 0) && FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Run->GetCombatCheckpoint(), &Checkpoint, 0), TEXT("Production Continue retains the migrated clone's complete owning identity, participation and checkpoint data."))) return false;
            if (!Check(FRunItemShopState::StaticStruct()->CompareScriptStruct(&Run->GetItemShopState(), &Expected.ItemShopState, 0) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Run->GetSkillShopState(), &Expected.SkillShopState, 0), TEXT("Continue preserves the frozen item catalog and officially migrated skill catalog rather than replacing them with current CSV data."))) return false;
            if (!Check(FSoftObjectPath(Run->PartyDefinition.Get()) == Expected.Catalog, TEXT("Continue retains the saved party-definition catalog reference."))) return false;
            if (!Check(FRunTargetState::StaticStruct()->CompareScriptStruct(&Run->GetTargetRunState(), &Expected.TargetRun, 0) && FRunEncounterProgress::StaticStruct()->CompareScriptStruct(&Run->GetEncounterProgress(), &Expected.EncounterProgress, 0) && FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Run->GetGoldRewardState(), &Expected.GoldRewardState, 0) && FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Run->GetWeaponSkillRules(), &Expected.WeaponSkillRules, 0) && Run->UsesWeaponSkills() == (Expected.WeaponSkillAcquisitionVersion == 1), TEXT("Continue preserves the frozen Target route, encounter selection, reward claims and weapon-skill policy."))) return false;
            if (!Check(Run->GetNodes().Num() == Expected.Nodes.Num(), TEXT("Continue retains the entire saved route node count."))) return false;
            for (int32 Index = 0; Index < Expected.Nodes.Num(); ++Index) if (!Check(FRunNodeDefinition::StaticStruct()->CompareScriptStruct(&Run->GetNodes()[Index], &Expected.Nodes[Index], 0), FString::Printf(TEXT("Saved route node %d retains its full definition."), Index))) return false;
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
            Observation->Report->SetNumberField(TEXT("restored_party_count"), ExpectedParty.Num());
            Observation->Report->SetBoolField(TEXT("public_restore_compared"), true);
            return true;
        }

        bool VerifyCombatRestored(ACombatManager* Combat, ACombatRoundCoordinator* Round)
        {
            const FCombatCheckpointData& Checkpoint = Observation->Expected->CombatCheckpoint;
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
            Observation->Report->SetNumberField(TEXT("restored_unit_count"), View.Units.Num());
            Observation->Report->SetNumberField(TEXT("restored_round_number"), View.RoundNumber);
            Observation->Report->SetNumberField(TEXT("restored_plan_revision"), View.PlanRevision);
            Observation->Report->SetBoolField(TEXT("combat_checkpoint_units_compared"), true);
            return true;
        }

        bool PollNonCombatPresentation(URunStateSubsystem* Run, AGameplayPlayerController* Controller)
        {
            const ERunPhase Phase = Observation->Expected->Phase;
            UUserWidget* Screen = nullptr;
            bool bCameraReady = Controller->PlayerCameraManager && !Controller->PlayerCameraManager->PendingViewTarget.Target;
            bool bContentReady = false;
            if (Phase == ERunPhase::Shop || Phase == ERunPhase::EncounterChoice)
            {
                Screen = ActiveScreen<URunEncounterWidget>(Controller);
                if (Phase == ERunPhase::Shop)
                {
                    const FRunEncounterOffer* Offer = Run->GetEncounterProgress().FindSelectedOffer();
                    const AEncounterPrototypeStage* NPC = Cast<AEncounterPrototypeStage>(Controller->GetViewTarget());
                    bContentReady = Offer && NPC && NPC->MatchesOffer(*Offer);
                    UTextBlock* Title = Screen ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_EncounterTitle"))) : nullptr;
                    FText ExpectedTitle = Offer ? Offer->GetDisplayName() : FText::GetEmpty();
                    if (Run->IsTargetRun()) ExpectedTitle = FText::Format(NSLOCTEXT("RunEncounter", "TargetProgressTitle", "{0} · {1}/80 완료"), ExpectedTitle, FText::AsNumber(Run->GetCompletedNodes().Num() + Run->GetTargetRunState().CompletedEncounterChoices.Num()));
                    bContentReady &= Title && Title->GetText().ToString() == ExpectedTitle.ToString();
                    PresentationReport->SetStringField(TEXT("selected_encounter"), Offer ? Offer->EncounterId.ToString() : TEXT("missing"));
                    PresentationReport->SetStringField(TEXT("npc_stage"), NPC ? NPC->StageId.ToString() : TEXT("missing"));
                    PresentationReport->SetStringField(TEXT("visible_shop_title"), Title ? Title->GetText().ToString() : TEXT("missing"));
                }
                else bContentReady = !Run->GetEncounterProgress().Offers.IsEmpty() && Controller->GetViewTarget() && Controller->GetViewTarget()->ActorHasTag(TEXT("GameplayEncounterOverview"));
            }
            else if (Phase == ERunPhase::Map)
            {
                Screen = ActiveScreen<URunMapWidget>(Controller);
                bContentReady = Controller->GetViewTarget() && Controller->GetViewTarget()->ActorHasTag(TEXT("GameplayEncounterOverview"));
            }
            else if (Phase == ERunPhase::Result)
            {
                Screen = ActiveScreen<UEncounterResultWidget>(Controller);
                UTextBlock* Title = Screen ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_Result"))) : nullptr;
                UButton* Continue = Screen ? Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_Continue"))) : nullptr;
                bContentReady = Title && !Title->GetText().IsEmpty() && Continue && Continue->GetIsEnabled() == (Run->CanContinueAfterRewards() && Controller->CanIssueRunCommands());
                PresentationReport->SetStringField(TEXT("visible_result"), Title ? Title->GetText().ToString() : TEXT("missing"));
            }
            else
            {
                Observation->Report->SetStringField(TEXT("outcome"), TEXT("UnsupportedEligiblePhase"));
                return Check(false, TEXT("Production eligibility unexpectedly admitted a saved phase without a stable continuation screen: ") + UEnum::GetValueAsString(Phase));
            }
            bool bCombatInactive = true;
            for (TActorIterator<ACombatManager> It(Controller->GetWorld()); It; ++It) bCombatInactive &= !It->IsCombatActive();
            PresentationReport->SetStringField(TEXT("saved_phase"), UEnum::GetValueAsString(Phase));
            PresentationReport->SetBoolField(TEXT("screen_active"), Screen != nullptr);
            PresentationReport->SetBoolField(TEXT("screen_enabled"), Screen && Screen->GetIsEnabled());
            PresentationReport->SetBoolField(TEXT("camera_transition_completed"), bCameraReady);
            PresentationReport->SetBoolField(TEXT("saved_phase_content_ready"), bContentReady);
            PresentationReport->SetBoolField(TEXT("combat_inactive"), bCombatInactive);
            return Screen && Screen->GetIsEnabled() && bCameraReady && bContentReady && bCombatInactive && ++PresentationFrames >= 12 && FPlatformTime::Seconds() - PresentationStarted >= 1.1 && !IsAsyncLoading();
        }

        bool Capture(UWorld* World, bool bDiagnostic = false)
        {
            if (!TodoReviewWindowPlacement::Ensure(Test, World)) return false;
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const FIntPoint Size = Viewport && Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
            TArray<FColor> Pixels;
            FIntVector Dimensions = FIntVector::ZeroValue;
            if (!Check(Widget.IsValid() && Size.X > 0 && Size.Y > 0 && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Dimensions) && Dimensions.X == Size.X && Dimensions.Y == Size.Y && Pixels.Num() == Size.X * Size.Y, TEXT("A fresh complete Slate screenshot contains the actual Continue observation screen."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
            const bool bMenuRefusal = Cast<AMainMenuPlayerController>(World->GetFirstPlayerController()) != nullptr;
            const FString Path = Observation->OutputDirectory / (Observation->Slot + (bMenuRefusal ? TEXT("_MenuRefusal.png") : bDiagnostic ? TEXT("_PresentationNotReady.png") : TEXT(".png")));
            if (!Check(!PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *Path), TEXT("The restored gameplay screenshot is written to Saved only."))) return false;
            Observation->Report->SetStringField(bMenuRefusal ? TEXT("menu_refusal_png") : bDiagnostic ? TEXT("diagnostic_png") : TEXT("gameplay_png"), Path);
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
        TodoReviewGameplayPresentation::FReadiness Presentation;
        TSharedRef<FJsonObject> PresentationReport = MakeShared<FJsonObject>();
        double Started = 0.0;
        double PresentationStarted = 0.0;
        int32 Stage = 0;
        int32 PresentationFrames = 0;
        bool bPublicStateCompared = false;
        bool bChecksPassed = true;
        bool bObservationCompleted = false;
        bool bGameplayRestored = false;
        bool bPolicyRefusalVerified = false;
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
    if (!TestTrue(TEXT("An explicit current user-state baseline is required; historical baselines are not assumed."), FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueBaseline="), Observation->BaselinePath))) return false;
    Observation->BaselinePath = AbsolutePath(Observation->BaselinePath);
    if (!TodoReviewWindowPlacement::OutputRoot(this, Observation->OutputDirectory)) return false;
    if (!TestTrue(TEXT("The current review output directory can be created inside Saved."), IFileManager::Get().MakeDirectory(*Observation->OutputDirectory, true))) return false;
    if (!TestTrue(TEXT("The baseline must be an existing JSON inside this project's Saved/Automation directory."), Observation->BaselinePath.StartsWith(AbsolutePath(Observation->Root / TEXT("Saved/Automation")) + TEXT("/"), ESearchCase::IgnoreCase) && FPaths::GetExtension(Observation->BaselinePath).Equals(TEXT("json"), ESearchCase::IgnoreCase) && FPaths::FileExists(Observation->BaselinePath))) return false;
    if (!TestTrue(TEXT("An explicit fresh UUID Continue clone, positive source size and SHA256 are required."), FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueSlot="), Observation->Slot) && Observation->Slot.StartsWith(Prefix) && Observation->Slot.Len() == Prefix.Len() + 32 && FGuid::ParseExact(Observation->Slot.Right(32), EGuidFormats::Digits, Guid) && Guid.IsValid() && Observation->Slot == Prefix + Guid.ToString(EGuidFormats::Digits).ToLower() && URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get()) == Observation->Slot && FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueBytes="), ExpectedBytes) && ExpectedBytes > 0 && FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveContinueSHA256="), ExpectedSHA256) && IsSHA256Literal(ExpectedSHA256))) return false;
    Observation->bOwnsClone = true;
    bool bReviewEnqueued = false;
    ON_SCOPE_EXIT
    {
        if (!bReviewEnqueued) CleanupClone(this, Observation);
    };
    Observation->SourcePath = Observation->Root / TEXT("Saved/SaveGames/ProjectA_Run.sav");
    TArray<uint8> Clone;
    FString BaselineText;
    FString BaselineSHA256;
    TSharedPtr<FJsonObject> Baseline;
    if (!TestTrue(TEXT("The original and disposable UUID clone exactly match the externally authorized original baseline."), FFileHelper::LoadFileToArray(Observation->OriginalBytes, *Observation->SourcePath) && Observation->OriginalBytes.Num() == ExpectedBytes && FFileHelper::LoadFileToArray(Clone, *(Observation->Root / TEXT("Saved/SaveGames") / (Observation->Slot + TEXT(".sav")))) && Clone == Observation->OriginalBytes && FFileHelper::LoadFileToString(BaselineText, *Observation->BaselinePath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BaselineText), Baseline) && Baseline.IsValid() && Baseline->TryGetStringField(TEXT("Saved/SaveGames/ProjectA_Run.sav"), BaselineSHA256) && IsSHA256Literal(BaselineSHA256) && BaselineSHA256.Equals(ExpectedSHA256, ESearchCase::IgnoreCase))) return false;
    if (!TestTrue(TEXT("The fresh clone observation never replaces an earlier report or screenshot."), !FPaths::FileExists(Observation->OutputDirectory / (Observation->Slot + TEXT(".json"))) && !FPaths::FileExists(Observation->OutputDirectory / (Observation->Slot + TEXT(".png"))))) return false;
    Observation->Raw.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Observation->Slot, 0)));
    FText Error;
    Observation->Expected.Reset(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Observation->Slot, Error)));
    Observation->Report->SetStringField(TEXT("scope"), TEXT("Original-byte UUID clone only: actual main-menu Continue delegate, unmodified production eligibility and in-memory migration, common saved-phase/party/identity/route/stock/catalog/policy restoration, plus phase-specific rendered UI. Combat alone additionally compares its planning checkpoint and actual units; Shop requires its saved offer's matching NPC and ready panel. Original-slot Unreal load/save, eligibility repair, managed authority mutation, gameplay actions and cooked compatibility are excluded. The validated UUID clone is deleted after PIE teardown or failed preflight. External driver checks original files with SHA256; C++ checks source bytes/SHA1."));
    Observation->Report->SetStringField(TEXT("clone_slot"), Observation->Slot);
    Observation->Report->SetStringField(TEXT("current_baseline_path"), Observation->BaselinePath);
    Observation->Report->SetStringField(TEXT("external_baseline_sha256"), BaselineSHA256);
    Observation->Report->SetStringField(TEXT("cpp_original_sha1_before"), FSHA1::HashBuffer(Observation->OriginalBytes.GetData(), Observation->OriginalBytes.Num()).ToString());
    Observation->Report->SetBoolField(TEXT("raw_load_succeeded"), Observation->Raw.IsValid());
    if (Observation->Raw.IsValid())
    {
        Observation->Report->SetStringField(TEXT("raw_phase"), UEnum::GetValueAsString(Observation->Raw->Phase));
        Observation->Report->SetBoolField(TEXT("raw_managed"), FRunSaveFormat::IsManaged(Observation->Raw->Version));
        Observation->Report->SetNumberField(TEXT("raw_version"), Observation->Raw->Version);
        Observation->Report->SetNumberField(TEXT("raw_checkpoint_schema"), Observation->Raw->CombatCheckpoint.SchemaVersion);
        Observation->Report->SetNumberField(TEXT("raw_party_count"), Observation->Raw->Party.Num());
        Observation->Report->SetNumberField(TEXT("raw_skill_catalog_count"), Observation->Raw->SkillShopState.Catalog.Num());
        Observation->Report->SetNumberField(TEXT("raw_item_catalog_count"), Observation->Raw->ItemShopState.Catalog.Num());
        Observation->Report->SetStringField(TEXT("raw_identity_origin"), UEnum::GetValueAsString(Observation->Raw->Identity.Origin));
        Observation->Report->SetStringField(TEXT("raw_party_catalog"), Observation->Raw->Catalog.ToString());
    }
    Observation->Report->SetBoolField(TEXT("official_migration_preload_succeeded"), Observation->Expected.IsValid());
    Observation->Report->SetStringField(TEXT("official_migration_preload_error"), Error.ToString());
    if (Observation->Expected.IsValid())
    {
        Observation->Report->SetStringField(TEXT("migrated_phase"), UEnum::GetValueAsString(Observation->Expected->Phase));
        Observation->Report->SetBoolField(TEXT("migrated_managed"), FRunSaveFormat::IsManaged(Observation->Expected->Version));
        Observation->Report->SetNumberField(TEXT("migrated_skill_catalog_count"), Observation->Expected->SkillShopState.Catalog.Num());
    }
    Observation->Report->SetBoolField(TEXT("original_slot_loaded_or_saved_by_unreal"), false);
    Observation->Report->SetBoolField(TEXT("settings_or_managed_authority_mutated_by_fixture"), false);
    Observation->Report->SetBoolField(TEXT("actual_continue_delegate_invoked"), false);
    Observation->Report->SetBoolField(TEXT("public_restore_compared"), false);
    Observation->Report->SetBoolField(TEXT("gameplay_restored"), false);
    Observation->Report->SetBoolField(TEXT("policy_refusal_verified"), false);
    FString Diagnostics;
    FJsonSerializer::Serialize(Observation->Report, TJsonWriterFactory<>::Create(&Diagnostics));
    AddInfo(TEXT("Original-save clone preflight diagnostics (before production eligibility): ") + Diagnostics);
    if (!TestTrue(TEXT("Raw load and migration diagnostics are saved before any menu eligibility or phase-specific failure."), FFileHelper::SaveStringToFile(Diagnostics, *(Observation->OutputDirectory / (Observation->Slot + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReview>(this, Observation));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    bReviewEnqueued = true;
    return true;
}

#endif
