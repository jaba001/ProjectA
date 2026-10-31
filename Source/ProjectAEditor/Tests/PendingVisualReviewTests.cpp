#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Combat/Presentation/CombatShoulderCamera.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/CombatDebugPlayerController.h"
#include "Controller/GameplayPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Run/RunArmorCatalog.h"
#include "Game/Run/RunStateSubsystem.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "PlayInEditorDataTypes.h"
#include "RenderingThread.h"
#include "Tests/AutomationEditorCommon.h"
#include "TodoReviewWindowPlacement.h"
#include "Unit/CharacterAppearanceComponent.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace PendingVisualReview
{
    // Record actual frame intervals without taking screenshots or changing the simulation.
    // 화면 캡처나 시뮬레이션 변경 없이 실제 프레임 간격을 기록합니다.
    class FFrameTrace
    {
    public:
        explicit FFrameTrace(FString InPath) : Path(MoveTemp(InPath))
        {
            Buffer = TEXT("elapsed_seconds,frame_ms,run_phase,encounter_transition,shoulder_camera,round_phase,round_number\n");
            Handle = FCoreDelegates::OnEndFrame.AddRaw(this, &FFrameTrace::Tick);
        }

        ~FFrameTrace()
        {
            FCoreDelegates::OnEndFrame.Remove(Handle);
            Flush();
        }

    private:
        void Tick()
        {
            const double Now = FPlatformTime::Seconds();
            if (LastTime == 0.0) FirstTime = Now;
            const double Interval = LastTime == 0.0 ? 0.0 : (Now - LastTime) * 1000.0;
            LastTime = Now;
            if (!GEngine) return;
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) World = Context.World();
            }
            if (!World || !World->GetGameInstance()) return;
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            const ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
            Buffer += FString::Printf(TEXT("%.6f,%.3f,%d,%d,%d,%d,%d\n"), Now - FirstTime, Interval,
                Run ? static_cast<int32>(Run->GetPhase()) : -1, Controller && Controller->IsEncounterPresentationTransitioning(),
                Controller && Cast<ACombatShoulderCamera>(Controller->GetViewTarget()) != nullptr,
                Round ? static_cast<int32>(Round->GetView().Phase) : -1, Round ? Round->GetView().RoundNumber : -1);
            if (Buffer.Len() > 32768 || Now - LastFlush > 1.0)
            {
                Flush();
                LastFlush = Now;
            }
        }

        void Flush()
        {
            if (Buffer.IsEmpty()) return;
            FFileHelper::SaveStringToFile(Buffer, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
            Buffer.Reset();
        }

        FString Path;
        FString Buffer;
        FDelegateHandle Handle;
        double FirstTime = 0.0;
        double LastTime = 0.0;
        double LastFlush = 0.0;
    };

    TUniquePtr<FFrameTrace> FrameTrace;

    void StartFrameTrace()
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectAPendingFrameTrace"))) return;
        FString Root;
        FParse::Value(FCommandLine::Get(), TEXT("ProjectAReviewOutputRoot="), Root);
        Root = FPaths::ConvertRelativePathToFull(Root);
        const FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/"));
        if (!Root.StartsWith(Allowed, ESearchCase::IgnoreCase)) return;
        IFileManager::Get().MakeDirectory(*Root, true);
        FrameTrace = MakeUnique<FFrameTrace>(Root / (TEXT("FrameTrace_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".csv")));
    }

    struct FFrameTraceRegistration
    {
        FFrameTraceRegistration()
        {
            FCoreDelegates::GetOnPostEngineInit().AddStatic(&StartFrameTrace);
            FCoreDelegates::OnEnginePreExit.AddLambda([]() { FrameTrace.Reset(); });
        }
    };

    FFrameTraceRegistration FrameTraceRegistration;

    // Use disposable debug actors and original garments; captures remain evidence for visual inspection.
    // 일회성 디버그 액터와 원본 의상을 사용하며 캡처는 화면 검수 근거로 남깁니다.
    class FWardrobe : public IAutomationLatentCommand
    {
    public:
        FWardrobe(FAutomationTestBase* InTest, FString InOutput) : Test(InTest), Output(MoveTemp(InOutput)) {}

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (Started == 0.0) Started = Changed = Now;
            if (Stage == 99)
            {
                if (GEditor && GEditor->PlayWorld && Now - Changed < 30.0) return false;
                Test->TestTrue(TEXT("The isolated wardrobe PIE session closes."), !GEditor || !GEditor->PlayWorld);
                return true;
            }
            if (Now - Started > 240.0)
            {
                Test->AddError(FString::Printf(TEXT("Wardrobe review timed out at stage %d, garment %d, view %d."), Stage, ItemIndex, ViewIndex));
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
                Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat");
                GEditor->RequestPlaySession(Params);
                return Advance(1);
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World) return false;
            Controller = Cast<ACombatDebugPlayerController>(World->GetFirstPlayerController());
            ACombatDebugGameMode* Mode = World->GetAuthGameMode<ACombatDebugGameMode>();
            ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
            if (!Controller || !Mode || !Round) return false;
            if (!TodoReviewWindowPlacement::Ensure(Test, World)) return End();
            if (Stage == 1)
            {
                if (Round->GetView().Phase != ECombatRoundPhase::Planning) return false;
                FText Error;
                if (!Test->TestTrue(TEXT("The original six armor assets resolve without copying or modifying them."), RunArmorCatalog::Load(Items, Error) && Items.Num() == 6)) return End();
                for (int32 Index = 0; Index < 2; ++Index)
                {
                    int32 AddedId = INDEX_NONE;
                    const bool bSpawned = Mode->SpawnDebugUnit(Controller, false, TEXT("Warrior"), AddedId, Error);
                    if (!Test->TestTrue(TEXT("The public debug tool adds the alternating male/female comparison characters."), bSpawned)) return End();
                }
                for (const FCombatRoundUnitView& Entry : Round->GetView().Units)
                {
                    if (Entry.bEnemy || !Entry.Unit || !Entry.Unit->CharacterAppearance) continue;
                    if (!Subjects.ContainsByPredicate([&Entry](const TWeakObjectPtr<AUnitBase>& Existing)
                    {
                        return Existing.IsValid() && Existing->CharacterAppearance->Selection.BodyId == Entry.Unit->CharacterAppearance->Selection.BodyId;
                    })) Subjects.Add(Entry.Unit);
                }
                if (!Test->TestEqual(TEXT("Both original body variants are present."), Subjects.Num(), 2)) return End();
                Observer = World->SpawnActor<ACameraActor>();
                Observer->GetCameraComponent()->FieldOfView = 45.f;
                TacticalTarget = Controller->GetViewTarget();
                return Advance(2);
            }
            if (Stage == 2)
            {
                FText Error;
                const FRunItemDefinition& Item = Items[ItemIndex];
                const FString Name = Item.Asset.GetAssetName();
                const TCHAR* Slot = Name.Contains(TEXT("Gloves")) ? TEXT("Equipment.Slot.Hands") : Name.Contains(TEXT("Cloak")) ? TEXT("Equipment.Slot.Accessory") : TEXT("Equipment.Slot.Body");
                for (const TWeakObjectPtr<AUnitBase>& Subject : Subjects)
                {
                    const bool bEquipped = Controller->GetDebugLoadout()->GrantEquipment(Controller, Subject->UnitIndex, Item.Asset, FGameplayTag::RequestGameplayTag(Slot), Error);
                    if (!Test->TestTrue(TEXT("The real debug equipment request accepts the original garment."), bEquipped))
                    {
                        Test->AddError(Error.ToString());
                        return End();
                    }
                }
                ViewIndex = 0;
                return PositionCamera();
            }
            if (Stage == 3)
            {
                if (Now - Changed < 0.8) return false;
                const FString CaptureName = FString::Printf(TEXT("%02d_%s_%s_%s"), ItemIndex + 1, *Items[ItemIndex].Asset.GetAssetName(),
                    *Subjects[ViewIndex / 2]->CharacterAppearance->Selection.BodyId.ToString(), ViewIndex % 2 ? TEXT("Back") : TEXT("Front"));
                if (!VerifyPoses() || !Capture(World, CaptureName)) return End();
                if (++ViewIndex < 4) return PositionCamera();
                if (++ItemIndex < Items.Num()) return Advance(2);
                for (const FCombatRoundUnitView& Entry : Round->GetView().Units) if (Entry.Unit) Entry.Unit->SetActorHiddenInGame(false);
                if (!CollisionProbe(World)) return End();
                Controller->SetViewTarget(TacticalTarget.Get());
                Controller->SetRoundCameraUnit(Subjects[0]->UnitIndex);
                if (!PlanAttack(Round, Subjects[0].Get())) return End();
                if (!PlanAttack(Round, Subjects[1].Get())) return End();
                InitialRound = Round->GetView().RoundNumber;
                Controller->SetRoundReady(true);
                return Advance(4);
            }
            if (Stage == 4)
            {
                const FCombatRoundUnitView* Subject = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Entry) { return Entry.Unit == Subjects[0].Get(); });
                if (!Subject) return End();
                const bool bShoulder = Cast<ACombatShoulderCamera>(Controller->GetViewTarget()) != nullptr;
                if (bShoulder && !bCapturedShoulder)
                {
                    bCapturedShoulder = Capture(World, TEXT("Wardrobe_NaturalShoulderAction"));
                }
                const FString Phase = FString::FromInt(static_cast<int32>(Subject->ActionPhase));
                if (!SeenPhases.Contains(Phase))
                {
                    SeenPhases.Add(Phase);
                    Test->AddInfo(FString::Printf(TEXT("Natural wardrobe action phase=%s; shoulder=%d; elapsed=%.3f."), *Phase, bShoulder, Now - Changed));
                    if (!VerifyPoses() || !Capture(World, TEXT("Wardrobe_ActionPhase_") + Phase)) return End();
                }
                if (Round->GetView().Phase == ECombatRoundPhase::Resolving || Round->GetView().RoundNumber == InitialRound) return false;
                Test->TestTrue(TEXT("The real action enters its shoulder camera."), bCapturedShoulder);
                for (const TWeakObjectPtr<AUnitBase>& Unit : Subjects) Unit->Die();
                ViewIndex = 0;
                return Advance(5);
            }
            if (Stage == 5)
            {
                if (Now - Changed < 1.0) return false;
                if (!VerifyPoses()) return End();
                AUnitBase* Subject = Subjects[ViewIndex].Get();
                const FVector Center = Subject->GetMesh()->Bounds.Origin;
                const FVector Position = Center + FVector(-360.f, -260.f, 210.f);
                Observer->SetActorLocationAndRotation(Position, (Center - Position).Rotation());
                Controller->SetViewTarget(Observer);
                return Advance(6);
            }
            if (Stage == 6)
            {
                if (Now - Changed < 0.5) return false;
                if (!Capture(World, TEXT("Wardrobe_Ragdoll_") + Subjects[ViewIndex]->CharacterAppearance->Selection.BodyId.ToString())) return End();
                if (++ViewIndex < Subjects.Num()) return Advance(5);
                return End();
            }
            return false;
        }

    private:
        bool Advance(int32 InStage)
        {
            Stage = InStage;
            Changed = FPlatformTime::Seconds();
            return false;
        }

        bool PositionCamera()
        {
            AUnitBase* Subject = Subjects[ViewIndex / 2].Get();
            // Isolate the inspected body in the temporary close-up; restore every unit before combat.
            // 임시 근접 촬영에서 검수 몸체만 보이게 하며 전투 전에 모든 유닛 표시를 복원합니다.
            for (const FCombatRoundUnitView& Entry : Controller->GetRoundCoordinator()->GetView().Units) if (Entry.Unit) Entry.Unit->SetActorHiddenInGame(Entry.Unit != Subject);
            const FVector Center = Subject->GetActorLocation() + FVector(0.f, 0.f, -5.f);
            const float Sign = ViewIndex % 2 ? -1.f : 1.f;
            const FVector Position = Center + Subject->GetActorForwardVector() * (Sign * 390.f) + Subject->GetActorRightVector() * 85.f + FVector(0.f, 0.f, 55.f);
            Observer->SetActorLocationAndRotation(Position, (Center - Position).Rotation());
            Controller->SetViewTarget(Observer);
            return Advance(3);
        }

        bool VerifyPoses()
        {
            for (const TWeakObjectPtr<AUnitBase>& Subject : Subjects)
            {
                TInlineComponentArray<USkeletalMeshComponent*> Meshes(Subject.Get());
                int32 Garments = 0;
                for (USkeletalMeshComponent* Mesh : Meshes)
                {
                    if (!Mesh->GetSkeletalMeshAsset() || !Mesh->GetSkeletalMeshAsset()->GetPathName().StartsWith(TEXT("/Game/ROG_Modular_Armor/Accessories_Robes/"))) continue;
                    ++Garments;
                    const bool bFollowsBody = Mesh->IsRegistered() && Mesh->IsVisible()
                        && Mesh->LeaderPoseComponent.Get() == Subject->GetMesh() && Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
                    if (!Test->TestTrue(TEXT("A visible worn garment shares the current body's animated or ragdoll pose without collision."), bFollowsBody)) return false;
                    Test->AddInfo(FString::Printf(TEXT("Garment %s on %s: clothing=%d; leader=%s; ragdoll=%d."), *Mesh->GetSkeletalMeshAsset()->GetName(),
                        *Subject->CharacterAppearance->Selection.BodyId.ToString(), Mesh->GetSkeletalMeshAsset()->GetMeshClothingAssets().Num(),
                        *GetNameSafe(Mesh->LeaderPoseComponent.Get()), Subject->GetMesh()->IsSimulatingPhysics()));
                }
                if (!Test->TestTrue(TEXT("The expected worn garment is present on each body."), Garments > 0)) return false;
            }
            return true;
        }

        bool PlanAttack(ACombatRoundCoordinator* Round, AUnitBase* Unit)
        {
            const FCombatRoundUnitView* Entry = Round->GetView().Units.FindByPredicate([Unit](const FCombatRoundUnitView& Candidate) { return Candidate.Unit == Unit; });
            if (!Entry) return false;
            for (FName SkillId : Entry->SkillIds)
            {
                const FCombatRoundSkill* Skill = Round->FindSkill(SkillId);
                if (!Skill || Skill->Kind == ECombatRoundSkillKind::Wait) continue;
                for (const FCombatRoundUnitView& Enemy : Round->GetView().Units)
                {
                    if (!Enemy.bEnemy || Enemy.HP <= 0.f) continue;
                    FCombatRoundCommand Command;
                    Command.UnitId = Entry->UnitId;
                    Command.SkillId = SkillId;
                    Command.TargetUnitId = CombatRoundRules::UsesUnitTarget(*Skill) ? Enemy.UnitId : INDEX_NONE;
                    Command.TargetCoord = Enemy.HomeCoord;
                    Command.DestinationCoord = Entry->HomeCoord;
                    FText Error;
                    if (!Round->CanPlanCommand(Command, Error)) continue;
                    Controller->SubmitRoundPlan(Command);
                    return true;
                }
            }
            return Test->TestTrue(TEXT("The authored character has a legal natural attack."), false);
        }

        bool CollisionProbe(UWorld* World)
        {
            FActorSpawnParameters Spawn;
            Spawn.ObjectFlags |= RF_Transient;
            AUnitBase* Subject = Subjects[0].Get();
            ACombatShoulderCamera* Camera = World->SpawnActor<ACombatShoulderCamera>(Spawn);
            FMinimalViewInfo Template;
            Camera->FollowUnit(Subject, Template);
            FMinimalViewInfo Baseline;
            Camera->CalcCamera(0.f, Baseline);
            const FVector Pivot = Subject->GetActorLocation() + FVector(0.f, 0.f, Subject->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * Camera->PivotHeightFraction);
            AUnitBase* Other = Subjects[1].Get();
            const FTransform Original = Other->GetActorTransform();
            Other->SetActorLocation((Pivot + Baseline.Location) * 0.5f);
            FMinimalViewInfo Alongside;
            Camera->CalcCamera(0.f, Alongside);
            const bool bIgnoredUnit = Test->TestTrue(TEXT("A real nearby unit does not retract the actual shoulder-camera sweep."), Baseline.Location.Equals(Alongside.Location, 1.f));
            Other->SetActorTransform(Original);
            AActor* Wall = World->SpawnActor<AActor>(Spawn);
            UBoxComponent* Box = NewObject<UBoxComponent>(Wall, NAME_None, RF_Transient);
            Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(35.f));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionResponseToAllChannels(ECR_Ignore);
            Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
            Box->RegisterComponent();
            Wall->SetActorLocation((Pivot + Baseline.Location) * 0.5f);
            FMinimalViewInfo Obstructed;
            Camera->CalcCamera(0.f, Obstructed);
            const bool bBlockedWall = Test->TestTrue(TEXT("A real blocking wall retracts the actual shoulder-camera sweep before penetration."),
                FVector::Dist(Pivot, Obstructed.Location) < FVector::Dist(Pivot, Baseline.Location) * 0.8f);
            Wall->Destroy();
            Camera->Destroy();
            return bIgnoredUnit && bBlockedWall;
        }

        bool Capture(UWorld* World, const FString& Name)
        {
            FViewport* Viewport = World->GetGameViewport() ? World->GetGameViewport()->Viewport : nullptr;
            TArray<FColor> Pixels;
            if (!Test->TestTrue(TEXT("The live rendered wardrobe viewport is readable."), Viewport && Viewport->ReadPixels(Pixels))) return false;
            const FIntPoint Size = Viewport->GetSizeXY();
            const FString Path = Output / (Name + TEXT(".png"));
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
            return Test->TestTrue(TEXT("The wardrobe evidence is saved under Saved/Automation."), FFileHelper::SaveArrayToFile(Png, *Path));
        }

        bool End()
        {
            GEditor->RequestEndPlayMap();
            return Advance(99);
        }

        FAutomationTestBase* Test;
        FString Output;
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        TArray<FRunItemDefinition> Items;
        TArray<TWeakObjectPtr<AUnitBase>> Subjects;
        TSet<FString> SeenPhases;
        ACombatDebugPlayerController* Controller = nullptr;
        ACameraActor* Observer = nullptr;
        TWeakObjectPtr<AActor> TacticalTarget;
        double Started = 0.0;
        double Changed = 0.0;
        int32 Stage = 0;
        int32 ItemIndex = 0;
        int32 ViewIndex = 0;
        int32 InitialRound = 0;
        bool bCapturedShoulder = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPendingWardrobeReview, "ProjectA.TodoReview.PendingWardrobe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPendingWardrobeReview::RunTest(const FString& Parameters)
{
    const bool bAllowed = GEditor && !GEditor->PlayWorld && FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ProjectAReviewLeftMonitor"));
    if (!TestTrue(TEXT("Wardrobe review requires explicit left-monitor rendering and no active PIE."), bAllowed)) return false;
    FString Output;
    if (!TodoReviewWindowPlacement::OutputRoot(this, Output)) return false;
    Output /= TEXT("Wardrobe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if (!IFileManager::Get().MakeDirectory(*Output, true)) return false;
    AddInfo(TEXT("Wardrobe evidence directory: ") + Output);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<PendingVisualReview::FWardrobe>(this, Output));
    return true;
}

#endif
