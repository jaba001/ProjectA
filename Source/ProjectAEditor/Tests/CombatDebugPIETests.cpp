#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "GAS/Attribute/AS_Unit.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace CombatDebugPIE
{
// Observe particle simulation on real frames and save viewport captures for a separate visual review.
// 실제 프레임의 파티클 시뮬레이션을 관찰하고 화면 가시성은 별도로 확인하도록 뷰포트 캡처를 저장합니다.
class FAuthoredToolsAndVfx : public IAutomationLatentCommand
{
public:
    explicit FAuthoredToolsAndVfx(FAutomationTestBase* InTest) : Test(InTest)
    {
        OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/CombatDebugPIE") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*OutputDirectory, true);
        ViewportRenderedHandle = UGameViewportClient::OnViewportRendered().AddRaw(this, &FAuthoredToolsAndVfx::CaptureRenderedViewport);
    }

    virtual ~FAuthoredToolsAndVfx() override
    {
        UGameViewportClient::OnViewportRendered().Remove(ViewportRenderedHandle);
    }

    virtual bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (Stage == 9)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && FPlatformTime::Seconds() - Started < 30.0) return false;
                if (Context.WorldType == EWorldType::PIE) Test->AddError(TEXT("Debug PIE world did not close."));
            }
            return true;
        }
        if (FPlatformTime::Seconds() - Started > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Debug PIE timed out at stage %d, sample %d."), Stage, SampleIndex));
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
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/DebugCombat");
            GEditor->RequestPlaySession(Params);
            Advance(1);
            return false;
        }
        if (Stage == 1)
        {
            if (!Connect()) return false;
            TArray<UUserWidget*> Screens;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Screens, UCombatRoundPlanningWidget::StaticClass(), false);
            if (!Screens.ContainsByPredicate([this](UUserWidget* Screen) { return Screen->GetOwningPlayer() == Controller && Cast<UCombatRoundPlanningWidget>(Screen)->IsActivated(); })) return false;
            if (!PrepareRoster()) return End();
            RevivedUnit->Die();
            Advance(2);
            return false;
        }
        ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
        if (!Check(IsValid(Round) && IsValid(Mode), TEXT("The debug combat context remains available throughout verification."))) return End();
        if (Stage == 2)
        {
            FText Error;
            if (!Check(!RevivedUnit->IsUnitAlive() && RevivedUnit->GetMesh()->IsSimulatingPhysics(), TEXT("The authored character enters death ragdoll on a real frame."))) return End();
            if (!Check(Round->ReviveDebugUnit(Controller, RevivedUnit->UnitIndex, Error), FString::Printf(TEXT("The authored ally revives: %s"), *Error.ToString()))) return End();
            Advance(3);
            return false;
        }
        if (Stage == 3)
        {
            if (FPlatformTime::Seconds() - Started < 0.2) return false;
            const FRunPartyMember* Equipment = Controller->GetDebugLoadout()->GetEquipmentMember(RevivedUnit->UnitIndex);
            if (!Check(RevivedUnit->IsUnitAlive() && !RevivedUnit->GetMesh()->IsSimulatingPhysics() && RevivedUnit->GetMesh()->GetAnimInstance() && RevivedUnit->GetMesh()->GetAttachParent() == RevivedUnit->GetCapsuleComponent() && RevivedUnit->GetMesh()->GetRelativeTransform().Equals(AliveMeshTransform), TEXT("Revival restores animated mesh attachment and pose without ragdoll simulation."))) return End();
            if (!Check(RevivedUnit->GetAttributeSet()->GetHP() == RevivedUnit->GetAttributeSet()->GetMaxHP() && RevivedUnit->GetCurrentActionPoint() == RevivedUnit->GetMaxActionPoint() && RevivedUnit->GetCurrentSubActionPoint() == RevivedUnit->GetMaxSubActionPoint() && RevivedUnit->GetEquippedSkillDataAssets() == OriginalSkills && Equipment && FRunPartyMember::StaticStruct()->CompareScriptStruct(Equipment, &OriginalEquipment, 0), TEXT("Authored revival restores resources and retains equipped assets and debug equipment."))) return End();
            if (!Check(Mode->RestartCombat(), TEXT("Each VFX sample starts in a fresh debug combat."))) return End();
            Advance(4);
            return false;
        }
        if (Stage == 4)
        {
            if (Round->GetView().Phase != ECombatRoundPhase::Planning) return false;
            if (!PrepareSkill()) return End();
            Advance(5);
            return false;
        }
        if (Stage == 5)
        {
            ObserveVfx();
            if (bCaptureFailed) return End();
            if (Round->GetView().Phase == ECombatRoundPhase::Resolving) return false;
            if (Round->GetView().RoundNumber == SampleRound && Round->GetView().Phase != ECombatRoundPhase::Finished) return false;
            if (!PendingScreenshot.IsEmpty()) return false;
            Test->AddInfo(FString::Printf(TEXT("%s: active=%d, ready=%d, max particles=%d, max age=%.3f; GPU particle counts use the engine's delayed count/estimate."), *Samples[SampleIndex], bSawActive, bSawReady, MaxParticles, MaxAge));
            if (!Check(bSawActive && bSawReady && MaxParticles > 0 && MaxAge > 0.f, TEXT("The real authored skill produces active, ready, advancing particle simulation."))) return End();
            if (!Check(bFirstScreenshot && bMiddleScreenshot, TEXT("Viewport captures were requested on two frames with active particle simulation."))) return End();
            Advance(6);
            return false;
        }
        if (Stage == 6)
        {
            if (!PendingScreenshot.IsEmpty() || FScreenshotRequest::IsScreenshotRequested()) return false;
            if (!Check(!bCaptureFailed && bFirstCaptureCompleted && bMiddleCaptureCompleted && IFileManager::Get().FileSize(*FirstScreenshot) > 0 && IFileManager::Get().FileSize(*MiddleScreenshot) > 0, TEXT("Both captures completed on rendered PIE viewport frames and were written before restart or teardown."))) return End();
            Test->AddInfo(TEXT("VFX screenshots: ") + FirstScreenshot + TEXT(" ; ") + MiddleScreenshot + TEXT(". The _middle file is the second particle observation at least 0.1 seconds after the first; it does not identify the effect lifetime midpoint."));
            ++SampleIndex;
            if (SampleIndex == Samples.Num())
            {
                Check(CompletedCaptures == Samples.Num() * 2, TEXT("Every authored sample has two completed captures from its real PIE viewport."));
                return End();
            }
            if (!Check(Mode->RestartCombat(), TEXT("Restart the disposable combat for the next authored effect."))) return End();
            Advance(4);
        }
        return false;
    }

private:
    bool Connect()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || World->GetNetMode() != NM_Standalone) continue;
            Mode = World->GetAuthGameMode<ACombatDebugGameMode>();
            Controller = Cast<ACombatDebugPlayerController>(World->GetFirstPlayerController());
            if (Mode && Controller && Controller->GetRoundCoordinator() && Controller->GetRoundCoordinator()->GetView().Phase == ECombatRoundPhase::Planning) return true;
        }
        return false;
    }

    bool PrepareRoster()
    {
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const FCombatRoundUnitView* Existing = Round->GetView().Units.FindByPredicate([](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && IsValid(Unit.Unit); });
        if (!Check(Existing != nullptr, TEXT("The authored debug roster contains an allied character."))) return false;
        RevivedUnit = Existing->Unit;
        OriginalSkills = RevivedUnit->GetEquippedSkillDataAssets();
        AliveMeshTransform = RevivedUnit->GetMesh()->GetRelativeTransform();
        const FRunPartyMember* Equipment = Controller->GetDebugLoadout()->GetEquipmentMember(RevivedUnit->UnitIndex);
        if (!Check(Equipment != nullptr, TEXT("The authored original ally has a debug equipment record."))) return false;
        OriginalEquipment = *Equipment;
        const FGuid CombatId = Round->GetView().CombatId;
        const FGuid UnitId = Mode->GetCombatManager()->GetRuntimeUnitId(RevivedUnit);
        const int32 OriginalCount = Round->GetView().Units.Num();
        for (bool bEnemy : {false, true})
        {
            TArray<FName> Ids;
            TArray<FText> Names;
            Mode->GetDebugSpawnOptions(bEnemy, Ids, Names);
            if (!Check(!Ids.IsEmpty() && Ids.Num() == Names.Num(), TEXT("The authored spawn catalog resolves names and options."))) return false;
            int32 AddedId = INDEX_NONE;
            FText Error;
            if (!Check(Mode->SpawnDebugUnit(Controller, bEnemy, Ids[0], AddedId, Error), FString::Printf(TEXT("Spawn authored %s: %s"), bEnemy ? TEXT("enemy") : TEXT("profession"), *Error.ToString()))) return false;
            const FCombatRoundUnitView* Added = Round->GetView().Units.FindByPredicate([AddedId](const FCombatRoundUnitView& Unit) { return Unit.UnitId == AddedId; });
            if (!Check(Added && Added->bEnemy == bEnemy && Added->Unit && Added->Unit->GetMesh()->GetSkeletalMeshAsset() && Added->Unit->GetMesh()->GetAnimInstance(), TEXT("Added authored units have the requested team, skeletal mesh and animation instance."))) return false;
            if (!bEnemy && !Check(Controller->GetDebugLoadout()->GetEquipmentMember(AddedId) != nullptr, TEXT("A newly added ally receives an editable equipment record in the same combat."))) return false;
        }
        return Check(Round->GetView().Units.Num() == OriginalCount + 2 && Round->GetView().CombatId == CombatId && Mode->GetCombatManager()->GetRuntimeUnitId(RevivedUnit) == UnitId && RevivedUnit->GetEquippedSkillDataAssets() == OriginalSkills, TEXT("Authored spawning preserves the current combat, original character and loadout."));
    }

    bool PrepareSkill()
    {
        const FString Path = TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/") + Samples[SampleIndex] + TEXT(".") + Samples[SampleIndex];
        Skill.Reset(LoadObject<USkillDefinitionDataAsset>(nullptr, *Path));
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const FCombatRoundUnitView* Ally = Round->GetView().Units.FindByPredicate([](const FCombatRoundUnitView& Unit) { return !Unit.bEnemy && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive(); });
        const FCombatRoundUnitView* Enemy = Round->GetView().Units.FindByPredicate([](const FCombatRoundUnitView& Unit) { return Unit.bEnemy && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive(); });
        if (!Check(Skill.IsValid() && Ally && Enemy, TEXT("The representative authored skill and both teams are available."))) return false;
        FText Error;
        if (!Check(Skill->ResolveRoundSkill(Definition, Error) && Round->SetDebugUnitSkills(Controller, Ally->UnitId, {Skill.Get()}, Error), FString::Printf(TEXT("Equip authored skill %s: %s"), *Samples[SampleIndex], *Error.ToString()))) return false;
        FCombatRoundCommand Command;
        Command.UnitId = Ally->UnitId;
        Command.SkillId = Definition.SkillId;
        Command.TargetUnitId = Definition.TargetRule == ESkillTargetRule::EnemyTile ? INDEX_NONE : Enemy->UnitId;
        Command.TargetCoord = Enemy->HomeCoord;
        Command.DestinationCoord = Ally->HomeCoord;
        const FCombatRoundView& View = Round->GetView();
        SampleRound = View.RoundNumber;
        if (!Check(Round->SubmitPlan(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, Command, Error) && Round->SetParticipantReady(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, true, Error), FString::Printf(TEXT("Execute the authored command: %s"), *Error.ToString()))) return false;
        bSawActive = bSawReady = bFirstScreenshot = bMiddleScreenshot = false;
        bFirstCaptureCompleted = bMiddleCaptureCompleted = bCaptureFailed = false;
        MaxParticles = 0;
        MaxAge = FirstParticleAge = 0.f;
        FirstScreenshot = OutputDirectory / (Samples[SampleIndex] + TEXT("_first.png"));
        MiddleScreenshot = OutputDirectory / (Samples[SampleIndex] + TEXT("_middle.png"));
        return true;
    }

    void ObserveVfx()
    {
        int32 Particles = 0;
        float Age = 0.f;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* Component = *It;
            if (Component->GetWorld() != Controller->GetWorld() || !Component->GetAsset() || Component->GetAsset() != Definition.Vfx.Niagara.Get() || !Component->IsActive()) continue;
            bSawActive = true;
            bSawReady |= Component->GetAsset()->IsReadyToRun();
            const FNiagaraSystemInstanceControllerPtr Instance = Component->GetSystemInstanceController();
            if (!Instance || !Instance->IsValid()) continue;
            // Finish concurrent work before inspecting the emitter data; never advance simulation manually.
            // 이미 진행 중인 동시 작업을 마친 뒤 이미터 데이터를 읽고 시뮬레이션을 수동 진행하지 않습니다.
            Instance->WaitForConcurrentTickAndFinalize();
            Age = FMath::Max(Age, Instance->GetAge());
            for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetSystemInstance_Unsafe()->GetEmitters()) Particles += FMath::Max(Emitter->GetNumParticles(), 0);
        }
        for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
        {
            UParticleSystemComponent* Component = *It;
            if (Component->GetWorld() != Controller->GetWorld() || !Component->Template || Component->Template != Definition.Vfx.Cascade.Get() || !Component->IsActive()) continue;
            bSawActive = bSawReady = true;
            Particles += Component->GetNumActiveParticles();
            if (Particles > 0) Age = FMath::Max(Age, static_cast<float>(FPlatformTime::Seconds() - Started));
        }
        MaxParticles = FMath::Max(MaxParticles, Particles);
        MaxAge = FMath::Max(MaxAge, Age);
        if (Particles <= 0 || !PendingScreenshot.IsEmpty() || FScreenshotRequest::IsScreenshotRequested()) return;
        if (!bFirstScreenshot)
        {
            FirstParticleAge = Age;
            // Capture the rendered battlefield so the tool overlay cannot hide the effect under inspection.
            // 검사할 이펙트가 도구 패널에 가려지지 않도록 렌더링된 전장을 캡처합니다.
            bFirstScreenshot = QueueViewportCapture(FirstScreenshot, Age);
        }
        else if (!bMiddleScreenshot && Age - FirstParticleAge >= 0.1f)
        {
            bMiddleScreenshot = QueueViewportCapture(MiddleScreenshot, Age);
        }
    }

    bool QueueViewportCapture(const FString& Filename, float ParticleAge)
    {
        UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
        UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
        if (!Check(World && World->WorldType == EWorldType::PIE && IsValid(ViewportClient) && ViewportClient->GetWorld() == World && ViewportClient->Viewport, TEXT("Each capture request targets the live PIE game viewport.")))
        {
            bCaptureFailed = true;
            return false;
        }
        CaptureViewportClient = ViewportClient;
        ExpectedScreenshotSize = ViewportClient->Viewport->GetRenderTargetTextureSizeXY();
        PendingScreenshot = Filename;
        PendingParticleAge = ParticleAge;
        return true;
    }

    void CaptureRenderedViewport(FViewport* RenderedViewport)
    {
        if (PendingScreenshot.IsEmpty()) return;
        UGameViewportClient* ViewportClient = CaptureViewportClient.Get();
        if (!IsValid(ViewportClient) || RenderedViewport != ViewportClient->Viewport) return;
        UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
        const FIntPoint Size = RenderedViewport->GetRenderTargetTextureSizeXY();
        TArray<FColor> Pixels;
        // Read only the matching PIE render target after its draw event, excluding Slate UI and editor viewports.
        // Slate UI와 에디터 뷰포트를 제외하고 그리기 이벤트가 끝난 해당 PIE 렌더 타깃만 읽습니다.
        const bool bPixelsValid = World && World->WorldType == EWorldType::PIE && ViewportClient->GetWorld() == World && Size == ExpectedScreenshotSize && Size.X > 0 && Size.Y > 0 && GetViewportScreenShot(RenderedViewport, Pixels) && Pixels.Num() == int64(Size.X) * Size.Y;
        if (Check(bPixelsValid, TEXT("The capture event supplied the expected live PIE viewport and complete pixels at its requested resolution.")))
        {
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
            if (Check(!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *PendingScreenshot) && IFileManager::Get().FileSize(*PendingScreenshot) == Png.Num(), TEXT("The rendered PIE capture was fully saved before completion was published.")))
            {
                bFirstCaptureCompleted |= PendingScreenshot == FirstScreenshot;
                bMiddleCaptureCompleted |= PendingScreenshot == MiddleScreenshot;
                ++CompletedCaptures;
                Test->AddInfo(FString::Printf(TEXT("Rendered PIE capture: %s; viewport=%dx%d; particle observation age=%.3f; world=%s."), *PendingScreenshot, Size.X, Size.Y, PendingParticleAge, *World->GetName()));
            }
            else bCaptureFailed = true;
        }
        else bCaptureFailed = true;
        PendingScreenshot.Reset();
        CaptureViewportClient.Reset();
    }

    bool Check(bool bValue, const FString& Message) { return Test->TestTrue(Message, bValue); }
    void Advance(int32 Next) { Stage = Next; Started = FPlatformTime::Seconds(); }
    bool End() { GEditor->RequestEndPlayMap(); Advance(9); return false; }
    FAutomationTestBase* Test;
    ACombatDebugPlayerController* Controller = nullptr;
    ACombatDebugGameMode* Mode = nullptr;
    AUnitBase* RevivedUnit = nullptr;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TStrongObjectPtr<USkillDefinitionDataAsset> Skill;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> OriginalSkills;
    FRunPartyMember OriginalEquipment;
    FTransform AliveMeshTransform;
    FCombatRoundSkill Definition;
    TArray<FString> Samples{TEXT("BPDA_N_BlackholeExplosion"), TEXT("BPDA_NS_Dark_Solo_Projectile"), TEXT("BPDA_NS_Fire_Slash"), TEXT("BPDA_P_Warrior_Swipe")};
    FString OutputDirectory;
    FString FirstScreenshot;
    FString MiddleScreenshot;
    FString PendingScreenshot;
    TWeakObjectPtr<UGameViewportClient> CaptureViewportClient;
    FDelegateHandle ViewportRenderedHandle;
    FIntPoint ExpectedScreenshotSize = FIntPoint::ZeroValue;
    int32 Stage = 0;
    int32 SampleIndex = 0;
    int32 SampleRound = 0;
    int32 MaxParticles = 0;
    int32 CompletedCaptures = 0;
    double Started = 0.0;
    float MaxAge = 0.f;
    float FirstParticleAge = 0.f;
    float PendingParticleAge = 0.f;
    bool bSawActive = false;
    bool bSawReady = false;
    bool bFirstScreenshot = false;
    bool bMiddleScreenshot = false;
    bool bFirstCaptureCompleted = false;
    bool bMiddleCaptureCompleted = false;
    bool bCaptureFailed = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatDebugAuthoredPIETest, "ProjectA.CombatDebugPIE.AuthoredToolsAndVfx", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatDebugAuthoredPIETest::RunTest(const FString& Parameters)
{
    if (!GEditor || !GEngine || !FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("Authored VFX PIE verification requires an editor with a real rendering device."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            AddError(TEXT("End the existing PIE session before running isolated debug verification."));
            return false;
        }
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/DebugCombat")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatDebugPIE::FAuthoredToolsAndVfx>(this));
    return true;
}

#endif
