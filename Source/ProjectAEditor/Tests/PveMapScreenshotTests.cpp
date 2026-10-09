#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/Presentation/CombatShoulderCamera.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Button.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunStateSubsystem.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "RunEncounterPIEHelpers.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationEditorCommon.h"
#include "TodoReviewGameplayPresentation.h"
#include "TodoReviewWindowPlacement.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/GameModeSelectionWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace PveMapScreenshots
{
    template <typename T>
    T* Screen(UWorld* World)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, T::StaticClass(), false);
        for (UUserWidget* Widget : Widgets) if (Widget->GetOwningPlayer() == World->GetFirstPlayerController() && Cast<T>(Widget)->IsActivated()) return Cast<T>(Widget);
        return nullptr;
    }

    // Enter the current normal Run through its menu; only public choices, plans and Ready requests advance it.
    // 현재 정상 Run에 메뉴로 진입하고 공개 선택·계획·준비 요청만으로 진행합니다.
    class FReview : public IAutomationLatentCommand
    {
    public:
        FReview(FAutomationTestBase* InTest, FString InDifficulty, FString InSlot, FString InOutput) : Test(InTest), Difficulty(MoveTemp(InDifficulty)), Slot(MoveTemp(InSlot)), Output(MoveTemp(InOutput)) {}

        virtual ~FReview() override
        {
            ReleaseViewport();
        }

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Started == 0.0) Started = StageStarted = Now;
            if (Stage == 99)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.WorldType != EWorldType::PIE) continue;
                    if (Now - StageStarted < 30.0) return false;
                    Check(false, TEXT("The screenshot PIE session closes within thirty seconds."));
                    FinishReport();
                    return true;
                }
                ReleaseViewport();
                if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) Check(UGameplayStatics::DeleteGameInSlot(Slot, 0), TEXT("Only the screenshot run's previously absent owned save slot is removed."));
                FinishReport();
                return true;
            }
            if (Now - Started > 240.0 || Now - StageStarted > 90.0)
            {
                Check(false, FString::Printf(TEXT("Normal PvE screenshot %s timed out at stage %d without camera or simulation repair."), *Difficulty, Stage));
                return End();
            }
            if (Stage == 0) return Start();
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World || !World->GetFirstPlayerController()) return false;
            RetainViewport(World);
            if (Stage == 1)
            {
                UMainMenuScreenWidget* Menu = Screen<UMainMenuScreenWidget>(World);
                if (!Cast<AMainMenuPlayerController>(World->GetFirstPlayerController()) || !Menu) return false;
                if (!PrepareViewport(World) || !Warm(World, 30, 1.f)) return bPassed ? false : End();
                if (Difficulty == TEXT("Low") && !Capture(World, TEXT("MainMenu"))) return End();
                if (!Click(Menu, TEXT("Button_NewGame"))) return End();
                Advance(2);
                return false;
            }
            if (Stage == 2)
            {
                UGameModeSelectionWidget* Menu = Screen<UGameModeSelectionWidget>(World);
                if (!Menu) return false;
                if (!Click(Menu, TEXT("Button_SinglePlayer"))) return End();
                Advance(3);
                return false;
            }
            if (Stage == 3)
            {
                UCharacterCreationWidget* Creation = Screen<UCharacterCreationWidget>(World);
                if (!Creation) return false;
                for (int32 Index = 0; Index < 4; ++Index) if (!Click(Creation, FName(*FString::Printf(TEXT("Button_Slot%d_Create"), Index)))) return End();
                if (!Click(Creation, TEXT("Button_Slot0_PlayerControl"))) return End();
                const TArray<FRunPartyMember> Members = Creation->GetPartyMembers();
                if (!Check(Members.FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated; }).Num() == 4 && Members.FilterByPredicate([](const FRunPartyMember& Member) { return Member.bCreated && Member.bPlayerControlled; }).Num() == 1, TEXT("Normal creation retains four default characters with one directly controlled character."))) return End();
                if (!Click(Creation, TEXT("Button_StartGame"))) return End();
                Advance(4);
                return false;
            }
            Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            if (!Controller.IsValid() || !Run || Run->GetPhase() == ERunPhase::None) return false;
            if (!Check(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TEXT("/Game/User_JeHoon/LEVEL/Core/Gameplay") && Run->IsTargetRun() && !Run->IsManagedRun() && Run->IsCheckpointSavingEnabled(), TEXT("The real single-player target Run uses the saved unified Gameplay level and isolated checkpoint."))) return End();
            if (Stage == 4) return AdvanceToCombat(Run);
            Round = Controller->GetRoundCoordinator();
            if (Stage == 8) return ObserveAfterAction(World, Run);
            if (!Round.IsValid() || !Round->GetArena()) return false;
            if (!Check(Round->GetArena()->EnvironmentId == ExpectedArena(), TEXT("The actual arena retains the selected difficulty's saved environment ID."))) return End();
            if (Stage == 5)
            {
                if (Round->GetView().Phase != ECombatRoundPhase::Planning || !Controller->IsRoundInputEnabled()) return false;
                if (!PrepareViewport(World)) return bPassed ? false : End();
                if (!TacticalReadiness.Poll(World, Round.Get(), TacticalEvidence)) return false;
                OriginalTacticalCamera = Round->GetArena()->CameraAnchor;
                SelectedArena = Round->GetArena()->EnvironmentId;
                if (!Capture(World, TEXT("Tactical")) || !PlanAction()) return End();
                Advance(6);
                return false;
            }
            const FCombatRoundUnitView* Direct = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Entry) { return Entry.UnitId == DirectId; });
            if (!Check(Direct != nullptr, TEXT("The original directly controlled character remains in the actual round."))) return End();
            if (Stage == 6)
            {
                if (!Check(FCombatRoundCommand::StaticStruct()->CompareScriptStruct(&Direct->Command, &Command, 0), TEXT("The normal server accepts the submitted authored skill plan."))) return End();
                Controller->SetRoundReady(true);
                Advance(7);
                return false;
            }
            if (Stage == 7)
            {
                const ACombatShoulderCamera* Shoulder = Cast<ACombatShoulderCamera>(Controller->GetViewTarget());
                const APlayerCameraManager* Camera = Controller->PlayerCameraManager;
                if (Shoulder && Camera && !Camera->PendingViewTarget.Target && !CombatRoundRules::IsTerminal(Direct->ActionPhase))
                {
                    if (!Warm(World, 2, 0.05f)) return false;
                    if (!Capture(World, TEXT("Shoulder"))) return End();
                    Advance(8);
                    return false;
                }
                ResetWarm();
                if (CombatRoundRules::IsTerminal(Direct->ActionPhase) || (Round->GetView().RoundNumber != PlannedRound))
                {
                    Check(false, TEXT("The natural action finished before a stable shoulder frame was observed; no timing override is applied."));
                    return End();
                }
                return false;
            }
            return false;
        }

    private:
        // Capture the actual post-action state even if result cleanup removes the round or camera restoration fails.
        // 결과 정리로 라운드가 제거되거나 카메라 복원이 실패해도 실제 행동 후 상태를 캡처합니다.
        bool ObserveAfterAction(UWorld* World, URunStateSubsystem* Run)
        {
            const FCombatRoundUnitView* Direct = Round.IsValid() ? Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Entry) { return Entry.UnitId == DirectId; }) : nullptr;
            const ERunPhase RunPhase = Run->GetPhase();
            bObservedActionTerminal = Direct && CombatRoundRules::IsTerminal(Direct->ActionPhase);
            bObservedNextRound = Round.IsValid() && Round->GetView().RoundNumber != PlannedRound;
            bObservedCombatResult = RunPhase == ERunPhase::Result || RunPhase == ERunPhase::Complete || RunPhase == ERunPhase::Defeat;
            const APlayerCameraManager* Camera = Controller->PlayerCameraManager;
            const bool bSettledNonShoulder = Camera && !Camera->PendingViewTarget.Target && !Cast<ACombatShoulderCamera>(Controller->GetViewTarget());
            const bool bActionBoundary = bObservedActionTerminal || bObservedNextRound || bObservedCombatResult || bSettledNonShoulder;
            if (!bActionBoundary)
            {
                ResetWarm();
                if (FPlatformTime::Seconds() - StageStarted < 30.0) return false;
                bAfterActionTimeout = true;
                Test->AddWarning(TEXT("No post-action boundary was observed in thirty natural seconds; preserve an unmodified diagnostic screenshot without claiming tactical restoration."));
                if (!Capture(World, TEXT("AfterActionTimeout"))) return End();
                bCapturedSequence = true;
                return End();
            }
            if (!Warm(World, 30, 1.f)) return false;
            if (!Capture(World, TEXT("AfterAction"))) return End();
            bCapturedSequence = true;
            return End();
        }

        FName ExpectedArena() const
        {
            return Difficulty == TEXT("Low") ? FName(TEXT("MeadowBloom")) : Difficulty == TEXT("Medium") ? FName(TEXT("DungeonStone")) : FName(TEXT("IceCitadel"));
        }

        FGameplayTag DifficultyTag() const
        {
            return Difficulty == TEXT("Low") ? RunPveDifficulty::GetLowTag() : Difficulty == TEXT("Medium") ? RunPveDifficulty::GetMediumTag() : RunPveDifficulty::GetHighTag();
        }

        bool Check(bool bCondition, const FString& Message)
        {
            bPassed = Test->TestTrue(*Message, bCondition) && bPassed;
            return bCondition;
        }

        bool Click(UUserWidget* Widget, FName ButtonName)
        {
            UButton* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(ButtonName)) : nullptr;
            if (!Check(Button && Button->GetIsEnabled(), TEXT("The original menu button is available: ") + ButtonName.ToString())) return false;
            Button->OnClicked.Broadcast();
            return true;
        }

        bool Start()
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
            Advance(1);
            return false;
        }

        bool AdvanceToCombat(URunStateSubsystem* Run)
        {
            if (Run->GetPhase() == ERunPhase::EncounterChoice || Run->GetPhase() == ERunPhase::Shop)
            {
                URunEncounterWidget* Widget = RunEncounterPIE::FindScreen(Controller.Get());
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), Widget)) return false;
                if (Run->GetPhase() == ERunPhase::Shop) Controller->RequestLeaveRunEncounter();
                else
                {
                    const TArray<FRunEncounterOffer>& Offers = Run->GetEncounterProgress().Offers;
                    if (!Check(Offers.Num() > 0, TEXT("The actual encounter offers an existing branch before the first battle."))) return End();
                    Controller->RequestSelectRunEncounter(Offers[0].EncounterId);
                }
                StageStarted = FPlatformTime::Seconds();
                return false;
            }
            if (Run->GetPhase() == ERunPhase::Map)
            {
                if (!RunEncounterPIE::PresentationReady(Controller.Get(), RunEncounterPIE::FindActiveScreen<URunMapWidget>(Controller.Get()))) return false;
                const FRunNodeDefinition* Next = Run->GetNodes().FindByPredicate([Run](const FRunNodeDefinition& Node) { return Run->CanStartNode(Node.NodeId); });
                if (!Check(Next && Run->GetCompletedNodes().IsEmpty(), TEXT("Only the first naturally available PvE node is selected."))) return End();
                TArray<FRunPveDifficultyOffer> Offers;
                FText Error;
                if (!Check(RunPveDifficulty::BuildOffers(Run->GetTargetRunState(), 0, Offers, Error), TEXT("The saved first combat exposes the actual three difficulty offers: ") + Error.ToString())) return End();
                const FRunPveDifficultyOffer* Selected = Offers.FindByPredicate([this](const FRunPveDifficultyOffer& Offer) { return Offer.DifficultyTag == DifficultyTag(); });
                if (!Check(Selected && Selected->ArenaId == ExpectedArena(), TEXT("The actual selected card maps to the expected authored arena."))) return End();
                Controller->RequestStartNode(Next->NodeId, DifficultyTag());
                return false;
            }
            if (Run->GetPhase() == ERunPhase::Combat) Advance(5);
            return false;
        }

        bool PlanAction()
        {
            const FCombatRoundView& View = Round->GetView();
            const int32 SlotNumber = Controller->GetRoundParticipantSlot();
            const FCombatRoundUnitView* Direct = View.Units.FindByPredicate([SlotNumber](const FCombatRoundUnitView& Entry) { return SlotNumber > 0 && !Entry.bEnemy && Entry.OwnerSlot == SlotNumber && Entry.Unit && Entry.Unit->IsUnitAlive(); });
            if (!Check(Direct != nullptr, TEXT("A positive participant slot owns the living directly controlled character."))) return false;
            DirectId = Direct->UnitId;
            PlannedRound = View.RoundNumber;
            for (FName SkillId : Direct->SkillIds)
            {
                const FCombatRoundSkill* Skill = Round->FindSkill(SkillId);
                if (!Skill || Skill->Kind == ECombatRoundSkillKind::Wait) continue;
                for (const FCombatRoundUnitView& Enemy : View.Units)
                {
                    if (!Enemy.bEnemy || !Enemy.Unit || !Enemy.Unit->IsUnitAlive()) continue;
                    FCombatRoundCommand Candidate;
                    Candidate.UnitId = DirectId;
                    Candidate.SkillId = SkillId;
                    Candidate.TargetUnitId = CombatRoundRules::UsesUnitTarget(*Skill) ? Enemy.UnitId : INDEX_NONE;
                    Candidate.TargetCoord = Enemy.HomeCoord;
                    Candidate.DestinationCoord = Direct->HomeCoord;
                    FText Error;
                    if (!Round->CanPlanCommand(Candidate, Error)) continue;
                    Command = Candidate;
                    Controller->SetRoundCameraUnit(DirectId);
                    Controller->SubmitRoundPlan(Command);
                    return true;
                }
            }
            return Check(false, TEXT("The unchanged default character has a legal authored attack for the natural shoulder sequence."));
        }

        bool PrepareViewport(UWorld* World)
        {
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
            if (!Check(Viewport && Viewport->Viewport && Window.IsValid(), TEXT("The actual PIE gameplay viewport owns a Slate window."))) return false;
            const FIntPoint Size = Viewport->Viewport->GetSizeXY();
            if (Size == FIntPoint(1280, 720)) return TodoReviewWindowPlacement::Ensure(Test, World);
            if (LastResizeFrame == GFrameCounter || (LastResizeFrame > 0 && GFrameCounter - LastResizeFrame < 10)) return false;
            if (!Check(++ResizeAttempts <= 6 && Size.X > 0 && Size.Y > 0, TEXT("Bounded Slate border correction converges to a true 1280x720 viewport."))) return false;
            // Correct measured window borders only; do not alter the camera or crop the rendered evidence.
            // 측정한 창 테두리만 보정하며 카메라를 바꾸거나 렌더링 증거를 잘라내지 않습니다.
            Window->Resize(Window->GetClientSizeInScreen() + FVector2D(1280 - Size.X, 720 - Size.Y));
            LastResizeFrame = GFrameCounter;
            ResetWarm();
            return false;
        }

        bool Warm(UWorld* World, int32 Frames, float Seconds)
        {
            if (WarmFrame != GFrameCounter)
            {
                WarmFrame = GFrameCounter;
                if (WarmFrames++ == 0) WarmWorldTime = World->GetTimeSeconds();
            }
            return WarmFrames >= Frames && World->GetTimeSeconds() - WarmWorldTime >= Seconds;
        }

        void ResetWarm()
        {
            WarmFrame = MAX_uint64;
            WarmFrames = 0;
            WarmWorldTime = 0.f;
        }

        void Advance(int32 Next)
        {
            Stage = Next;
            StageStarted = FPlatformTime::Seconds();
            ResetWarm();
        }

        bool Capture(UWorld* World, const FString& Name)
        {
            if (!TodoReviewWindowPlacement::Ensure(Test, World)) return false;
            UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            TArray<FColor> Pixels;
            FIntVector Size = FIntVector::ZeroValue;
            if (!Check(Widget.IsValid() && FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size) && Size.X == 1280 && Size.Y == 720 && Pixels.Num() == Size.X * Size.Y, TEXT("The actual complete gameplay Slate viewport produces a fresh 1280x720 screenshot with HUD."))) return false;
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
            const FString Filename = Output / (Name + TEXT(".png"));
            if (!Check(!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *Filename), TEXT("Fresh screenshots are written only under the isolated review directory."))) return false;
            TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
            Record->SetStringField(TEXT("filename"), Filename);
            Record->SetStringField(TEXT("capture"), Name);
            Record->SetStringField(TEXT("map"), UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
            Record->SetStringField(TEXT("difficulty"), Difficulty);
            APlayerController* Player = World->GetFirstPlayerController();
            Record->SetStringField(TEXT("view_target"), GetPathNameSafe(Player->GetViewTarget()));
            Record->SetStringField(TEXT("view_target_class"), GetPathNameSafe(Player->GetViewTarget() ? Player->GetViewTarget()->GetClass() : nullptr));
            Record->SetStringField(TEXT("camera_location"), Player->PlayerCameraManager ? Player->PlayerCameraManager->GetCameraLocation().ToString() : TEXT("none"));
            Record->SetStringField(TEXT("camera_rotation"), Player->PlayerCameraManager ? Player->PlayerCameraManager->GetCameraRotation().ToString() : TEXT("none"));
            Record->SetNumberField(TEXT("world_seconds"), World->GetTimeSeconds());
            Record->SetNumberField(TEXT("width"), Size.X);
            Record->SetNumberField(TEXT("height"), Size.Y);
            const URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            const APlayerCameraManager* Camera = Player->PlayerCameraManager;
            const UCameraComponent* TacticalCamera = OriginalTacticalCamera.IsValid() ? OriginalTacticalCamera->FindComponentByClass<UCameraComponent>() : nullptr;
            const bool bMatchesTacticalTarget = OriginalTacticalCamera.IsValid() && Player->GetViewTarget() == OriginalTacticalCamera.Get();
            const bool bMatchesTacticalPOV = bMatchesTacticalTarget && Camera && TacticalCamera && !Camera->PendingViewTarget.Target && Camera->GetCameraLocation().Equals(TacticalCamera->GetComponentLocation(), 1.f) && Camera->GetCameraRotation().Equals(TacticalCamera->GetComponentRotation(), 0.1f);
            Record->SetStringField(TEXT("selected_arena_id"), SelectedArena.ToString());
            Record->SetStringField(TEXT("original_tactical_view_target"), GetPathNameSafe(OriginalTacticalCamera.Get()));
            Record->SetBoolField(TEXT("matches_original_tactical_target"), bMatchesTacticalTarget);
            Record->SetBoolField(TEXT("actual_tactical_pov_restored"), bMatchesTacticalPOV);
            Record->SetBoolField(TEXT("camera_blend_pending"), Camera && Camera->PendingViewTarget.Target);
            Record->SetBoolField(TEXT("round_available"), Round.IsValid());
            Record->SetBoolField(TEXT("observed_action_terminal"), bObservedActionTerminal);
            Record->SetBoolField(TEXT("observed_next_round"), bObservedNextRound);
            Record->SetBoolField(TEXT("observed_combat_result"), bObservedCombatResult);
            Record->SetBoolField(TEXT("after_action_observation_timeout"), bAfterActionTimeout);
            if (Run) Record->SetNumberField(TEXT("run_phase"), static_cast<int32>(Run->GetPhase()));
            if (Name == TEXT("AfterAction") || Name == TEXT("AfterActionTimeout")) bAfterActionTacticalRestored = bMatchesTacticalPOV;
            if (Round.IsValid())
            {
                if (Round->GetArena()) Record->SetStringField(TEXT("arena_id"), Round->GetArena()->EnvironmentId.ToString());
                Record->SetNumberField(TEXT("round_number"), Round->GetView().RoundNumber);
                Record->SetNumberField(TEXT("round_phase"), static_cast<int32>(Round->GetView().Phase));
                const FCombatRoundUnitView* Direct = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Entry) { return Entry.UnitId == DirectId; });
                if (Direct)
                {
                    Record->SetNumberField(TEXT("direct_unit_id"), Direct->UnitId);
                    Record->SetNumberField(TEXT("direct_action_phase"), static_cast<int32>(Direct->ActionPhase));
                    Record->SetBoolField(TEXT("direct_ready"), Direct->bReady);
                    Record->SetStringField(TEXT("direct_command_skill"), Direct->Command.SkillId.ToString());
                }
            }
            Captures.Add(MakeShared<FJsonValueObject>(Record));
            Test->AddInfo(TEXT("PvE map screenshot: ") + Filename);
            return true;
        }

        void RetainViewport(UWorld* World)
        {
            if (RetainedViewport.IsValid()) return;
            UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
            const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
        }

        void ReleaseViewport()
        {
            if (!RetainedViewport.IsValid()) return;
            FlushRenderingCommands();
            RetainedViewport.Reset();
        }

        bool End()
        {
            GEditor->RequestEndPlayMap();
            Advance(99);
            return false;
        }

        void FinishReport()
        {
            TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
            Report->SetBoolField(TEXT("passed_capture_contract"), bPassed && bCapturedSequence);
            Report->SetBoolField(TEXT("after_action_tactical_pov_restored"), bAfterActionTacticalRestored);
            Report->SetBoolField(TEXT("after_action_observation_timeout"), bAfterActionTimeout);
            Report->SetStringField(TEXT("difficulty"), Difficulty);
            Report->SetStringField(TEXT("arena_id"), ExpectedArena().ToString());
            Report->SetStringField(TEXT("isolated_save_slot"), Slot);
            Report->SetStringField(TEXT("scope"), TEXT("Actual normal menu creation, four default characters with one direct player, public encounter/difficulty/skill/Ready requests, original unified Gameplay map and production cameras. Natural tactical, Ready shoulder and observed post-action frames; successful capture does not imply correct shoulder framing or tactical restoration. AfterAction records the state after a terminal action, new round, combat result or settled non-shoulder target followed by thirty natural frames and one world second; a thirty-second missing boundary produces a labelled AfterActionTimeout diagnostic. No GameMode, health, damage, skill timing, actor transform, asset, camera or simulation overrides. UI button delegates and public controller requests are automation, not physical mouse input. Screenshots support later visual judgement and do not establish balance, multiplayer, Continue or all camera edge cases."));
            Report->SetObjectField(TEXT("tactical_render_readiness"), TacticalEvidence);
            Report->SetArrayField(TEXT("captures"), Captures);
            FString Json;
            FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
            Check(FFileHelper::SaveStringToFile(Json, *(Output / TEXT("Manifest.json"))), TEXT("The screenshot manifest preserves actual map, camera, phase and fixture scope."));
        }

        FAutomationTestBase* Test;
        FString Difficulty;
        FString Slot;
        FString Output;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TWeakObjectPtr<AGameplayPlayerController> Controller;
        TWeakObjectPtr<ACombatRoundCoordinator> Round;
        TWeakObjectPtr<AActor> OriginalTacticalCamera;
        TSharedPtr<ISlateViewport> RetainedViewport;
        TodoReviewGameplayPresentation::FReadiness TacticalReadiness;
        TSharedRef<FJsonObject> TacticalEvidence = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Captures;
        FCombatRoundCommand Command;
        FName SelectedArena;
        int32 Stage = 0;
        int32 DirectId = INDEX_NONE;
        int32 PlannedRound = 0;
        int32 ResizeAttempts = 0;
        int32 WarmFrames = 0;
        uint64 LastResizeFrame = 0;
        uint64 WarmFrame = MAX_uint64;
        float WarmWorldTime = 0.f;
        double Started = 0.0;
        double StageStarted = 0.0;
        bool bPassed = true;
        bool bCapturedSequence = false;
        bool bObservedActionTerminal = false;
        bool bObservedNextRound = false;
        bool bObservedCombatResult = false;
        bool bAfterActionTimeout = false;
        bool bAfterActionTacticalRestored = false;
    };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPveMapScreenshotReview, "ProjectA.TodoReview.PveMapScreenshots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPveMapScreenshotReview::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    for (const FString& Difficulty : {FString(TEXT("Low")), FString(TEXT("Medium")), FString(TEXT("High"))})
    {
        OutBeautifiedNames.Add(Difficulty);
        OutTestCommands.Add(Difficulty);
    }
}

bool FPveMapScreenshotReview::RunTest(const FString& Parameters)
{
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_MapScreens_");
    FGuid Guid;
    if (!TestTrue(TEXT("Map screenshot review requires explicit opt-in, actual left-monitor rendering, normal target mode and no current PIE."), GEditor && GEngine && !GEditor->PlayWorld && FApp::CanEverRender() && FSlateApplication::IsInitialized() && FParse::Param(FCommandLine::Get(), TEXT("ProjectAMapScreenshotReview")) && FParse::Param(FCommandLine::Get(), TEXT("ProjectAReviewLeftMonitor")) && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && !FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen")) && !FParse::Param(FCommandLine::Get(), TEXT("ProjectAPrototypeRun")))) return false;
    if (!TestTrue(TEXT("Only the three named PvE screenshot cases are allowed."), Parameters == TEXT("Low") || Parameters == TEXT("Medium") || Parameters == TEXT("High"))) return false;
    if (!TestTrue(TEXT("A previously absent owned UUID save slot protects every existing user Run."), Slot.StartsWith(Prefix) && Slot.Len() == Prefix.Len() + 32 && FGuid::ParseExact(Slot.Right(32), EGuidFormats::Digits, Guid) && Guid.IsValid() && !UGameplayStatics::DoesSaveGameExist(Slot, 0))) return false;
    FString OutputRoot;
    if (!TodoReviewWindowPlacement::OutputRoot(this, OutputRoot)) return false;
    const FString Output = OutputRoot / TEXT("Pve/Render") / FGuid::NewGuid().ToString(EGuidFormats::Digits) / Parameters;
    if (!TestTrue(TEXT("Screenshots use a fresh isolated output directory under Saved/Automation."), !IFileManager::Get().DirectoryExists(*Output) && IFileManager::Get().MakeDirectory(*Output, true))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<PveMapScreenshots::FReview>(this, Parameters, Slot, Output));
    return true;
}

#endif
