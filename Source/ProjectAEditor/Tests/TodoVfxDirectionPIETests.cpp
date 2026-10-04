#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "ActiveSound.h"
#include "AssetCompilingManager.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatChainEffectActor.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Containers/Queue.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "ISubmixBufferListener.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraTypes.h"
#include "PlayInEditorDataTypes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Sound/SoundBase.h"
#include "Tests/AutomationEditorCommon.h"
#include "Unit/UnitBase.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace TodoVfxDirections
{
enum class EFocusedAudioCondition : uint8
{
    None,
    Default,
    ListenerOnly,
    NearCameraOnly,
    NearCameraAndListener
};

enum class ENinjaVisibilityCondition : uint8
{
    Baseline,
    VisualHeight30,
    SourceHeight20
};

struct FCase
{
    FString Label;
    FString Asset;
    FVector Direction = FVector::ForwardVector;
    int32 TargetIndex = 0;
    bool bChain = false;
    bool bLargeWorld = false;
    bool bCardinal = false;
    bool bFalling = false;
    bool bBasicAttack = false;
    bool bMonster = false;
    FString MonsterClass;
    EFocusedAudioCondition FocusedAudioCondition = EFocusedAudioCondition::None;
    bool bFocusedExtraPhases = false;
    bool bNinjaShortPhases = false;
    bool bFocusedFinalBurstPhase = false;
    ENinjaVisibilityCondition NinjaVisibilityCondition = ENinjaVisibilityCondition::Baseline;
};

TArray<FCase> MakeCases()
{
    TArray<FCase> Cases;
    const FString Root = TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/");
    const TArray<TPair<FString, FVector>> Directions = {{TEXT("E"), FVector(1, 0, 0)}, {TEXT("W"), FVector(-1, 0, 0)}, {TEXT("N"), FVector(0, 1, 0)}, {TEXT("S"), FVector(0, -1, 0)}};
    for (const FString& Theme : {FString(TEXT("Bramble")), FString(TEXT("Electric")), FString(TEXT("Energy")), FString(TEXT("Fire")), FString(TEXT("Magic"))})
    {
        const FString Asset = Root + TEXT("___LinkChainVFX/DA_DrGame_LinkChainVFX_Link_") + Theme;
        for (const TPair<FString, FVector>& Direction : Directions) Cases.Add({TEXT("chain_") + Theme + TEXT("_") + Direction.Key, Asset, Direction.Value, 0, true, false, true});
        Cases.Add({TEXT("chain_") + Theme + TEXT("_LWC"), Asset, FVector::ForwardVector, 0, true, true, true});
    }
    for (const FString& Theme : {FString(TEXT("FireArrow")), FString(TEXT("Hail"))})
    {
        const FString Asset = Root + TEXT("__AoeVFX/DA_DrGame_AoeVFX_AOE_") + Theme;
        for (int32 Target = 0; Target < 2; ++Target) Cases.Add({TEXT("fall_") + Theme + FString::Printf(TEXT("_target%d"), Target), Asset, FVector::ForwardVector, Target, false, false, false, true});
    }
    for (const TPair<FString, FVector>& Direction : Directions) Cases.Add({TEXT("tusk_") + Direction.Key, Root + TEXT("__GroundAttackVFX/DA_DrGame_GroundAttackVFX_BrambleTusk"), Direction.Value, 0, false, false, true});
    Cases.Add({TEXT("projectile_Arrow"), Root + TEXT("ProjectileHitVFX/DA_DrGame_ProjectileHitVFX_Arrow")});
    Cases.Add({TEXT("slash_Katana"), Root + TEXT("SlashHitVFX/DA_DrGame_SlashHitVFX_Slash_Katana")});
    Cases.Add({TEXT("heal_Ascend"), Root + TEXT("_LevelUpSpawn/DA_DrGame_LevelUpSpawn_LevelUp_Ascend_Root")});
    Cases.Add({TEXT("shield_Ground"), Root + TEXT("_LevelUpSpawn/DA_DrGame_LevelUpSpawn_Spawn_Ground_Root")});
    Cases.Add({TEXT("basic_Unarmed"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack"), FVector::ForwardVector, 0, false, false, false, false, true});
    Cases.Add({TEXT("basic_Melee"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack"), FVector::ForwardVector, 0, false, false, false, false, true});
    return Cases;
}

TArray<FCase> MakeFocusedCases()
{
    TArray<FCase> Cases;
    const FString Root = TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/");
    const FString Poison = Root + TEXT("__AoeVFX/DA_DrGame_AoeVFX_AOE_PoisonCarousel");
    const TArray<TPair<FString, EFocusedAudioCondition>> Conditions = {{TEXT("Default"), EFocusedAudioCondition::Default}, {TEXT("ListenerOnly"), EFocusedAudioCondition::ListenerOnly}, {TEXT("NearCameraOnly"), EFocusedAudioCondition::NearCameraOnly}, {TEXT("NearCameraAndListener"), EFocusedAudioCondition::NearCameraAndListener}};
    for (const TPair<FString, EFocusedAudioCondition>& Condition : Conditions)
    {
        FCase& Added = Cases.AddDefaulted_GetRef();
        Added.Label = TEXT("focus_PoisonCarousel_") + Condition.Key;
        Added.Asset = Poison;
        Added.FocusedAudioCondition = Condition.Value;
        Added.bFocusedExtraPhases = true;
    }
    for (const TPair<FString, FString>& Profile : TArray<TPair<FString, FString>>{{TEXT("FeudFang"), TEXT("__GroundAttackVFX/DA_DrGame_GroundAttackVFX_FeudFang")}, {TEXT("Line_Lava"), TEXT("__GroundAttackVFX/DA_DrGame_GroundAttackVFX_Line_Lava")}, {TEXT("Spawn_Ninja_Root"), TEXT("_LevelUpSpawn/DA_DrGame_LevelUpSpawn_Spawn_Ninja_Root")}})
    {
        FCase& Added = Cases.AddDefaulted_GetRef();
        Added.Label = TEXT("focus_") + Profile.Key;
        Added.Asset = Root + Profile.Value;
        Added.bFocusedExtraPhases = true;
        Added.bNinjaShortPhases = Profile.Key == TEXT("Spawn_Ninja_Root");
        Added.bFocusedFinalBurstPhase = Profile.Key == TEXT("Line_Lava");
    }
    return Cases;
}

TArray<FCase> MakeNinjaVisibilityCases()
{
    TArray<FCase> Cases;
    const TArray<TPair<FString, ENinjaVisibilityCondition>> Conditions = {{TEXT("Baseline"), ENinjaVisibilityCondition::Baseline}, {TEXT("VisualHeightPlus30"), ENinjaVisibilityCondition::VisualHeight30}, {TEXT("SourceHeight20"), ENinjaVisibilityCondition::SourceHeight20}};
    for (const TPair<FString, ENinjaVisibilityCondition>& Condition : Conditions)
    {
        FCase& Added = Cases.AddDefaulted_GetRef();
        Added.Label = TEXT("ninja_visibility_") + Condition.Key;
        Added.Asset = TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/_LevelUpSpawn/DA_DrGame_LevelUpSpawn_Spawn_Ninja_Root");
        Added.bFocusedExtraPhases = true;
        Added.bNinjaShortPhases = true;
        Added.NinjaVisibilityCondition = Condition.Value;
    }
    return Cases;
}

struct FObservedSegment
{
    TWeakObjectPtr<UNiagaraComponent> Component;
    TWeakObjectPtr<AUnitBase> Target;
    const FNiagaraSystemInstance* Instance = nullptr;
    FVector FixedSource = FVector::ZeroVector;
    FVector TargetBeforeFixtureMove = FVector::ZeroVector;
    int32 Sequence = INDEX_NONE;
    uint64 MovedOnFrame = 0;
    int32 EndpointProgressSteps = 0;
    double MovedAtWorldTime = 0.0;
    float LastAge = 0.f;
    bool bMovedTarget = false;
    bool bCaptured = false;
    bool bFollowingVerified = false;
};

struct FMovedActor
{
    TWeakObjectPtr<AActor> Actor;
    FTransform Original;
};

struct FAudioReviewDiagnostic
{
    FString Label;
    FString ListenerPositions;
    FString SoundDetails;
    FString ExpectedCuePath;
    int32 CaseIndex = INDEX_NONE;
    uint32 DeviceId = 0;
    int32 ActiveWorldSounds = 0;
    int32 ActiveMixerSources = 0;
    int32 WorldListeners = 0;
    int32 ExpectedCueCount = 0;
    double AudioClock = 0.0;
    float ExpectedCuePlaybackTime = 0.f;
    float ExpectedCuePlaybackTimeUnscaled = 0.f;
    float PrimaryVolume = 0.f;
    float TransientVolume = 0.f;
    float AppVolume = 0.f;
    float UnfocusedVolume = 0.f;
    bool bDeviceMuted = false;
    bool bAppFocused = false;
};

struct FAudioReviewMailbox
{
    TQueue<FAudioReviewDiagnostic, EQueueMode::Mpsc> Pending;
};

struct FAudioReviewClockSnapshot
{
    int64 Samples = 0;
    int32 Callbacks = 0;
    int32 SampleRate = 0;
    int32 Channels = 0;
    double FirstAudioClock = -1.0;
    double AudioClock = 0.0;
};

// Observe actual master-submix render callbacks without injecting sound or advancing the mixer manually.
// 사운드를 주입하거나 믹서를 수동 진행하지 않고 실제 마스터 서브믹스 렌더 콜백을 관찰합니다.
class FAudioReviewClock final : public ISubmixBufferListener
{
public:
    virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override
    {
        FScopeLock Lock(&Mutex);
        Snapshot.Samples += NumSamples;
        ++Snapshot.Callbacks;
        Snapshot.SampleRate = SampleRate;
        Snapshot.Channels = NumChannels;
        if (Snapshot.FirstAudioClock < 0.0) Snapshot.FirstAudioClock = AudioClock;
        Snapshot.AudioClock = AudioClock;
    }

    FAudioReviewClockSnapshot Read() const
    {
        FScopeLock Lock(&Mutex);
        return Snapshot;
    }

    virtual bool IsRenderingAudio() const override { return true; }
    virtual const FString& GetListenerName() const override
    {
        static const FString Name(TEXT("ProjectATodoReviewMasterClock"));
        return Name;
    }

private:
    mutable FCriticalSection Mutex;
    FAudioReviewClockSnapshot Snapshot;
};

// Exercise authored effects through the real coordinator, observing live Niagara and rendered frames without advancing simulation.
// 실제 조정자로 원본 효과를 실행하며 시뮬레이션을 수동 진행하지 않고 활성 Niagara와 렌더링 프레임을 관찰합니다.
class FDirectionReview : public IAutomationLatentCommand
{
public:
    FDirectionReview(FAutomationTestBase* InTest, const FString& InSlot, bool bInMonsters = false) : Test(InTest), SaveSlot(InSlot), Cases(bInMonsters ? TArray<FCase>() : MakeCases()), bMonsterReview(bInMonsters)
    {
        bNinjaVisibilityReview = !bMonsterReview && FParse::Param(FCommandLine::Get(), TEXT("ProjectANinjaVisibilityReview"));
        bFocusedReview = !bMonsterReview && (bNinjaVisibilityReview || FParse::Param(FCommandLine::Get(), TEXT("ProjectAVfxFocusedReview")));
        bSettlingReview = bMonsterReview && FParse::Param(FCommandLine::Get(), TEXT("ProjectARagdollSettlingReview"));
        if (bFocusedReview) Cases = bNinjaVisibilityReview ? MakeNinjaVisibilityCases() : MakeFocusedCases();
        const FString ReviewFolder = bNinjaVisibilityReview ? TEXT("VfxNinjaVisibility") : bFocusedReview ? TEXT("VfxFocused") : bSettlingReview ? TEXT("MonsterSettling") : bMonsterReview ? TEXT("MonsterAttacks") : TEXT("VfxDirections");
        OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TodoReview") / ReviewFolder / FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*OutputDirectory, true);
        CaptureHandle = UGameViewportClient::OnViewportRendered().AddRaw(this, &FDirectionReview::CaptureRenderedViewport);
        bRecordRequested = FParse::Param(FCommandLine::Get(), TEXT("ProjectARecordReviewAudio"));
    }

    virtual ~FDirectionReview() override
    {
        if (bRecordingAudio && IsValid(Controller)) UAudioMixerBlueprintLibrary::StopRecordingOutput(Controller, EAudioRecordingExportType::WavFile, AudioBasename, OutputDirectory);
        UnregisterAudioReviewClock();
        RestoreAudioReviewEnvironment();
        RestoreFocusedView();
        RestoreMovedActors();
        UGameViewportClient::OnViewportRendered().Remove(CaptureHandle);
        RemoveGasObserver();
        ReleaseRetainedViewport();
    }

    virtual bool Update() override
    {
        DrainAudioDiagnostics();
        if (StageStarted == 0.0) StageStarted = FPlatformTime::Seconds();
        if (Stage == 9)
        {
            bool bPIEStillPresent = false;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && FPlatformTime::Seconds() - StageStarted < 30.0) return false;
                if (Context.WorldType == EWorldType::PIE)
                {
                    bPIEStillPresent = true;
                    Check(false, TEXT("VFX review PIE closes within its bounded teardown time."));
                }
            }
            if (!bPIEStillPresent) ReleaseRetainedViewport();
            Check(!UGameplayStatics::DoesSaveGameExist(SaveSlot, 0), TEXT("Disposable VFX review never creates or modifies a Run checkpoint."));
            return true;
        }
        if (FPlatformTime::Seconds() - StageStarted > 120.0)
        {
            Check(false, FString::Printf(TEXT("Render review timed out at stage %d, case %d (%s)."), Stage, CaseIndex, Cases.IsValidIndex(CaseIndex) ? *Cases[CaseIndex].Label : TEXT("awaiting catalog")));
            return End();
        }
        if (Stage == 0)
        {
            if (bRecordRequested && !bMonsterReview && !PrepareAudioReviewEnvironment()) return End();
            Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            Settings->SetPlayNetMode(PIE_Standalone);
            Settings->SetPlayNumberOfClients(1);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;
            Settings->NewWindowWidth = 1280;
            Settings->NewWindowHeight = 720;
            Settings->SetClientWindowSize(FIntPoint(1280, 720));
            if (bRecordRequested && !bMonsterReview)
            {
                // Enable sound only on the duplicated review settings; saved editor audio preferences remain unchanged.
                // 저장된 에디터 오디오 설정은 유지하고 복제한 검수 설정에서만 사운드를 활성화합니다.
                Settings->EnableGameSound = true;
                Settings->DisableStandaloneSound = false;
                Settings->bUseNonRealtimeAudioDevice = false;
                Settings->SoloAudioInFirstPIEClient = true;
            }
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = Settings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat");
            GEditor->RequestPlaySession(Params);
            Advance(1);
            return false;
        }
        if (Stage == 1)
        {
            if (!Connect()) return false;
            if (!bCatalogExpanded && !ExpandCatalog()) return End();
            if (!PrepareCase()) return End();
            Advance(2);
            return false;
        }
        if (!Check(IsValid(Controller) && IsValid(Mode) && IsValid(Source) && Controller->GetRoundCoordinator(), TEXT("The live local combat context remains valid."))) return End();
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        if (Stage == 4) return ObserveMonsterDeathAndRestart();
        if (Stage == 5)
        {
            QueueAudioDiagnostics();
            const FAudioReviewClockSnapshot Clock = RecordingClock.IsValid() ? RecordingClock->Read() : FAudioReviewClockSnapshot();
            if (!AudioStartFence.IsFenceComplete() || FPlatformTime::Seconds() - AudioStartedAt < 1.0 || Clock.Callbacks < 3 || Clock.SampleRate <= 0 || Clock.Channels <= 0 || Clock.Samples < int64(Clock.SampleRate) * Clock.Channels / 4 || Clock.AudioClock - Clock.FirstAudioClock < 0.25)
            {
                if (FPlatformTime::Seconds() - AudioStartedAt > 10.0)
                {
                    Check(false, TEXT("The actual master submix produces recording-ready samples after the audio-thread start fence within ten real seconds."));
                    return End();
                }
                return false;
            }
            Test->AddInfo(FString::Printf(TEXT("AudioReview recording ready %s: device=%u callbacks=%d samples=%lld rate=%d channels=%d audioClock=%.3f wallSeconds=%.3f."), *Cases[CaseIndex].Label, RecordingDevice.GetDeviceID(), Clock.Callbacks, Clock.Samples, Clock.SampleRate, Clock.Channels, Clock.AudioClock, FPlatformTime::Seconds() - AudioStartedAt));
            if (!ExecuteCase()) return End();
            Advance(3);
            return false;
        }
        if (Stage == 2)
        {
            ++WarmFrames;
            if (WarmFrames < 30 || FPlatformTime::Seconds() - StageStarted < 1.0 || FAssetCompilingManager::Get().GetNumRemainingAssets() != 0) return false;
            if (!Definition.Vfx.Niagara.IsNull() && (!Definition.Vfx.Niagara.Get() || !Definition.Vfx.Niagara.Get()->IsReadyToRun())) return false;
            const UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
            if (!Check(Viewport && Viewport->Viewport, TEXT("Warm-up retains a real physical rendering viewport."))) return End();
            const FIntPoint ActualSize = Viewport->Viewport->GetRenderTargetTextureSizeXY();
            if (ActualSize != FIntPoint(1280, 720))
            {
                const TSharedPtr<SWindow> Window = ReviewWindow.Pin();
                if (!Check(Window.IsValid() && ResizeAttempts < 3, TEXT("Bounded Slate feedback restores the exact requested physical viewport size."))) return End();
                ++ResizeAttempts;
                // Compensate actual border and DPI fitting using rendered pixel feedback instead of assuming client-size equivalence.
                // 클라이언트 크기를 픽셀 크기와 같다고 가정하지 않고 렌더 픽셀 피드백으로 실제 테두리·DPI 차이를 보정합니다.
                Window->Resize(Window->GetClientSizeInScreen() + FVector2D(1280 - ActualSize.X, 720 - ActualSize.Y));
                Test->AddInfo(FString::Printf(TEXT("VFX viewport feedback resize %d: physical=%s; new client=%s; DPI=%.3f."), ResizeAttempts, *ActualSize.ToString(), *Window->GetClientSizeInScreen().ToString(), Window->GetDPIScaleFactor()));
                WarmFrames = 0;
                StageStarted = FPlatformTime::Seconds();
                return false;
            }
            if (bFocusedReview && !InspectFocusedView()) return End();
            if (ShouldRecordCase())
            {
                if (!StartAudioReviewRecording()) return End();
                Advance(5);
                return false;
            }
            if (!ExecuteCase()) return End();
            Advance(3);
            return false;
        }
        if (Stage == 3)
        {
            if (bRecordingAudio) QueueAudioDiagnostics();
            if (Round->GetView().Phase == ECombatRoundPhase::Resolving && !Check(Source->GetCurrentActionPoint() == InitialAP - Definition.ActionPointCost, TEXT("All impacts and delayed jumps retain exactly one paid action cost."))) return End();
            if (!ObserveLiveEffect() || bCaptureFailed) return End();
            if (Round->GetView().RoundNumber == InitialRound && Round->GetView().Phase != ECombatRoundPhase::Finished) return false;
            if (!PendingScreenshot.IsEmpty()) return false;
            const int32 ExpectedCaptures = AttackCaptureCount(Cases[CaseIndex]);
            if (CapturedThisCase < ExpectedCaptures && FPlatformTime::Seconds() - StageStarted < 15.0) return false;
            const int32 ExpectedImpacts = Cases[CaseIndex].bChain ? 3 : 1;
            if (!Definition.ImpactVfx.Niagara.IsNull() && (ImpactInstances.Num() < ExpectedImpacts || MaxImpactParticles <= 0) && FPlatformTime::Seconds() - StageStarted < 15.0) return false;
            if (bRecordingAudio)
            {
                if (bNaturalCueObservationRequired && !ObserveNaturalCueCompletion()) return bNaturalCueObservationFailed ? End() : false;
                const FAudioReviewClockSnapshot Clock = RecordingClock->Read();
                Test->AddInfo(FString::Printf(TEXT("AudioReview recording stop %s: callbacks=%d samples=%lld audioClock=%.3f elapsed=%.3f."), *Cases[CaseIndex].Label, Clock.Callbacks, Clock.Samples, Clock.AudioClock, FPlatformTime::Seconds() - AudioStartedAt));
                UAudioMixerBlueprintLibrary::StopRecordingOutput(Controller, EAudioRecordingExportType::WavFile, AudioBasename, OutputDirectory);
                bRecordingAudio = false;
                UnregisterAudioReviewClock();
                bAudioExportPending = true;
                AudioStoppedAt = FPlatformTime::Seconds();
                return false;
            }
            if (bAudioExportPending)
            {
                const int64 Bytes = IFileManager::Get().FileSize(*AudioFilename);
                AudioStableFrames = Bytes > 44 && Bytes == AudioLastBytes ? AudioStableFrames + 1 : 0;
                AudioLastBytes = Bytes;
                if (AudioStableFrames < 2)
                {
                    if (FPlatformTime::Seconds() - AudioStoppedAt < 15.0) return false;
                    Check(false, TEXT("The optional master-submix WAV export completes within its bounded wait."));
                    return End();
                }
                bAudioExportPending = false;
                Test->AddInfo(FString::Printf(TEXT("Recorded live master-submix WAV: %s; bytes=%lld. Signal and clipping analysis is separate from perceptual listening."), *AudioFilename, Bytes));
            }
            if (!FinishCase()) return End();
            RemoveGasObserver();
            if (Cases[CaseIndex].bMonster)
            {
                // Finish attack assertions before applying independent lethal GAS damage through the public combat utility.
                // 공개 전투 유틸리티로 별도 치명 GAS 피해를 적용하기 전에 공격 계약 검증을 완료합니다.
                DeathHomeTile = Source->GetCurrentTile();
                Records.Last()->AsObject()->SetBoolField(TEXT("attack_runtime_contract_passed"), true);
                Records.Last()->AsObject()->SetBoolField(TEXT("passed_runtime_contract"), false);
                const UAS_Unit* Attributes = Source->GetAttributeSet();
                if (!Check(DeathHomeTile.IsValid() && UCombatEffectLibrary::ApplyDamageToUnit(Target, Source, UGE_Damage::StaticClass(), Attributes->GetHP() + Attributes->GetShield() + 1.f), TEXT("Actual public GAS damage kills the original monster after its authored attack and return."))) return End();
                WarmFrames = 0;
                bDeathCaptureQueued = false;
                bLateDeathCaptureQueued = false;
                bSettlingDeathCaptureQueued = false;
                SettlingFrameSamples.Reset();
                LastSettlingObservedWorldTime = -1.0;
                DeathStartedWorldTime = Controller->GetWorld()->GetTimeSeconds();
                Advance(4);
                return false;
            }
            RestoreFocusedView();
            RestoreMovedActors();
            if (IsValid(LargeWorldFloor.Get())) LargeWorldFloor->Destroy();
            LargeWorldFloor.Reset();
            if (IsValid(FixtureMonster.Get())) FixtureMonster->Destroy();
            FixtureMonster.Reset();
            ++CaseIndex;
            if (CaseIndex == Cases.Num())
            {
                Check(CompletedCaptures + (bFocusedReview ? DeferredCaptures : 0) == PlannedCaptures, bFocusedReview ? TEXT("Every focused requested phase is either captured during natural playback or explicitly recorded as unobserved.") : TEXT("Every catalog case produces its requested live viewport captures."));
                return End();
            }
            if (!Check(Mode->RestartCombat(), TEXT("The next effect receives a fresh disposable combat."))) return End();
            Advance(1);
        }
        return false;
    }

private:
    bool NeedsLateCaptures(const FCase& Current, const FCombatRoundSkill& Skill) const { return !Current.bChain && !Current.bBasicAttack && (Current.bFocusedExtraPhases || Current.bFalling || Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Area) || Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal) || Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield)); }
    int32 CapturePlanCount(const FCase& Current, const FCombatRoundSkill& Skill) const { return Current.bChain || Current.bMonster ? 3 : 2 + (NeedsLateCaptures(Current, Skill) ? 2 : 0) + (Current.bNinjaShortPhases ? 2 : 0) + (Current.bFocusedFinalBurstPhase ? 1 : 0) + (!Current.bBasicAttack && !Skill.ImpactVfx.Niagara.IsNull() ? 1 : 0); }
    int32 AttackCaptureCount(const FCase& Current) const { return CapturePlanCount(Current, Definition); }

    bool PrepareFocusedView(AActor* Camera, const FVector& Focus)
    {
        if (!bFocusedReview) return true;
        FVector ExistingAttenuationPosition = FVector::ZeroVector;
        // Reject an existing attached override before mutation because the public API cannot reconstruct its attachment.
        // 공개 API로 기존 부착 상태를 재구성할 수 없어 기존 오버라이드가 있으면 변경 전에 거부합니다.
        if (!Check(IsValid(Camera) && Camera->IsA<ACameraActor>() && !Controller->GetAudioListenerAttenuationOverridePosition(ExistingAttenuationPosition), TEXT("Focused review starts with the actual isolated arena camera and an unchanged default attenuation listener."))) return false;
        const FCase& Current = Cases[CaseIndex];
        const bool bNearCamera = Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraOnly || Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraAndListener;
        const bool bListenerOnly = Current.FocusedAudioCondition == EFocusedAudioCondition::ListenerOnly || Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraAndListener;
        if (bNearCamera)
        {
            // Change only the camera translation height and re-aim at the same focus; authored VFX LOD and quality remain untouched.
            // 카메라 이동 높이만 바꾸고 같은 중심을 다시 바라보며 원본 VFX LOD·품질은 유지합니다.
            FVector Position = Camera->GetActorLocation();
            Position.Z = Focus.Z + 500.0;
            MoveFixtureActor(Camera, FTransform((Focus - Position).Rotation(), Position, Camera->GetActorScale3D()));
        }
        if (bListenerOnly)
        {
            Controller->SetAudioListenerAttenuationOverride(Target->GetCapsuleComponent(), FVector::ZeroVector);
            bFocusedListenerChanged = true;
        }
        return true;
    }

    bool InspectFocusedView()
    {
        if (bFocusedViewObserved) return true;
        FVector ViewPosition = FVector::ZeroVector;
        FRotator ViewRotation = FRotator::ZeroRotator;
        Controller->GetPlayerViewPoint(ViewPosition, ViewRotation);
        FVector AttenuationPosition = FVector::ZeroVector;
        const bool bOverride = Controller->GetAudioListenerAttenuationOverridePosition(AttenuationPosition);
        const FCase& Current = Cases[CaseIndex];
        const bool bNearCamera = Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraOnly || Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraAndListener;
        const bool bListener = Current.FocusedAudioCondition == EFocusedAudioCondition::ListenerOnly || Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraAndListener;
        FocusedViewPosition = ViewPosition;
        FocusedViewTargetDistance = FVector::Dist(ViewPosition, Target->GetCapsuleComponent()->GetComponentLocation());
        if (!Check(bOverride == bListener && (!bListener || AttenuationPosition.Equals(Target->GetCapsuleComponent()->GetComponentLocation(), 0.1)), TEXT("The actual focused listener uses exactly the requested scoped attenuation override."))) return false;
        if (Current.FocusedAudioCondition != EFocusedAudioCondition::None && !Check(bNearCamera ? FocusedViewTargetDistance < 2000.0 : FocusedViewTargetDistance > 2000.0, TEXT("Actual focused camera viewpoints straddle the original 2000cm audio-emitter distance boundary."))) return false;
        bFocusedViewObserved = true;
        Test->AddInfo(FString::Printf(TEXT("Focused VFX view %s: actual camera=%s target=%s distance=%.3fcm attenuationOverride=%d attenuationPosition=%s. Niagara LOD/scalability and authored audio parameters are unchanged."), *Current.Label, *ViewPosition.ToString(), *Target->GetCapsuleComponent()->GetComponentLocation().ToString(), FocusedViewTargetDistance, bOverride, *AttenuationPosition.ToString()));
        return true;
    }

    void RestoreFocusedView()
    {
        if (!bFocusedListenerChanged) return;
        if (IsValid(Controller))
        {
            Controller->ClearAudioListenerAttenuationOverride();
            FVector Position = FVector::ZeroVector;
            Check(!Controller->GetAudioListenerAttenuationOverridePosition(Position), TEXT("Focused review restores the original default attenuation-listener state before restart or teardown."));
        }
        bFocusedListenerChanged = false;
    }

    bool PrepareAudioReviewEnvironment()
    {
        IConsoleVariable* AppVolumeBypass = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume"));
        IConsoleVariable* NeverDisableSubmixes = IConsoleManager::Get().FindConsoleVariable(TEXT("au.NeverDisableSubmixes"));
        if (!Check(AppVolumeBypass && NeverDisableSubmixes && !FParse::Param(FCommandLine::Get(), TEXT("nosound")), TEXT("Optional audio review requires enabled sound and the official focus-volume and silent-submix diagnostic controls."))) return false;
        PreviousAppVolumeBypass = AppVolumeBypass->GetInt();
        PreviousNeverDisableSubmixes = NeverDisableSubmixes->GetInt();
        PreviousAppVolumePriority = static_cast<EConsoleVariableFlags>(AppVolumeBypass->GetFlags() & ECVF_SetByMask);
        PreviousNeverDisablePriority = static_cast<EConsoleVariableFlags>(NeverDisableSubmixes->GetFlags() & ECVF_SetByMask);
        Test->AddInfo(FString::Printf(TEXT("AudioReview environment before temporary bypass: appFocused=%d appVolume=%.3f unfocusedVolume=%.3f au.DisableAppVolume=%d au.NeverDisableSubmixes=%d. Focus muting and auto-disabled silent submixes are separate recording-environment conditions."), FApp::HasFocus(), FApp::GetVolumeMultiplier(), FApp::GetUnfocusedVolumeMultiplier(), PreviousAppVolumeBypass, PreviousNeverDisableSubmixes));
        // Preserve console priorities and saved preferences while allowing offscreen gain and silent-buffer recording only during review.
        // 콘솔 우선순위와 저장 설정을 보존하며 검수 중에만 화면 밖 볼륨과 무음 버퍼 녹음을 허용합니다.
        AppVolumeBypass->Set(1, PreviousAppVolumePriority);
        NeverDisableSubmixes->Set(1, PreviousNeverDisablePriority);
        bAudioEnvironmentChanged = true;
        return Check(AppVolumeBypass->GetInt() == 1 && NeverDisableSubmixes->GetInt() == 1, TEXT("The scoped recording review bypasses focus muting and records true silence without changing original sound parameters."));
    }

    void RestoreAudioReviewEnvironment()
    {
        if (!bAudioEnvironmentChanged) return;
        if (IConsoleVariable* AppVolumeBypass = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume")))
        {
            AppVolumeBypass->Set(PreviousAppVolumeBypass, PreviousAppVolumePriority);
            Check(AppVolumeBypass->GetInt() == PreviousAppVolumeBypass && (AppVolumeBypass->GetFlags() & ECVF_SetByMask) == PreviousAppVolumePriority, TEXT("The optional recording review restores its original application-volume console value and priority."));
        }
        if (IConsoleVariable* NeverDisableSubmixes = IConsoleManager::Get().FindConsoleVariable(TEXT("au.NeverDisableSubmixes")))
        {
            NeverDisableSubmixes->Set(PreviousNeverDisableSubmixes, PreviousNeverDisablePriority);
            Check(NeverDisableSubmixes->GetInt() == PreviousNeverDisableSubmixes && (NeverDisableSubmixes->GetFlags() & ECVF_SetByMask) == PreviousNeverDisablePriority, TEXT("The optional recording review restores its original submix auto-disable console value and priority."));
        }
        bAudioEnvironmentChanged = false;
    }

    bool ShouldRecordCase() const { return bRecordRequested && !Cases[CaseIndex].bMonster && Cases[CaseIndex].Asset.StartsWith(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/")); }

    bool StartAudioReviewRecording()
    {
        RecordingDevice = Controller->GetWorld()->GetAudioDevice();
        if (!Check(RecordingDevice.IsValid() && RecordingDevice->bAudioMixerModuleLoaded && !FParse::Param(FCommandLine::Get(), TEXT("nosound")), TEXT("Optional review recording requires the live world's audio mixer and enabled sound."))) return false;
        bNaturalCueObservationRequired = bFocusedReview && Cases[CaseIndex].FocusedAudioCondition != EFocusedAudioCondition::None && !Definition.Vfx.Sound.IsNull();
        if (bNaturalCueObservationRequired)
        {
            USoundBase* Cue = Definition.Vfx.Sound.LoadSynchronous();
            const float Pitch = Cue ? Definition.Vfx.SoundPitch * Cue->GetPitchMultiplier() : 0.f;
            NaturalCueDuration = Cue ? Cue->GetDuration() : 0.f;
            if (!Check(Cue && !Cue->IsLooping() && FMath::IsFinite(NaturalCueDuration) && NaturalCueDuration > 0.f && FMath::IsFinite(Pitch) && Pitch > 0.f, TEXT("The focused external original cue has a finite nonlooping authored duration and unchanged positive pitch."))) return false;
            NaturalCueExpectedSeconds = NaturalCueDuration / Pitch;
            if (!Check(NaturalCueExpectedSeconds < 10.0 && Definition.Vfx.SoundMaxDuration >= NaturalCueExpectedSeconds, TEXT("The existing original cue completes within its unchanged production sound lifetime and the bounded observation window."))) return false;
            NaturalCuePath = Cue->GetPathName();
            Test->AddInfo(FString::Printf(TEXT("Focused cue completion requested: %s; authoredDuration=%.6f; authoredPitch=%.6f; expectedSeconds=%.6f; unchangedSoundCap=%.6f."), *NaturalCuePath, NaturalCueDuration, Pitch, NaturalCueExpectedSeconds, Definition.Vfx.SoundMaxDuration));
        }
        AudioBasename = FString::Printf(TEXT("%02d_%s"), CaseIndex + 1, *Cases[CaseIndex].Label);
        AudioFilename = OutputDirectory / (AudioBasename + TEXT(".wav"));
        RecordingClock = MakeShared<FAudioReviewClock, ESPMode::ThreadSafe>();
        RecordingDevice->RegisterSubmixBufferListener(RecordingClock.ToSharedRef(), RecordingDevice->GetMainSubmixObject());
        // Queue recording before the fence, then require real callback samples and wall-clock warm-up before submitting the cast.
        // 녹음을 펜스보다 먼저 등록하고 실제 콜백 샘플과 실시간 준비 대기 후 시전 명령을 제출합니다.
        UAudioMixerBlueprintLibrary::StartRecordingOutput(Controller, 20.f);
        AudioStartFence.BeginFence();
        AudioStartedAt = FPlatformTime::Seconds();
        bRecordingAudio = true;
        return true;
    }

    bool ObserveNaturalCueCompletion()
    {
        const FAudioReviewClockSnapshot Clock = RecordingClock->Read();
        if (bNaturalCueObservationFailed) return false;
        if (bNaturalCueSeen && NaturalCueAbsentSamples >= 2 && NaturalCueFirstAbsentClock >= 0.0 && Clock.AudioClock >= FMath::Max(NaturalCueFirstSeenClock + NaturalCueExpectedSeconds, NaturalCueFirstAbsentClock) + 0.15)
        {
            bNaturalCueCompletionObserved = true;
            Test->AddInfo(FString::Printf(TEXT("Focused original cue observed through completion: %s; firstSeenClock=%.6f; inferredStartClock=%.6f; firstAbsentClock=%.6f; lastActiveClock=%.6f; maxPlaybackAge=%.6f; latestSubmixClock=%.6f; absentSamples=%d. No restart, stop or presentation lifetime change occurred during the observation."), *NaturalCuePath, NaturalCueFirstSeenClock, NaturalCueInferredStartClock, NaturalCueFirstAbsentClock, NaturalCueLastActiveClock, NaturalCueMaxPlaybackTime, Clock.AudioClock, NaturalCueAbsentSamples));
            return true;
        }
        if (FPlatformTime::Seconds() - StageStarted <= 10.0) return false;
        bNaturalCueObservationFailed = true;
        return Check(false, FString::Printf(TEXT("Focused original cue completion is not established within ten real seconds: %s; seen=%d; expectedSeconds=%.6f; firstSeenClock=%.6f; inferredStartClock=%.6f; firstAbsentClock=%.6f; lastActiveClock=%.6f; maxPlaybackAge=%.6f; latestSubmixClock=%.6f; absentSamples=%d."), *NaturalCuePath, bNaturalCueSeen, NaturalCueExpectedSeconds, NaturalCueFirstSeenClock, NaturalCueInferredStartClock, NaturalCueFirstAbsentClock, NaturalCueLastActiveClock, NaturalCueMaxPlaybackTime, Clock.AudioClock, NaturalCueAbsentSamples));
    }

    void UnregisterAudioReviewClock()
    {
        if (RecordingDevice.IsValid() && RecordingClock.IsValid()) RecordingDevice->UnregisterSubmixBufferListener(RecordingClock.ToSharedRef(), RecordingDevice->GetMainSubmixObject());
        RecordingClock.Reset();
        RecordingDevice.Reset();
    }

    void QueueAudioDiagnostics()
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - LastAudioDiagnosticAt < 0.1 || !IsValid(Controller)) return;
        LastAudioDiagnosticAt = Now;
        const FAudioDeviceHandle Device = Controller->GetWorld()->GetAudioDevice();
        if (!Device.IsValid()) return;
        FAudioReviewDiagnostic Snapshot;
        Snapshot.Label = Cases[CaseIndex].Label;
        Snapshot.CaseIndex = CaseIndex;
        Snapshot.DeviceId = Device.GetDeviceID();
        Snapshot.AppVolume = FApp::GetVolumeMultiplier();
        Snapshot.UnfocusedVolume = FApp::GetUnfocusedVolumeMultiplier();
        Snapshot.bAppFocused = FApp::HasFocus();
        Snapshot.ExpectedCuePath = NaturalCuePath;
        const uint32 WorldId = Controller->GetWorld()->GetUniqueID();
        const TSharedRef<FAudioReviewMailbox, ESPMode::ThreadSafe> Mailbox = AudioMailbox;
        // Inspect device sounds and listeners on their owning audio thread; the mailbox remains valid even if PIE ends before delivery.
        // 장치 사운드와 리스너를 소유 오디오 스레드에서 읽고 PIE가 먼저 종료돼도 유효한 우편함으로 결과를 전달합니다.
        FAudioThread::RunCommandOnAudioThread([Device, Mailbox, WorldId, Snapshot]() mutable
        {
            Snapshot.bDeviceMuted = Device->IsAudioDeviceMuted();
            Snapshot.PrimaryVolume = Device->GetPrimaryVolume();
            Snapshot.TransientVolume = Device->GetTransientPrimaryVolume();
            Snapshot.ActiveMixerSources = Device->GetNumActiveSources();
            Snapshot.AudioClock = Device->GetAudioClock();
            const TArray<FListener>& Listeners = Device->GetListeners();
            for (const FListener& Listener : Listeners)
            {
                if (Listener.WorldID != WorldId) continue;
                ++Snapshot.WorldListeners;
                if (!Snapshot.ListenerPositions.IsEmpty()) Snapshot.ListenerPositions += TEXT("; ");
                // Mirror the official override-enabled listener position using public data because GetPosition is not exported by Engine.
                // Engine에서 GetPosition을 내보내지 않으므로 공개 데이터로 공식 감쇠 오버라이드 위치를 동일하게 읽습니다.
                Snapshot.ListenerPositions += (Listener.bUseAttenuationOverride ? Listener.AttenuationOverride : Listener.Transform.GetTranslation()).ToString();
            }
            for (const FActiveSound* Sound : Device->GetActiveSounds())
            {
                if (!Sound || Sound->GetWorldID() != WorldId) continue;
                ++Snapshot.ActiveWorldSounds;
                if (!Snapshot.ExpectedCuePath.IsEmpty() && GetPathNameSafe(Sound->GetSound()) == Snapshot.ExpectedCuePath)
                {
                    ++Snapshot.ExpectedCueCount;
                    Snapshot.ExpectedCuePlaybackTime = FMath::Max(Snapshot.ExpectedCuePlaybackTime, Sound->PlaybackTime);
                    Snapshot.ExpectedCuePlaybackTimeUnscaled = FMath::Max(Snapshot.ExpectedCuePlaybackTimeUnscaled, Sound->PlaybackTimeUnscaled);
                }
                if (Snapshot.ActiveWorldSounds > 4) continue;
                double ListenerDistance = TNumericLimits<double>::Max();
                for (const FListener& Listener : Listeners)
                {
                    if (Listener.WorldID == WorldId) ListenerDistance = FMath::Min(ListenerDistance, FVector::Dist(Sound->Transform.GetLocation(), Listener.bUseAttenuationOverride ? Listener.AttenuationOverride : Listener.Transform.GetTranslation()));
                }
                if (!Snapshot.SoundDetails.IsEmpty()) Snapshot.SoundDetails += TEXT("; ");
                Snapshot.SoundDetails += FString::Printf(TEXT("%s gain=%.3f position=%s attenuation=%d maxDistance=%.1f listenerDistance=%.1f age=%.3f"), *GetPathNameSafe(Sound->GetSound()), Sound->GetVolume(), *Sound->Transform.GetLocation().ToString(), Sound->bHasAttenuationSettings, Sound->MaxDistance, ListenerDistance, Sound->PlaybackTime);
            }
            Mailbox->Pending.Enqueue(MoveTemp(Snapshot));
        });
    }

    void DrainAudioDiagnostics()
    {
        FAudioReviewDiagnostic Snapshot;
        while (AudioMailbox->Pending.Dequeue(Snapshot))
        {
            if (bNaturalCueObservationRequired && Snapshot.CaseIndex == CaseIndex && Snapshot.ExpectedCuePath == NaturalCuePath)
            {
                if (Snapshot.ExpectedCueCount > 0)
                {
                    if (NaturalCueFirstAbsentClock >= 0.0 || Snapshot.ExpectedCueCount != 1)
                    {
                        bNaturalCueObservationFailed = true;
                        Check(false, TEXT("The focused original cue plays once without reactivation after its observed disappearance."));
                    }
                    if (!bNaturalCueSeen)
                    {
                        NaturalCueFirstSeenClock = Snapshot.AudioClock;
                        NaturalCueInferredStartClock = Snapshot.AudioClock - Snapshot.ExpectedCuePlaybackTimeUnscaled;
                    }
                    bNaturalCueSeen = true;
                    NaturalCueLastActiveClock = Snapshot.AudioClock;
                    NaturalCueMaxPlaybackTime = FMath::Max(NaturalCueMaxPlaybackTime, Snapshot.ExpectedCuePlaybackTime);
                    NaturalCueAbsentSamples = 0;
                }
                else if (bNaturalCueSeen)
                {
                    if (NaturalCueFirstAbsentClock < 0.0)
                    {
                        NaturalCueFirstAbsentClock = Snapshot.AudioClock;
                        // Bound the first absent observation by the actual audio-thread sampling interval, then retain a full authored-duration submix tail.
                        // 첫 부재 관측의 허용 오차를 실제 오디오 스레드 샘플 간격으로 제한하고 원본 길이를 덮는 서브믹스 꼬리를 유지합니다.
                        if (NaturalCueFirstAbsentClock - NaturalCueInferredStartClock + 0.1 < NaturalCueExpectedSeconds)
                        {
                            bNaturalCueObservationFailed = true;
                            Check(false, FString::Printf(TEXT("The original cue disappeared before its finite authored duration: %s; observedSeconds=%.6f; expectedSeconds=%.6f; samplingTolerance=0.1; maxPlaybackAge=%.6f."), *NaturalCuePath, NaturalCueFirstAbsentClock - NaturalCueInferredStartClock, NaturalCueExpectedSeconds, NaturalCueMaxPlaybackTime));
                        }
                    }
                    ++NaturalCueAbsentSamples;
                }
            }
            if (bFocusedReview)
            {
                TSharedRef<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
                Diagnostic->SetStringField(TEXT("case"), Snapshot.Label);
                Diagnostic->SetNumberField(TEXT("case_index"), Snapshot.CaseIndex + 1);
                Diagnostic->SetNumberField(TEXT("active_world_sounds"), Snapshot.ActiveWorldSounds);
                Diagnostic->SetNumberField(TEXT("active_mixer_sources"), Snapshot.ActiveMixerSources);
                Diagnostic->SetStringField(TEXT("listener_positions"), Snapshot.ListenerPositions);
                Diagnostic->SetStringField(TEXT("sound_details"), Snapshot.SoundDetails);
                Diagnostic->SetStringField(TEXT("expected_cue_path"), Snapshot.ExpectedCuePath);
                Diagnostic->SetNumberField(TEXT("expected_cue_count"), Snapshot.ExpectedCueCount);
                Diagnostic->SetNumberField(TEXT("audio_clock"), Snapshot.AudioClock);
                Diagnostic->SetNumberField(TEXT("expected_cue_playback_age"), Snapshot.ExpectedCuePlaybackTime);
                Diagnostic->SetNumberField(TEXT("expected_cue_playback_age_unscaled"), Snapshot.ExpectedCuePlaybackTimeUnscaled);
                FocusedAudioDiagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));
                FocusedMaxWorldSounds.FindOrAdd(Snapshot.Label) = FMath::Max(FocusedMaxWorldSounds.FindRef(Snapshot.Label), Snapshot.ActiveWorldSounds);
                for (const TSharedPtr<FJsonValue>& Entry : Records)
                {
                    if (Entry->AsObject()->GetStringField(TEXT("case")) == Snapshot.Label) Entry->AsObject()->SetNumberField(TEXT("max_active_world_sounds_observed"), FocusedMaxWorldSounds.FindRef(Snapshot.Label));
                }
            }
            Test->AddInfo(FString::Printf(TEXT("AudioReview %s case=%d device=%u focused=%d appVolume=%.3f unfocused=%.3f transient=%.3f primary=%.3f muted=%d activeWorldSounds=%d mixerSources=%d worldListeners=%d listeners=[%s] sounds=[%s]. This diagnoses the live mix and does not assert individual audible quality."), *Snapshot.Label, Snapshot.CaseIndex + 1, Snapshot.DeviceId, Snapshot.bAppFocused, Snapshot.AppVolume, Snapshot.UnfocusedVolume, Snapshot.TransientVolume, Snapshot.PrimaryVolume, Snapshot.bDeviceMuted, Snapshot.ActiveWorldSounds, Snapshot.ActiveMixerSources, Snapshot.WorldListeners, *Snapshot.ListenerPositions, *Snapshot.SoundDetails));
        }
    }

    bool ObserveMonsterDeathAndRestart()
    {
        ++WarmFrames;
        if (WarmFrames < 5 || FPlatformTime::Seconds() - StageStarted < 0.15) return false;
        if (!Check(!Source->IsUnitAlive() && Source->GetAttributeSet()->GetHP() <= 0.f && Source->GetMesh()->IsSimulatingPhysics() && Source->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Source->GetCurrentTile() && DeathHomeTile.IsValid() && !DeathHomeTile->GetOccupyingUnit(), TEXT("Lethal GAS damage applies actual monster ragdoll, disables capsule collision and releases its reserved tile."))) return End();
        if (bCaptureFailed) return End();
        const double DeathWorldSeconds = Controller->GetWorld()->GetTimeSeconds() - DeathStartedWorldTime;
        if (bSettlingReview && bLateDeathCaptureQueued && !bSettlingDeathCaptureQueued && !ObserveSettlingRagdollFrame(DeathWorldSeconds)) return End();
        if (!bDeathCaptureQueued)
        {
            bDeathCaptureQueued = QueueCapture(TEXT("death_ragdoll"), static_cast<float>(FPlatformTime::Seconds() - StageStarted));
            return false;
        }
        if (!PendingScreenshot.IsEmpty()) return false;
        if (!Check(Records.Num() == CaseIndex + 1 && CapturedThisCase >= 4, TEXT("The original monster supplies all three attack/return frames and its initial actual death frame."))) return End();
        if (!bLateDeathCaptureQueued)
        {
            const double ElapsedWorldSeconds = Controller->GetWorld()->GetTimeSeconds() - DeathStartedWorldTime;
            if (ElapsedWorldSeconds < 2.5) return false;
            USkeletalMeshComponent* Mesh = Source->GetMesh();
            const UPhysicsAsset* Physics = Mesh->GetPhysicsAsset();
            bool bFiniteBodies = Physics && !Mesh->GetComponentTransform().ContainsNaN() && !Mesh->Bounds.Origin.ContainsNaN() && !Mesh->Bounds.BoxExtent.ContainsNaN();
            float MaxLinearSpeed = 0.f;
            float MaxAngularSpeed = 0.f;
            int32 ObservedBodies = 0;
            if (Physics)
            {
                for (const USkeletalBodySetup* Body : Physics->SkeletalBodySetups)
                {
                    if (!Body) continue;
                    const FVector Linear = Mesh->GetPhysicsLinearVelocity(Body->BoneName);
                    const FVector Angular = Mesh->GetPhysicsAngularVelocityInRadians(Body->BoneName);
                    bFiniteBodies &= !Linear.ContainsNaN() && !Angular.ContainsNaN();
                    MaxLinearSpeed = FMath::Max(MaxLinearSpeed, static_cast<float>(Linear.Length()));
                    MaxAngularSpeed = FMath::Max(MaxAngularSpeed, static_cast<float>(Angular.Length()));
                    ++ObservedBodies;
                }
            }
            if (!Check(bFiniteBodies && ObservedBodies > 0 && FMath::IsFinite(MaxLinearSpeed) && FMath::IsFinite(MaxAngularSpeed), TEXT("The late original ragdoll retains finite mesh bounds and physical body velocities without changing physics settings."))) return End();
            const bool bAnyBodyAwake = Mesh->IsAnyRigidBodyAwake();
            TSharedPtr<FJsonObject> Record = Records.Last()->AsObject();
            Record->SetNumberField(TEXT("late_ragdoll_observed_world_seconds"), ElapsedWorldSeconds);
            Record->SetNumberField(TEXT("late_ragdoll_physics_asset_bodies_sampled"), ObservedBodies);
            Record->SetNumberField(TEXT("late_ragdoll_max_linear_speed_cm_per_second"), MaxLinearSpeed);
            Record->SetNumberField(TEXT("late_ragdoll_max_angular_speed_radians_per_second"), MaxAngularSpeed);
            Record->SetBoolField(TEXT("late_ragdoll_any_body_awake"), bAnyBodyAwake);
            Record->SetBoolField(TEXT("late_ragdoll_all_bodies_asleep_observed"), !bAnyBodyAwake);
            Record->SetNumberField(TEXT("late_ragdoll_mesh_bounds_min_z_minus_tile_floor_z"), Mesh->Bounds.Origin.Z - Mesh->Bounds.BoxExtent.Z - FloorZ);
            Record->SetStringField(TEXT("late_ragdoll_observation_scope"), TEXT("One actual 2.5-world-second sample and viewport PNG; awake bodies and measured velocities are retained. Mesh bounds relative to the tile floor are an observation, not a penetration or final-settlement verdict. Original physics, HP policy and assets remain unchanged."));
            // Observe the original simulation after real world time; do not force sleep or treat a still-moving body as settled.
            // 실제 월드 시간 뒤 원본 시뮬레이션을 관찰하며 강제로 재우거나 움직이는 몸체를 안착으로 처리하지 않습니다.
            Test->AddInfo(FString::Printf(TEXT("%s late ragdoll: worldSeconds=%.3f physicsBodies=%d anyAwake=%d maxLinearCmPerSecond=%.3f maxAngularRadiansPerSecond=%.3f meshBoundsMinZMinusTileFloor=%.3f."), *Cases[CaseIndex].Label, ElapsedWorldSeconds, ObservedBodies, bAnyBodyAwake, MaxLinearSpeed, MaxAngularSpeed, Mesh->Bounds.Origin.Z - Mesh->Bounds.BoxExtent.Z - FloorZ));
            bLateDeathCaptureQueued = QueueCapture(TEXT("death_ragdoll_world250"), static_cast<float>(ElapsedWorldSeconds));
            return false;
        }
        if (bSettlingReview && !bSettlingDeathCaptureQueued)
        {
            if (DeathWorldSeconds < 8.0) return false;
            // Capture the naturally simulated eight-second sample without imposing engine sleep or changing original physics.
            // 엔진 수면을 강제하거나 원본 물리를 바꾸지 않고 자연 시뮬레이션의 8초 표본을 캡처합니다.
            bSettlingDeathCaptureQueued = QueueCapture(TEXT("death_ragdoll_world800"), static_cast<float>(DeathWorldSeconds));
            return false;
        }
        if (!Check(CapturedThisCase == (bSettlingReview ? 6 : 5), bSettlingReview ? TEXT("The original monster supplies both its preserved 2.5-second frame and optional actual eight-second frame before Restart.") : TEXT("The original monster also supplies its actual late ragdoll frame before Restart."))) return End();
        RestoreMovedActors();
        FixtureMonster->Destroy();
        FixtureMonster.Reset();
        if (!Check(Mode->RestartCombat(), TEXT("The public Restart restores the saved debug encounter after actual monster death."))) return End();
        const TArray<AUnitBase*>& Restored = Mode->GetCombatManager()->GetRegisteredUnits();
        TArray<FString> RestoredEnemyClasses;
        int32 AliveHumans = 0;
        bool bResourcesRestored = true;
        for (AUnitBase* Unit : Restored)
        {
            if (!IsValid(Unit) || !Unit->GetAttributeSet() || !Unit->GetMesh())
            {
                bResourcesRestored = false;
                continue;
            }
            if (Unit->GetTeam() == ETeam::Enemy) RestoredEnemyClasses.Add(Unit->GetClass()->GetPathName());
            else ++AliveHumans;
            bResourcesRestored &= Unit->IsUnitAlive() && !Unit->GetMesh()->IsSimulatingPhysics() && Unit->GetAttributeSet()->GetHP() == Unit->GetAttributeSet()->GetMaxHP() && Unit->GetCurrentActionPoint() == Unit->GetMaxActionPoint() && Unit->GetCurrentSubActionPoint() == Unit->GetMaxSubActionPoint() && Unit->GetCurrentTile() && Unit->GetCurrentTile()->GetOccupyingUnit() == Unit;
        }
        RestoredEnemyClasses.Sort();
        if (!Check(Restored.Num() == 5 && AliveHumans == 1 && RestoredEnemyClasses == DefaultEnemyClasses && bResourcesRestored && Controller->GetRoundCoordinator()->GetView().Phase == ECombatRoundPhase::Planning, TEXT("Restart restores the original four enemy classes, living human, full HP/AP/SAP, occupied home tiles and planning round."))) return End();
        TSharedPtr<FJsonObject> Record = Records.Last()->AsObject();
        Record->SetNumberField(TEXT("captures"), CapturedThisCase);
        Record->SetArrayField(TEXT("capture_phases"), CaseCaptureRecords);
        Record->SetBoolField(TEXT("actual_gas_death_ragdoll_tile_release"), true);
        Record->SetBoolField(TEXT("restart_restores_saved_default_roster"), true);
        Record->SetBoolField(TEXT("passed_runtime_contract"), true);
        Test->AddInfo(FString::Printf(TEXT("Completed %s death/restart: PNG=%d; initial and late actual ragdoll, tile release; original four enemy classes and full resources restored. Engine sleep is a separately recorded observation."), *Cases[CaseIndex].Label, CapturedThisCase));
        ++CaseIndex;
        if (CaseIndex == Cases.Num())
        {
            Check(CompletedCaptures == (bSettlingReview ? 78 : 65), bSettlingReview ? TEXT("All thirteen originals preserve their five frames and add an actual eight-second ragdoll frame without modifying physics.") : TEXT("All thirteen original monsters retain four initial attack/return/death captures and add one actual late ragdoll capture."));
            return End();
        }
        Advance(1);
        return false;
    }

    bool ObserveSettlingRagdollFrame(double ElapsedWorldSeconds)
    {
        if (ElapsedWorldSeconds <= LastSettlingObservedWorldTime) return true;
        USkeletalMeshComponent* Mesh = Source->GetMesh();
        const UPhysicsAsset* Physics = Mesh->GetPhysicsAsset();
        bool bFiniteBodies = Physics && !Mesh->GetComponentTransform().ContainsNaN() && !Mesh->Bounds.Origin.ContainsNaN() && !Mesh->Bounds.BoxExtent.ContainsNaN();
        float MaxLinearSpeed = 0.f;
        float MaxAngularSpeed = 0.f;
        int32 ObservedBodies = 0;
        if (Physics)
        {
            for (const USkeletalBodySetup* Body : Physics->SkeletalBodySetups)
            {
                if (!Body) continue;
                const FVector Linear = Mesh->GetPhysicsLinearVelocity(Body->BoneName);
                const FVector Angular = Mesh->GetPhysicsAngularVelocityInRadians(Body->BoneName);
                bFiniteBodies &= !Linear.ContainsNaN() && !Angular.ContainsNaN();
                MaxLinearSpeed = FMath::Max(MaxLinearSpeed, static_cast<float>(Linear.Length()));
                MaxAngularSpeed = FMath::Max(MaxAngularSpeed, static_cast<float>(Angular.Length()));
                ++ObservedBodies;
            }
        }
        if (!Check(bFiniteBodies && ObservedBodies > 0 && FMath::IsFinite(MaxLinearSpeed) && FMath::IsFinite(MaxAngularSpeed), TEXT("Every observed original ragdoll frame from 2.5 to eight world seconds keeps finite bounds and physical velocities."))) return false;
        LastSettlingObservedWorldTime = ElapsedWorldSeconds;
        const bool bAnyBodyAwake = Mesh->IsAnyRigidBodyAwake();
        const double BoundsMinZMinusFloor = Mesh->Bounds.Origin.Z - Mesh->Bounds.BoxExtent.Z - FloorZ;
        TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("world_seconds_since_death"), ElapsedWorldSeconds);
        Sample->SetBoolField(TEXT("any_body_awake"), bAnyBodyAwake);
        Sample->SetBoolField(TEXT("all_bodies_asleep_observed"), !bAnyBodyAwake);
        Sample->SetNumberField(TEXT("physics_asset_bodies_sampled"), ObservedBodies);
        Sample->SetNumberField(TEXT("max_linear_speed_cm_per_second"), MaxLinearSpeed);
        Sample->SetNumberField(TEXT("max_angular_speed_radians_per_second"), MaxAngularSpeed);
        Sample->SetNumberField(TEXT("mesh_bounds_min_z_minus_tile_floor_z"), BoundsMinZMinusFloor);
        SettlingFrameSamples.Add(MakeShared<FJsonValueObject>(Sample));
        Records.Last()->AsObject()->SetArrayField(TEXT("settling_ragdoll_frame_samples"), SettlingFrameSamples);
        if (ElapsedWorldSeconds >= 8.0)
        {
            TSharedPtr<FJsonObject> Record = Records.Last()->AsObject();
            Record->SetNumberField(TEXT("settling_ragdoll_observed_world_seconds"), ElapsedWorldSeconds);
            Record->SetNumberField(TEXT("settling_ragdoll_physics_asset_bodies_sampled"), ObservedBodies);
            Record->SetNumberField(TEXT("settling_ragdoll_max_linear_speed_cm_per_second"), MaxLinearSpeed);
            Record->SetNumberField(TEXT("settling_ragdoll_max_angular_speed_radians_per_second"), MaxAngularSpeed);
            Record->SetBoolField(TEXT("settling_ragdoll_any_body_awake"), bAnyBodyAwake);
            Record->SetBoolField(TEXT("settling_ragdoll_all_bodies_asleep_observed"), !bAnyBodyAwake);
            Record->SetNumberField(TEXT("settling_ragdoll_mesh_bounds_min_z_minus_tile_floor_z"), BoundsMinZMinusFloor);
            Record->SetArrayField(TEXT("settling_ragdoll_frame_samples"), SettlingFrameSamples);
            Record->SetStringField(TEXT("settling_ragdoll_status"), bAnyBodyAwake ? TEXT("Engine bodies remain awake at eight world seconds; final settling is unconfirmed even when combat/restart contracts pass.") : TEXT("Engine sleep observed at eight world seconds; viewport review is still required for actual pose and floor contact."));
            Test->AddInfo(FString::Printf(TEXT("%s eight-second ragdoll: worldSeconds=%.3f frames=%d physicsBodies=%d anyAwake=%d maxLinearCmPerSecond=%.3f maxAngularRadiansPerSecond=%.3f meshBoundsMinZMinusTileFloor=%.3f. Awake state is an observation, not a successful settling verdict."), *Cases[CaseIndex].Label, ElapsedWorldSeconds, SettlingFrameSamples.Num(), ObservedBodies, bAnyBodyAwake, MaxLinearSpeed, MaxAngularSpeed, BoundsMinZMinusFloor));
        }
        return true;
    }

    bool ExpandCatalog()
    {
        if (bMonsterReview)
        {
            TArray<FName> Ids;
            TArray<FText> Names;
            Mode->GetDebugSpawnOptions(true, Ids, Names);
            if (!Check(Ids.Num() == 13 && Names.Num() == Ids.Num(), TEXT("The official debug catalog resolves twelve monster bodies and the retained skeleton."))) return false;
            for (AUnitBase* Unit : Mode->GetCombatManager()->GetRegisteredUnits())
            {
                if (IsValid(Unit) && Unit->GetTeam() == ETeam::Enemy) DefaultEnemyClasses.Add(Unit->GetClass()->GetPathName());
            }
            DefaultEnemyClasses.Sort();
            if (!Check(DefaultEnemyClasses.Num() == 4, TEXT("The saved baseline contains exactly four original enemy classes for restart comparison."))) return false;
            TSet<FName> Unique;
            for (FName Id : Ids)
            {
                if (!Check(!Unique.Contains(Id), TEXT("Each catalog monster class is reviewed once."))) return false;
                Unique.Add(Id);
                FCase& Added = Cases.AddDefaulted_GetRef();
                Added.Label = TEXT("monster_") + FPaths::GetCleanFilename(Id.ToString()).Replace(TEXT("."), TEXT("_"));
                Added.bMonster = true;
                Added.bBasicAttack = true;
                Added.MonsterClass = Id.ToString();
            }
        }
        else
        {
            UCombatDebugLoadout* Catalog = Controller->GetDebugLoadout();
            if (!Check(Catalog != nullptr, TEXT("The official live debug skill catalog is available."))) return false;
            TSet<FString> Included;
            for (const FCase& Existing : Cases) Included.Add(Existing.Asset);
            TSet<FSoftObjectPath> UniqueDrGame;
            int32 AddedCount = 0;
            for (const FSoftObjectPath& Asset : Catalog->GetSkillAssets())
            {
                const FString Package = Asset.GetLongPackageName();
                if (!Package.StartsWith(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/"))) continue;
                if (!Check(!UniqueDrGame.Contains(Asset), TEXT("Official DrGame skill IDs are unique."))) return false;
                UniqueDrGame.Add(Asset);
                if (Included.Contains(Package) || bFocusedReview) continue;
                const USkillDefinitionDataAsset* Data = Cast<USkillDefinitionDataAsset>(Asset.TryLoad());
                FCombatRoundSkill Profile;
                FText Error;
                if (!Check(Data && Data->ResolveRoundSkill(Profile, Error), TEXT("Resolve the original remaining skill: ") + Package + TEXT("; ") + Error.ToString())) return false;
                FCase& Added = Cases.AddDefaulted_GetRef();
                Added.Label = TEXT("catalog_") + Asset.GetAssetName();
                Added.Asset = Package;
                Included.Add(Package);
                ++AddedCount;
            }
            if (!Check(UniqueDrGame.Num() == 60, TEXT("The original official DrGame catalog still contains all sixty distinct authored assets."))) return false;
            if (bFocusedReview)
            {
                if (!Check(Cases.Num() == (bNinjaVisibilityReview ? 3 : 7) && AddedCount == 0, bNinjaVisibilityReview ? TEXT("The explicit Ninja flag selects its unchanged baseline and two unsaved height-only comparisons.") : TEXT("The explicit focused flag selects four original PoisonCarousel conditions and three original visibility profiles."))) return false;
                for (const FCase& Current : Cases)
                {
                    if (!Check(UniqueDrGame.Contains(FSoftObjectPath(Current.Asset + TEXT(".") + FPaths::GetCleanFilename(Current.Asset))), TEXT("The focused fixture references the unchanged official authored asset: ") + Current.Asset)) return false;
                }
            }
            else if (!Check(AddedCount == 48 && Cases.Num() == 87, TEXT("Preserve the approved 39 casts and add exactly the 48 uncovered official DrGame assets."))) return false;
        }
        PlannedCaptures = bMonsterReview ? (bSettlingReview ? 78 : 65) : 0;
        if (!bMonsterReview)
        {
            for (const FCase& Current : Cases)
            {
                const USkillDefinitionDataAsset* Data = LoadObject<USkillDefinitionDataAsset>(nullptr, *Current.Asset);
                FCombatRoundSkill Profile;
                FText Error;
                if (!Check(Data && Data->ResolveRoundSkill(Profile, Error), TEXT("Resolve the original per-case capture plan: ") + Current.Asset + TEXT("; ") + Error.ToString())) return false;
                PlannedCaptures += CapturePlanCount(Current, Profile);
            }
            Test->AddInfo(FString::Printf(TEXT("Rendered capture plan: %d casts, %d PNG; preserve early observations, add active main phases at age 0.3/0.6 for target-centered effects and a real impact frame where authored."), Cases.Num(), PlannedCaptures));
        }
        bCatalogExpanded = true;
        return true;
    }

    bool Connect()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || World->GetNetMode() != NM_Standalone) continue;
            Mode = World->GetAuthGameMode<ACombatDebugGameMode>();
            Controller = Cast<ACombatDebugPlayerController>(World->GetFirstPlayerController());
            if (!Mode || !Controller || !Controller->IsLocalController() || !Controller->GetRoundCoordinator() || Controller->GetRoundCoordinator()->GetView().Phase != ECombatRoundPhase::Planning) continue;
            const URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
            return Check(!Run || (!Run->IsManagedRun() && !Run->HasManagedLease()), TEXT("The rendering fixture is isolated from persistent Run and lease state."));
        }
        return false;
    }

    bool ConfigureUnit(AUnitBase* Unit, const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills, float ObservationMaxHP = 0.f)
    {
        const UAS_Unit* Attributes = Unit ? Unit->GetAttributeSet() : nullptr;
        return Attributes && Unit->ConfigureProfession(FMath::Max(Attributes->GetMaxHP(), ObservationMaxHP), Unit->GetMaxActionPoint(), Unit->GetMaxSubActionPoint(), Skills, Attributes->GetStrength(), Attributes->GetDexterity(), Attributes->GetIntelligence());
    }

    void MoveFixtureActor(AActor* Actor, const FTransform& Transform)
    {
        MovedActors.Add({Actor, Actor->GetActorTransform()});
        Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
    }

    bool PrepareMonsterCase()
    {
        FCase& Current = Cases[CaseIndex];
        ACombatManager* Manager = Mode->GetCombatManager();
        if (!Check(Manager && Manager->GetRegisteredUnits().Num() == 5, TEXT("Each original monster fixture starts from the saved debug roster."))) return false;
        AUnitBase* Human = nullptr;
        ACombatGridTile* EnemyTile = nullptr;
        TArray<AUnitBase*> PreviousEnemies;
        for (const FCombatRoundUnitView& Entry : Controller->GetRoundCoordinator()->GetView().Units)
        {
            if (Entry.bEnemy)
            {
                PreviousEnemies.Add(Entry.Unit);
                if (!EnemyTile) EnemyTile = Entry.Unit->GetCurrentTile();
            }
            else Human = Entry.Unit;
        }
        if (!Check(Human && EnemyTile && PreviousEnemies.Num() == 4, TEXT("The real arena supplies its original human and a registered enemy home tile."))) return false;
        Controller->SetCombatContext(nullptr, false);
        Manager->ResetCombat();
        for (AUnitBase* Enemy : PreviousEnemies) Enemy->Destroy();
        UClass* OriginalClass = LoadClass<AUnitBase>(nullptr, *Current.MonsterClass);
        if (!Check(OriginalClass && !OriginalClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated), TEXT("Load the original class from the official monster catalog without copying an asset."))) return false;
        FActorSpawnParameters Spawn;
        Spawn.Owner = Mode;
        Spawn.ObjectFlags |= RF_Transient;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        FixtureMonster = Controller->GetWorld()->SpawnActor<AUnitBase>(OriginalClass, EnemyTile->GetActorLocation() + FVector(0, 0, 100), FRotator(0, -90, 0), Spawn);
        Source = FixtureMonster.Get();
        Target = Human;
        if (!Check(Source && Source->GetMesh() && Source->GetMesh()->GetSkeletalMeshAsset() && Source->GetMesh()->GetAnimInstance() && Source->GetEquippedSkillDataAssets().Num() == 1, TEXT("The original spawned monster retains its authored mesh, animation instance and single attack."))) return false;
        Source->SetTeam(ETeam::Enemy);
        Source->SetCurrentTile(EnemyTile);
        Source->SetActorLocation(EnemyTile->GetActorLocation() + FVector(0, 0, Source->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        AuthoredSkill.Reset(Source->GetEquippedSkillDataAssets()[0]);
        FText Error;
        if (!Check(AuthoredSkill.IsValid() && AuthoredSkill->ResolveRoundSkill(Definition, Error), TEXT("Resolve the actual monster's original equipped attack: ") + Error.ToString())) return false;
        Current.Asset = AuthoredSkill->GetOutermost()->GetName();
        OriginalAuthoredDefinition = AuthoredSkill->RoundDefinition;
        OriginalChainSettings = Definition.Chain;
        Prototype.Reset();
        if (!Check(ConfigureUnit(Human, {}, Definition.Power * 4.f + 100.f), TEXT("The sole human uses a real empty command with transient observation HP; original enemy AI is preserved."))) return false;
        Manager->RegisterUnits({Human, Source});
        Controller->SetCombatContext(Manager, false);
        Manager->StartCombat_Internal();
        Controller->SetCombatContext(Manager, true);
        if (!Check(Manager->IsCombatActive() && Controller->GetRoundCoordinator() && Controller->GetRoundCoordinator()->GetView().Phase == ECombatRoundPhase::Planning, TEXT("The real manager registers the original monster and owns its automatic enemy plan."))) return false;
        AActor* Camera = Controller->GetViewTarget();
        if (!Check(IsValid(Camera), TEXT("The original gameplay camera renders the monster attack fixture."))) return false;
        const FVector Focus = (Source->GetActorLocation() + Human->GetActorLocation()) * 0.5;
        const FVector CameraPosition = Focus + FVector(-800, -1000, 1000);
        MoveFixtureActor(Camera, FTransform((Focus - CameraPosition).Rotation(), CameraPosition, Camera->GetActorScale3D()));
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        if (!Check(Window.IsValid(), TEXT("The original monster fixture retains a live Slate render window."))) return false;
        Window->Resize(FVector2D(1280, 720));
        ReviewWindow = Window;
        ResizeAttempts = 0;
        InitialHP.Reset();
        InitialHP.Add(Human, Human->GetAttributeSet()->GetHP());
        InitialHP.Add(Source, Source->GetAttributeSet()->GetHP());
        InitialShield = Human->GetAttributeSet()->GetShield();
        InitialAP = Source->GetCurrentActionPoint();
        InitialRound = Controller->GetRoundCoordinator()->GetView().RoundNumber;
        FloorZ = EnemyTile->GetActorLocation().Z;
        OriginalMonsterPosition = Source->GetActorLocation();
        OriginalMonsterRotation = Source->GetActorQuat();
        ExpectedMonsterMontage = Source->ResolveRoundCastMontage(Definition.CastMontage);
        Enemies = {Source};
        ResetLiveObservations();
        Test->AddInfo(FString::Printf(TEXT("Original monster case %d/%d: class=%s; skill=%s; mesh=%s; montage=%s; power=%.2f; AP=%d; footZ=%.2f; floorZ=%.2f."), CaseIndex + 1, Cases.Num(), *Current.MonsterClass, *Current.Asset, *Source->GetMesh()->GetSkeletalMeshAsset()->GetPathName(), Definition.CastMontage ? *Definition.CastMontage->GetPathName() : TEXT("none"), Definition.Power, Definition.ActionPointCost, Source->GetActorLocation().Z - Source->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), FloorZ));
        return true;
    }

    bool PrepareCase()
    {
        const FCase& Current = Cases[CaseIndex];
        NinjaVisibilityDiagnostics.Reset();
        LastNinjaDiagnosticAge = -1.f;
        if (Current.bMonster) return PrepareMonsterCase();
        ACombatManager* Manager = Mode->GetCombatManager();
        if (!Check(IsValid(Manager) && Manager->GetRegisteredUnits().Num() == 5 && Manager->GetCombatGrid(), TEXT("The saved debug map provides one ally, four enemies and its real grid."))) return false;
        const TArray<AUnitBase*> Units = Manager->GetRegisteredUnits();
        Source = nullptr;
        Enemies.Reset();
        for (const FCombatRoundUnitView& Entry : Controller->GetRoundCoordinator()->GetView().Units)
        {
            if (!Entry.bEnemy) Source = Entry.Unit;
            else Enemies.Add(Entry.Unit);
        }
        if (!Check(Source && Enemies.Num() == 4, TEXT("The live roster resolves the original caster and all four candidate bodies."))) return false;
        const FString AssetPath = Current.Asset + TEXT(".") + FPaths::GetCleanFilename(Current.Asset);
        AuthoredSkill.Reset(LoadObject<USkillDefinitionDataAsset>(nullptr, *AssetPath));
        FText Error;
        if (!Check(AuthoredSkill.IsValid() && AuthoredSkill->ResolveRoundSkill(Definition, Error), TEXT("Resolve the existing authored profile: ") + Current.Label + TEXT("; ") + Error.ToString())) return false;
        OriginalAuthoredDefinition = AuthoredSkill->RoundDefinition;
        OriginalChainSettings = Definition.Chain;
        Controller->SetCombatContext(nullptr, false);
        Manager->ResetCombat();
        USkillDefinitionDataAsset* Equipped = AuthoredSkill.Get();
        Prototype.Reset();
        FCombatRoundSkill NinjaResolvedBaseline;
        if (bNinjaVisibilityReview)
        {
            const UNiagaraSystem* OriginalSystem = Definition.Vfx.Niagara.LoadSynchronous();
            const TOptional<float> Height = OriginalSystem ? OriginalSystem->GetExposedParameters().GetParameterOptionalValue<float>(FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.HeightOffset"))) : TOptional<float>();
            if (!Check(Height.IsSet() && FMath::IsNearlyEqual(Height.GetValue(), -20.f) && Definition.Vfx.RelativeTransform.GetLocation().Equals(FVector(0, 0, -90)) && !Definition.Vfx.FloatParameters.Contains(TEXT("User.HeightOffset")), TEXT("The Ninja comparison starts from the original -20cm source height and unchanged project visual offset, with no pre-existing height override."))) return false;
            OriginalNinjaDefinition = Definition;
            NinjaResolvedBaseline = OriginalNinjaDefinition;
            // Change only an unsaved visual field; the original DataAsset, Niagara system and combat values remain identical.
            // 저장하지 않는 시각 필드만 변경하며 원본 DataAsset·Niagara 시스템·전투 수치는 동일하게 보존합니다.
            if (Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::VisualHeight30) Definition.Vfx.RelativeTransform.AddToTranslation(FVector(0, 0, 30));
            else if (Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::SourceHeight20) Definition.Vfx.FloatParameters.Add(TEXT("User.HeightOffset"), 20.f);
        }
        if (Current.bChain || (bNinjaVisibilityReview && Current.NinjaVisibilityCondition != ENinjaVisibilityCondition::Baseline))
        {
            if (Current.bChain)
            {
                if (!Check(CombatRoundRules::UsesChain(Definition) && Definition.Chain.MaxTargets == 1, TEXT("Authored chain data retains the pending single-target default."))) return false;
                // Prototype values are scoped to an unsaved object in this disposable PIE; no content balance choice is applied.
                // 임시 수치는 저장하지 않는 일회성 PIE 객체에만 적용하며 콘텐츠 밸런스 선택으로 반영하지 않습니다.
                Definition.Chain.MaxTargets = 3;
                Definition.Chain.JumpDistance = 400.f;
                Definition.Chain.JumpIntervalSeconds = 0.4f;
                Definition.Chain.DamageMultiplierPerJump = 1.f;
            }
            Prototype.Reset(NewObject<USkillDefinitionDataAsset>(GetTransientPackage(), NAME_None, RF_Transient));
            Prototype->AbilityClass = AuthoredSkill->AbilityClass;
            Prototype->SkillName = AuthoredSkill->SkillName;
            Prototype->bUseRoundDefinition = true;
            Prototype->RoundDefinition = Definition;
            Equipped = Prototype.Get();
            if (bNinjaVisibilityReview)
            {
                const FCombatRoundSkill ModifiedInput = Prototype->RoundDefinition;
                FCombatRoundSkill RestoredInput = ModifiedInput;
                if (Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::VisualHeight30) RestoredInput.Vfx.RelativeTransform = OriginalNinjaDefinition.Vfx.RelativeTransform;
                else if (Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::SourceHeight20) RestoredInput.Vfx.FloatParameters.Remove(TEXT("User.HeightOffset"));
                if (!Check(FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&RestoredInput, &OriginalNinjaDefinition, 0), TEXT("Reversing the one Ninja height field restores the complete copied input profile before resolution."))) return false;
                // Resolve both profiles through the same transient object so its official primary asset identity is identical.
                // 공식 기본 에셋 식별자를 동일하게 유지하도록 두 프로필을 같은 임시 객체로 해석합니다.
                Prototype->RoundDefinition = OriginalNinjaDefinition;
                const bool bBaselineResolved = Prototype->ResolveRoundSkill(NinjaResolvedBaseline, Error);
                Prototype->RoundDefinition = ModifiedInput;
                if (!Check(bBaselineResolved, TEXT("The unchanged baseline resolves through the identical unsaved observation object."))) return false;
                Test->AddInfo(FString::Printf(TEXT("Ninja profile comparison: sourceResolvedId=%s; transientBaselineId=%s; transientPrimaryId=%s. Input and resolved profiles are compared independently with only the one visual field reversed."), *OriginalNinjaDefinition.SkillId.ToString(), *NinjaResolvedBaseline.SkillId.ToString(), *Prototype->GetPrimaryAssetId().ToString()));
            }
            if (!Check(Equipped->ResolveRoundSkill(Definition, Error), TEXT("The unsaved observation prototype passes the same official skill validation."))) return false;
        }
        if (bNinjaVisibilityReview)
        {
            FCombatRoundSkill Restored = Definition;
            if (Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::VisualHeight30) Restored.Vfx.RelativeTransform = OriginalNinjaDefinition.Vfx.RelativeTransform;
            else if (Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::SourceHeight20) Restored.Vfx.FloatParameters.Remove(TEXT("User.HeightOffset"));
            if (!Check(FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Restored, &NinjaResolvedBaseline, 0), TEXT("Reversing the one transient Ninja height field restores every resolved baseline skill, visual, tag, timing, AP and GAS field exactly."))) return false;
        }
        if (!Check(ConfigureUnit(Source, {Equipped}), TEXT("The official profession API equips the authored or transient profile on the original caster."))) return false;
        for (AUnitBase* Enemy : Enemies)
        {
            if (!Check(ConfigureUnit(Enemy, {}, Current.bChain ? Definition.Power * 4.f + 100.f : 0.f), TEXT("Passive observation enemies keep the original bodies and non-HP attributes; chain-only transient HP prevents premature corpse visuals."))) return false;
        }
        ACombatGridManager* Grid = Manager->GetCombatGrid();
        const FVector LargeOffset = Current.bLargeWorld ? FVector(FLargeWorldRenderScalar::GetTileSize() * 2.0, 0.0, 0.0) : FVector::ZeroVector;
        if (Current.bLargeWorld)
        {
            for (const auto& Pair : Grid->TileMap)
            {
                if (IsValid(Pair.Value)) MoveFixtureActor(Pair.Value, FTransform(Pair.Value->GetActorQuat(), Pair.Value->GetActorLocation() + LargeOffset, Pair.Value->GetActorScale3D()));
            }
            MoveFixtureActor(Grid, FTransform(Grid->GetActorQuat(), Grid->GetActorLocation() + LargeOffset, Grid->GetActorScale3D()));
            for (AUnitBase* Unit : Units) Unit->AddActorWorldOffset(LargeOffset, false, nullptr, ETeleportType::TeleportPhysics);
        }
        FloorZ = Source->GetCurrentTile()->GetActorLocation().Z;
        if (Current.bCardinal)
        {
            const FVector Center = Grid->GetActorLocation() + FVector(300, 300, 0);
            const FVector Side(-Current.Direction.Y, Current.Direction.X, 0);
            Source->SetActorLocation(FVector(Center.X, Center.Y, FloorZ + Source->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
            for (int32 Index = 0; Index < Enemies.Num(); ++Index)
            {
                const FVector Offset = Index == 0 ? Current.Direction * 230.0 : Index == 1 ? Current.Direction * 430.0 + Side * 100.0 : Index == 2 ? Current.Direction * 630.0 - Side * 80.0 : -Current.Direction * 900.0;
                Enemies[Index]->SetActorLocation(Center + Offset + FVector(0, 0, Enemies[Index]->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
            }
        }
        if (Current.bLargeWorld)
        {
            FActorSpawnParameters Spawn;
            Spawn.ObjectFlags |= RF_Transient;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            LargeWorldFloor = Controller->GetWorld()->SpawnActor<AStaticMeshActor>(Grid->GetActorLocation() + FVector(300, 300, -5), FRotator::ZeroRotator, Spawn);
            if (!Check(LargeWorldFloor.IsValid(), TEXT("The large-coordinate fixture has an unsaved floor actor for visual context."))) return false;
            UStaticMeshComponent* Mesh = LargeWorldFloor->GetStaticMeshComponent();
            Mesh->SetMobility(EComponentMobility::Movable);
            if (!Check(Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"))), TEXT("The large-coordinate floor references the original engine plane without a copied asset."))) return false;
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetCanEverAffectNavigation(false);
            LargeWorldFloor->SetActorScale3D(FVector(30, 30, 1));
        }
        // Re-register existing actors through the production manager so ownership, GAS and action cost use the normal path.
        // 소유권·GAS·행동 비용이 정상 경로를 사용하도록 실제 매니저에 기존 액터를 다시 등록합니다.
        Manager->RegisterUnits(Units);
        Controller->SetCombatContext(Manager, false);
        Manager->StartCombat_Internal();
        Controller->SetCombatContext(Manager, true);
        if (!Check(Manager->IsCombatActive() && Controller->GetRoundCoordinator() && Controller->GetRoundCoordinator()->GetView().Phase == ECombatRoundPhase::Planning, TEXT("The official manager initializes an owned planning round for the geometry fixture."))) return false;
        if (Current.bLargeWorld)
        {
            for (AUnitBase* Unit : Units)
            {
                const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
                if (!Check(Movement && Movement->MovementMode == MOVE_None && !Movement->IsComponentTickEnabled(), TEXT("The actual coordinator disables gravity and movement ticks while the LWC fixture uses an observation-only floor."))) return false;
            }
        }
        AActor* Camera = Controller->GetViewTarget();
        if (!Check(IsValid(Camera), TEXT("The actual gameplay camera remains the rendering view target."))) return false;
        const FVector Focus = Current.bCardinal ? (Source->GetActorLocation() + Enemies[2]->GetActorLocation()) * 0.5 : (Source->GetActorLocation() + Enemies[Current.TargetIndex]->GetActorLocation()) * 0.5;
        const FVector CameraPosition = Focus + FVector(-1000, -1200, 1200);
        MoveFixtureActor(Camera, FTransform((Focus - CameraPosition).Rotation(), CameraPosition, Camera->GetActorScale3D()));
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        const TSharedPtr<SWindow> Window = Widget.IsValid() ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        if (!Check(Window.IsValid(), TEXT("The real PIE render viewport belongs to an actual Slate window."))) return false;
        // Restore physical client pixels after initial Slate work-area fitting, then warm real render frames before capture.
        // Slate의 초기 작업 영역 맞춤 후 실제 클라이언트 픽셀 크기를 복원하고 캡처 전에 실제 렌더 프레임을 준비합니다.
        Window->Resize(FVector2D(1280, 720));
        ReviewWindow = Window;
        ResizeAttempts = 0;
        Test->AddInfo(FString::Printf(TEXT("VFX viewport resize requested: physical=%s; client=%s; local geometry=%s; DPI=%.3f."), *Viewport->Viewport->GetSizeXY().ToString(), *Window->GetClientSizeInScreen().ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString(), Window->GetDPIScaleFactor()));
        Target = Definition.TargetRule == ESkillTargetRule::AllyUnit ? Source : Enemies[Current.TargetIndex];
        if (Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal)) Source->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Source->GetAttributeSet()->GetMaxHP() * 0.5f);
        InitialHP.Reset();
        for (AUnitBase* Unit : Units) InitialHP.Add(Unit, Unit->GetAttributeSet()->GetHP());
        InitialShield = Target->GetAttributeSet()->GetShield();
        InitialAP = Source->GetCurrentActionPoint();
        InitialRound = Controller->GetRoundCoordinator()->GetView().RoundNumber;
        ResetLiveObservations();
        if (!PrepareFocusedView(Camera, Focus)) return false;
        if (!Definition.Vfx.Niagara.IsNull()) Definition.Vfx.Niagara.LoadSynchronous();
        if (!Definition.ImpactVfx.Niagara.IsNull()) Definition.ImpactVfx.Niagara.LoadSynchronous();
        Test->AddInfo(FString::Printf(TEXT("VFX case %d/%d: %s; asset=%s; transient chain=%d/400cm/0.4s/1; source=%d; target=%d; target capsule=%s; floorZ=%.2f."), CaseIndex + 1, Cases.Num(), *Current.Label, *AssetPath, Current.bChain ? 3 : 1, Source->UnitIndex, Target->UnitIndex, *Target->GetCapsuleComponent()->GetComponentLocation().ToString(), FloorZ));
        return true;
    }

    void ResetLiveObservations()
    {
        Segments.Reset();
        ChainActor.Reset();
        ChainHitIds.Reset();
        AppliedEffects = CapturedThisCase = WarmFrames = MaxParticles = MaxImpactParticles = GenericCapturesQueued = 0;
        LateCapturesQueued = 0;
        bFocusedFinalBurstCaptureQueued = false;
        ShortCapturesQueued = 0;
        ShortPhasesConsidered = 0;
        bFocusedViewObserved = bFocusedLODDistanceObserved = false;
        FocusedViewPosition = FVector::ZeroVector;
        FocusedViewTargetDistance = 0.0;
        FocusedMinLODDistance = TNumericLimits<float>::Max();
        FocusedMaxLODDistance = 0.f;
        bImpactCaptureQueued = false;
        CaseCaptureRecords.Reset();
        NinjaVisibilityDiagnostics.Reset();
        LastNinjaDiagnosticAge = -1.f;
        MaxAge = FirstParticleAge = 0.f;
        bCaptureFailed = bSawMontage = bSawActive = bSawReady = bAppliedShield = bOrientationVerified = false;
        ExpectedAimDirection = (Target->GetCapsuleComponent()->GetComponentLocation() - Source->GetCapsuleComponent()->GetComponentLocation()).GetSafeNormal2D();
        bOriginalGasContext = true;
        GasSource = Source;
        GasHandle = Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.AddLambda([this](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
        {
            bOriginalGasContext &= Spec.GetContext().GetOriginalInstigator() == Source && Spec.GetContext().GetSourceObject() == Source;
            ++AppliedEffects;
            bAppliedShield |= IsValid(Target) && Target->GetAttributeSet()->GetShield() > InitialShield;
        });
        bApproached = bReturned = false;
        MonsterCapturesQueued = 0;
        ImpactInstances.Reset();
        PriorImpactComponents.Reset();
        bCenterVerified = false;
        bRecordingAudio = bAudioExportPending = false;
        AudioFilename.Reset();
        AudioBasename.Reset();
        AudioStableFrames = 0;
        AudioLastBytes = -1;
        LastAudioDiagnosticAt = 0.0;
        NaturalCuePath.Reset();
        NaturalCueDuration = 0.f;
        NaturalCueExpectedSeconds = 0.0;
        NaturalCueFirstSeenClock = NaturalCueInferredStartClock = NaturalCueLastActiveClock = NaturalCueFirstAbsentClock = -1.0;
        NaturalCueMaxPlaybackTime = 0.f;
        NaturalCueAbsentSamples = 0;
        bNaturalCueObservationRequired = bNaturalCueSeen = bNaturalCueCompletionObserved = bNaturalCueObservationFailed = false;
    }

    bool ExecuteCase()
    {
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            if (It->GetWorld() == Controller->GetWorld() && It->GetAsset() == Definition.ImpactVfx.Niagara.Get()) PriorImpactComponents.Add(*It);
        }
        ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const FCombatRoundUnitView* Ally = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.Unit == Source; });
        const FCombatRoundUnitView* Victim = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.Unit == Target; });
        if (!Check(Ally && Victim && Ally->SkillIds.Contains(Definition.SkillId), TEXT("The real coordinator resolves caster ownership, target IDs and the equipped definition."))) return false;
        if (Cases[CaseIndex].bMonster)
        {
            if (!Check(Ally->bEnemy && Ally->Command.SkillId == Definition.SkillId && Ally->Command.TargetUnitId == Victim->UnitId && !Victim->bEnemy && Victim->Command.SkillId.IsNone(), TEXT("The original enemy AI chooses its own attack while the sole human uses the normal empty command."))) return false;
            FText ReadyError;
            const FCombatRoundView& View = Round->GetView();
            if (!Check(Round->SetParticipantReady(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, true, ReadyError), TEXT("The human readiness request releases the original monster AI: ") + ReadyError.ToString())) return false;
            return Check(Source->GetCurrentActionPoint() == InitialAP - Definition.ActionPointCost, TEXT("The original monster attack pays its authored AP cost once."));
        }
        FCombatRoundCommand Command;
        Command.UnitId = Ally->UnitId;
        Command.SkillId = Definition.SkillId;
        Command.TargetUnitId = CombatRoundRules::UsesUnitTarget(Definition) ? Victim->UnitId : INDEX_NONE;
        Command.TargetCoord = Victim->HomeCoord;
        Command.DestinationCoord = Ally->HomeCoord;
        FText Error;
        const FCombatRoundView& View = Round->GetView();
        if (!Check(Round->SubmitPlan(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, Command, Error) && Round->SetParticipantReady(Controller, View.CombatId, View.RoundNumber, View.PlanRevision, true, Error), TEXT("Public action request executes the case: ") + Error.ToString())) return false;
        return Check(Source->GetCurrentActionPoint() == InitialAP - Definition.ActionPointCost, TEXT("The owned command charges exactly one authored AP cost at lock."));
    }

    bool ReadWorldEndpoint(UNiagaraComponent* Component, FName Name, ECombatVfxEndpointSpace Space, FVector& OutWorld) const
    {
        const FNiagaraSystemInstanceControllerPtr Instance = Component->GetSystemInstanceController();
        if (!Instance || !Instance->IsValid() || Name.IsNone()) return false;
        bool bValid = false;
        const FVector Vector = Component->GetVariableVec3(Name, bValid);
        const FNiagaraLWCConverter Converter = Instance->GetSystemInstance_Unsafe()->GetLWCConverter(false);
        if (bValid)
        {
            OutWorld = Space == ECombatVfxEndpointSpace::World ? Converter.ConvertSimulationVectorToWorld(FVector3f(Vector)) : Component->GetComponentTransform().TransformPosition(Vector);
            return !OutWorld.ContainsNaN();
        }
        const FVector Position = Component->GetVariablePosition(Name, bValid);
        if (!bValid) return false;
        OutWorld = Space == ECombatVfxEndpointSpace::ComponentLocal ? Component->GetComponentTransform().TransformPosition(FVector(Converter.ConvertWorldToSimulationVector(Position))) : Position;
        return !OutWorld.ContainsNaN();
    }

    bool ObserveChain(UNiagaraComponent* Component, const FNiagaraSystemInstanceControllerPtr& Instance, int32 Particles)
    {
        ACombatChainEffectActor* Owner = Component->GetOwner() ? Cast<ACombatChainEffectActor>(Component->GetOwner()->GetOwner()) : nullptr;
        if (!Owner || Owner->GetOwner() != Controller->GetRoundCoordinator()) return true;
        ChainActor = Owner;
        ChainHitIds = Owner->GetChainRuntimeData().HitUnitIds;
        FObservedSegment* Segment = Segments.FindByPredicate([Component](const FObservedSegment& Existing) { return Existing.Component == Component; });
        if (!Segment)
        {
            int32 Sequence = INDEX_NONE;
            double NearestAnchor = TNumericLimits<double>::Max();
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const FVector Anchor = Index == 0 ? Source->GetCapsuleComponent()->GetComponentLocation() : Enemies[Index - 1]->GetCapsuleComponent()->GetComponentLocation();
                double Distance = FVector::DistSquared(Component->GetComponentLocation(), Anchor);
                const FObservedSegment* Previous = Segments.FindByPredicate([Index](const FObservedSegment& Existing) { return Existing.Sequence == Index - 1; });
                if (Previous) Distance = FMath::Min(Distance, FVector::DistSquared(Component->GetComponentLocation(), Previous->TargetBeforeFixtureMove));
                if (Distance < NearestAnchor)
                {
                    NearestAnchor = Distance;
                    Sequence = Index;
                }
            }
            if (!Check(Sequence >= 0 && Sequence < 3 && !Segments.ContainsByPredicate([Sequence](const FObservedSegment& Existing) { return Existing.Sequence == Sequence; }), TEXT("Each launched chain has a unique bounded presentation sequence."))) return false;
            FObservedSegment& Added = Segments.AddDefaulted_GetRef();
            Added.Component = Component;
            Added.Target = Enemies[Sequence];
            Added.TargetBeforeFixtureMove = Enemies[Sequence]->GetCapsuleComponent()->GetComponentLocation();
            Added.Sequence = Sequence;
            Added.FixedSource = Component->GetComponentLocation();
            Added.Instance = Instance->GetSystemInstance_Unsafe();
            bool bAudioValid = false;
            const bool bAudioOn = Component->GetVariableBool(TEXT("User.AudioOn"), bAudioValid);
            if (!Check(bAudioValid && bAudioOn == (Sequence == 0), TEXT("Only the first main beam enables authored SFX; later beams retain disabled main SFX."))) return false;
            if (!Check(NearestAnchor <= FMath::Square(2.0), TEXT("Authored offset cancellation anchors the new beam to its source capsule, including height."))) return false;
            if (Cases[CaseIndex].bLargeWorld && !Check(!Instance->GetSystemInstance_Unsafe()->GetLWCTile().IsNearlyZero(), TEXT("The live Niagara system actually uses a nonzero LWC tile."))) return false;
            Segment = &Added;
        }
        const float Age = Instance->GetAge();
        const float PreviousAge = Segment->LastAge;
        if (!Check(Segment->Instance == Instance->GetSystemInstance_Unsafe() && Age + 0.0001f >= Segment->LastAge && Component->GetComponentLocation().Equals(Segment->FixedSource, 2.0), TEXT("Moving endpoints preserve the original system instance, advancing age and fixed source holder."))) return false;
        Segment->LastAge = Age;
        if (!Segment->bMovedTarget && Age >= 0.04f)
        {
            const FVector Side(-Cases[CaseIndex].Direction.Y, Cases[CaseIndex].Direction.X, 0);
            Segment->Target->AddActorWorldOffset(Side * 20.0 + FVector(0, 0, 10), false, nullptr, ETeleportType::TeleportPhysics);
            Segment->bMovedTarget = true;
            Segment->MovedOnFrame = GFrameCounter;
            Segment->MovedAtWorldTime = Controller->GetWorld()->GetTimeSeconds();
            Segment->EndpointProgressSteps = 0;
        }
        if (!Segment->bMovedTarget || GFrameCounter < Segment->MovedOnFrame + 2) return true;
        // Editor frame counters may advance while PIE simulation is unchanged; require two observed Niagara age advances before sampling.
        // PIE 시뮬레이션이 멈춘 동안에도 에디터 프레임 번호가 증가할 수 있어 Niagara 나이의 실제 진행 두 번을 확인한 뒤 읽습니다.
        if (Age > PreviousAge + 0.0001f) ++Segment->EndpointProgressSteps;
        if (Segment->EndpointProgressSteps < 2 || Controller->GetWorld()->GetTimeSeconds() <= Segment->MovedAtWorldTime) return true;
        FVector Endpoint = FVector::ZeroVector;
        const FVector Expected = Segment->Target->GetCapsuleComponent()->GetComponentLocation();
        const bool bDecoded = ReadWorldEndpoint(Component, Definition.Vfx.EndPositionParameter, Definition.Vfx.EndPositionSpace, Endpoint);
        if (!bDecoded || !Endpoint.Equals(Expected, 2.0))
        {
            bool bRawVectorValid = false;
            bool bRawPositionValid = false;
            const FVector RawVector = Component->GetVariableVec3(Definition.Vfx.EndPositionParameter, bRawVectorValid);
            const FVector RawPosition = Component->GetVariablePosition(Definition.Vfx.EndPositionParameter, bRawPositionValid);
            const bool bVectorParameter = Component->GetAsset()->GetExposedParameters().FindParameterOffset(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), Definition.Vfx.EndPositionParameter)) != nullptr;
            const bool bPositionParameter = Component->GetAsset()->GetExposedParameters().FindParameterOffset(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), Definition.Vfx.EndPositionParameter)) != nullptr;
            const FCombatChainRuntimeData& Runtime = Owner->GetChainRuntimeData();
            FString HitIds;
            for (int32 UnitId : Runtime.HitUnitIds) HitIds += FString::Printf(TEXT("%d,"), UnitId);
            const FCombatRoundView& View = Controller->GetRoundCoordinator()->GetView();
            const FCombatRoundUnitView* TargetView = View.Units.FindByPredicate([Segment](const FCombatRoundUnitView& Unit) { return Unit.Unit == Segment->Target.Get(); });
            const UCharacterMovementComponent* Movement = Segment->Target->GetCharacterMovement();
            const FString Diagnostic = FString::Printf(TEXT("Chain endpoint mismatch: case=%s sequence=%d age=%.6f previousAge=%.6f frame=%llu movedFrame=%llu frameDelta=%llu decoded=%d endpoint=%s expected=%s rawVec3=%s validVec3=%d rawPosition=%s validPosition=%d declaredVec3=%d declaredPosition=%d space=%d parameter=%s component=%s transform=%s LWCtile=%s runtimeHop=%d runtimeTarget=%d hits=[%s] resolved=%d actualTarget=%s targetAlive=%d targetHP=%.3f movementMode=%d movementTick=%d round=%d roundPhase=%d targetActionPhase=%d."), *Cases[CaseIndex].Label, Segment->Sequence, Age, PreviousAge, GFrameCounter, Segment->MovedOnFrame, GFrameCounter - Segment->MovedOnFrame, bDecoded, *Endpoint.ToString(), *Expected.ToString(), *RawVector.ToString(), bRawVectorValid, *RawPosition.ToString(), bRawPositionValid, bVectorParameter, bPositionParameter, static_cast<int32>(Definition.Vfx.EndPositionSpace), *Definition.Vfx.EndPositionParameter.ToString(), *Component->GetPathName(), *Component->GetComponentTransform().ToString(), *FVector(Instance->GetSystemInstance_Unsafe()->GetLWCTile()).ToString(), Runtime.HopIndex, Runtime.TargetUnitId, *HitIds, Owner->HasResolved(), *Segment->Target->GetPathName(), Segment->Target->IsUnitAlive(), Segment->Target->GetAttributeSet()->GetHP(), Movement ? static_cast<int32>(Movement->MovementMode) : INDEX_NONE, Movement && Movement->IsComponentTickEnabled(), View.RoundNumber, static_cast<int32>(View.Phase), TargetView ? static_cast<int32>(TargetView->ActionPhase) : INDEX_NONE);
            // Persist raw authored values and live state before teardown; diagnostics never change endpoints or suppress the real assertion.
            // 종료 전에 원본 매개변수와 실제 상태를 보존하며 진단으로 끝점을 바꾸거나 실제 검증을 우회하지 않습니다.
            EndpointDiagnostics.Add(MakeShared<FJsonValueString>(Diagnostic));
            EndpointDiagnostics.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("PIE progress at endpoint mismatch: worldTime=%.6f movedAtWorldTime=%.6f observedNiagaraAgeAdvances=%d."), Controller->GetWorld()->GetTimeSeconds(), Segment->MovedAtWorldTime, Segment->EndpointProgressSteps)));
            Test->AddInfo(Diagnostic);
            UE_LOG(LogTemp, Display, TEXT("%s"), *Diagnostic);
            Check(false, TEXT("The actual world endpoint follows the moved target capsule on all axes without reactivation."));
            return false;
        }
        if (!Check(FVector::DotProduct(Component->GetForwardVector().GetSafeNormal2D(), (Expected - Segment->FixedSource).GetSafeNormal2D()) > 0.95, TEXT("Every live chain beam faces from its own source capsule toward the target capsule."))) return false;
        Segment->bFollowingVerified = true;
        if (!Segment->bCaptured && Age >= 0.15f && Particles > 0 && PendingScreenshot.IsEmpty())
        {
            Test->AddInfo(FString::Printf(TEXT("%s segment%d: source=%s; decoded endpoint=%s; expected capsule=%s; LWCtile=%s; particleAge=%.3f; particles=%d."), *Cases[CaseIndex].Label, Segment->Sequence, *Segment->FixedSource.ToString(), *Endpoint.ToString(), *Expected.ToString(), *FVector(Instance->GetSystemInstance_Unsafe()->GetLWCTile()).ToString(), Age, Particles));
            Segment->bCaptured = QueueCapture(FString::Printf(TEXT("segment%d"), Segment->Sequence), Age);
        }
        return true;
    }

    bool IsDirectional() const
    {
        return Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Projectile) || Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Beam) || Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Slash);
    }

    bool ObserveImpacts()
    {
        if (Definition.ImpactVfx.Niagara.IsNull()) return true;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* Component = *It;
            if (Component->GetWorld() != Controller->GetWorld() || Component->GetAsset() != Definition.ImpactVfx.Niagara.Get() || !Component->IsActive() || PriorImpactComponents.Contains(Component)) continue;
            const FNiagaraSystemInstanceControllerPtr Instance = Component->GetSystemInstanceController();
            if (!Instance || !Instance->IsValid()) continue;
            Instance->WaitForConcurrentTickAndFinalize();
            int32 Particles = 0;
            for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetSystemInstance_Unsafe()->GetEmitters()) Particles += FMath::Max(Emitter->GetNumParticles(), 0);
            if (Particles <= 0) continue;
            if (!ImpactInstances.Contains(Component))
            {
                bool bNearActualContact = false;
                TArray<AUnitBase*> ContactBodies = Cases[CaseIndex].bMonster ? TArray<AUnitBase*>{Target} : Enemies;
                for (AUnitBase* Body : ContactBodies)
                {
                    if (!IsValid(Body)) continue;
                    const UCapsuleComponent* Capsule = Body->GetCapsuleComponent();
                    const double ContactBound = Capsule->GetScaledCapsuleHalfHeight() + Capsule->GetScaledCapsuleRadius() + Definition.ImpactVfx.RelativeTransform.GetTranslation().Length() + 5.0;
                    bNearActualContact |= FVector::DistSquared(Component->GetComponentLocation(), Capsule->GetComponentLocation()) <= FMath::Square(ContactBound);
                }
                if (!Check(bNearActualContact, TEXT("The original impact instance is placed near an actual struck body rather than an unrelated world origin."))) return false;
                ImpactInstances.Add(Component);
                Test->AddInfo(FString::Printf(TEXT("%s impact%d: system=%s; position=%s; age=%.3f; particles=%d."), *Cases[CaseIndex].Label, ImpactInstances.Num(), *Component->GetAsset()->GetPathName(), *Component->GetComponentLocation().ToString(), Instance->GetAge(), Particles));
            }
            MaxImpactParticles = FMath::Max(MaxImpactParticles, Particles);
            if (!Cases[CaseIndex].bChain && !Cases[CaseIndex].bBasicAttack && !bImpactCaptureQueued && Instance->GetAge() >= 0.05f && PendingScreenshot.IsEmpty()) bImpactCaptureQueued = QueueCapture(TEXT("actual_impact"), Instance->GetAge());
        }
        return true;
    }

    bool ObserveMonsterAttack()
    {
        const ACombatRoundCoordinator* Round = Controller->GetRoundCoordinator();
        const UAnimInstance* Animation = Source->GetMesh()->GetAnimInstance();
        const bool bOriginalMontagePlaying = Animation && ExpectedMonsterMontage.IsValid() && Animation->Montage_IsPlaying(ExpectedMonsterMontage.Get());
        bSawMontage |= bOriginalMontagePlaying;
        bApproached |= FVector::DistSquared2D(Source->GetActorLocation(), OriginalMonsterPosition) > FMath::Square(2.0);
        if (bOriginalMontagePlaying)
        {
            const FVector TowardTarget = (Target->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D();
            if (!Check(!TowardTarget.IsNearlyZero() && FVector::DotProduct(Source->GetActorForwardVector().GetSafeNormal2D(), TowardTarget) > 0.95, TEXT("The original monster's actual attacking body faces its human target."))) return false;
            bOrientationVerified = true;
        }
        bReturned |= Round->GetView().RoundNumber > InitialRound && Source->GetActorLocation().Equals(OriginalMonsterPosition, 2.0);
        if (!PendingScreenshot.IsEmpty()) return true;
        if (MonsterCapturesQueued == 0 && bApproached && QueueCapture(TEXT("approach_observed"), static_cast<float>(FPlatformTime::Seconds() - StageStarted))) ++MonsterCapturesQueued;
        else if (MonsterCapturesQueued == 1 && CapturedThisCase == 1 && bOriginalMontagePlaying && QueueCapture(TEXT("authored_attack"), Animation->Montage_GetPosition(ExpectedMonsterMontage.Get()))) ++MonsterCapturesQueued;
        else if (MonsterCapturesQueued == 2 && CapturedThisCase == 2 && bReturned && QueueCapture(TEXT("returned_home"), static_cast<float>(FPlatformTime::Seconds() - StageStarted))) ++MonsterCapturesQueued;
        return !bCaptureFailed;
    }

    bool ObserveNinjaVisibility(UNiagaraComponent* Component, const FNiagaraSystemInstanceControllerPtr& Instance)
    {
        if (!bNinjaVisibilityReview) return true;
        bool bHeightValid = false;
        const float OverrideHeight = Component->GetVariableFloat(TEXT("User.HeightOffset"), bHeightValid);
        if (Cases[CaseIndex].NinjaVisibilityCondition == ENinjaVisibilityCondition::SourceHeight20 && !Check(bHeightValid && FMath::IsNearlyEqual(OverrideHeight, 20.f), TEXT("The actual original Ninja component receives the transient 20cm source-height parameter through the normal presentation path."))) return false;
        const float Age = Instance->GetAge();
        if (NinjaVisibilityDiagnostics.Num() >= 64 || (LastNinjaDiagnosticAge >= 0.f && Age - LastNinjaDiagnosticAge < 0.04f)) return true;
        bool bInspectMaterials = NinjaVisibilityDiagnostics.IsEmpty();
        for (float Threshold : {0.08f, 0.12f, 0.3f, 0.6f}) bInspectMaterials |= LastNinjaDiagnosticAge < Threshold && Age >= Threshold;
        LastNinjaDiagnosticAge = Age;
        TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetStringField(TEXT("case"), Cases[CaseIndex].Label);
        Sample->SetNumberField(TEXT("age"), Age);
        Sample->SetStringField(TEXT("component"), Component->GetPathName());
        Sample->SetStringField(TEXT("component_transform"), Component->GetComponentTransform().ToString());
        Sample->SetNumberField(TEXT("component_height_above_tile_cm"), Component->GetComponentLocation().Z - FloorZ);
        Sample->SetBoolField(TEXT("component_visible_flag"), Component->IsVisible());
        Sample->SetBoolField(TEXT("component_has_scene_proxy"), Component->GetSceneProxy() != nullptr);
        Sample->SetStringField(TEXT("component_render_bounds_center"), Component->Bounds.Origin.ToString());
        Sample->SetStringField(TEXT("component_render_bounds_extent"), Component->Bounds.BoxExtent.ToString());
        Sample->SetNumberField(TEXT("actual_camera_lod_distance_cm"), Instance->GetLODDistance());
        Sample->SetBoolField(TEXT("component_height_override_present"), bHeightValid);
        if (bHeightValid) Sample->SetNumberField(TEXT("component_height_override_cm"), OverrideHeight);
        TArray<TSharedPtr<FJsonValue>> Emitters;
        for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetSystemInstance_Unsafe()->GetEmitters())
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Emitter->GetEmitterHandle().GetName().ToString());
            Entry->SetNumberField(TEXT("execution_state"), static_cast<int32>(Emitter->GetExecutionState()));
            Entry->SetBoolField(TEXT("gpu_simulation"), Emitter->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim);
            Entry->SetBoolField(TEXT("particle_positions_local_space"), Emitter->IsLocalSpace());
            Entry->SetNumberField(TEXT("particles_delayed_gpu_estimate"), Emitter->GetNumParticles());
            Entry->SetNumberField(TEXT("gpu_count_buffer_estimate"), Emitter->GetGpuCountBufferEstimate());
            Entry->SetNumberField(TEXT("total_spawned_particles"), Emitter->GetTotalSpawnedParticles());
            Entry->SetBoolField(TEXT("waiting_for_pso_caching"), Emitter->IsWaitingForPSOCaching());
            const FBox Bounds = Emitter->GetBounds();
            Entry->SetBoolField(TEXT("cached_bounds_valid"), Bounds.IsValid != 0);
            if (Bounds.IsValid)
            {
                Entry->SetStringField(TEXT("cached_bounds_min_raw"), Bounds.Min.ToString());
                Entry->SetStringField(TEXT("cached_bounds_max_raw"), Bounds.Max.ToString());
            }
            TArray<TSharedPtr<FJsonValue>> Renderers;
            const FVersionedNiagaraEmitterData* Data = Emitter->GetEmitterHandle().GetEmitterData();
            if (Data)
            {
                for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
                {
                    if (!Renderer) continue;
                    TSharedRef<FJsonObject> RendererEntry = MakeShared<FJsonObject>();
                    RendererEntry->SetStringField(TEXT("class"), Renderer->GetClass()->GetPathName());
                    RendererEntry->SetBoolField(TEXT("property_enabled"), Renderer->GetIsEnabled());
                    RendererEntry->SetBoolField(TEXT("simulation_target_supported"), Renderer->IsSimTargetSupported(Emitter->GetSimTarget()));
                    RendererEntry->SetNumberField(TEXT("source_data_mode"), static_cast<int32>(Renderer->GetCurrentSourceMode()));
                    Renderers.Add(MakeShared<FJsonValueObject>(RendererEntry));
                }
            }
            Entry->SetArrayField(TEXT("renderer_properties"), Renderers);
            Emitters.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Sample->SetArrayField(TEXT("actual_emitter_instances"), Emitters);
        if (bInspectMaterials)
        {
            // Inspect live renderer materials and texture bindings without reading back or modifying GPU volume data.
            // GPU 볼륨 데이터를 읽어 오거나 수정하지 않고 실제 렌더러 재질과 텍스처 연결을 관찰합니다.
            TArray<UMaterialInterface*> UsedMaterials;
            Component->GetUsedMaterials(UsedMaterials);
            TArray<TSharedPtr<FJsonValue>> Materials;
            for (UMaterialInterface* Material : UsedMaterials)
            {
                if (!IsValid(Material)) continue;
                TSharedRef<FJsonObject> MaterialEntry = MakeShared<FJsonObject>();
                MaterialEntry->SetStringField(TEXT("material"), Material->GetPathName());
                MaterialEntry->SetNumberField(TEXT("blend_mode"), static_cast<int32>(Material->GetBlendMode()));
                TArray<FMaterialParameterInfo> Infos;
                TArray<FGuid> Ids;
                Material->GetAllTextureParameterInfo(Infos, Ids);
                TArray<TSharedPtr<FJsonValue>> Textures;
                for (const FMaterialParameterInfo& Info : Infos)
                {
                    UTexture* Texture = nullptr;
                    const bool bBound = Material->GetTextureParameterValue(Info, Texture);
                    TSharedRef<FJsonObject> TextureEntry = MakeShared<FJsonObject>();
                    TextureEntry->SetStringField(TEXT("parameter"), Info.Name.ToString());
                    TextureEntry->SetBoolField(TEXT("parameter_resolved"), bBound);
                    TextureEntry->SetStringField(TEXT("texture"), IsValid(Texture) ? Texture->GetPathName() : TEXT("none"));
                    if (IsValid(Texture))
                    {
                        TextureEntry->SetStringField(TEXT("class"), Texture->GetClass()->GetPathName());
                        TextureEntry->SetNumberField(TEXT("width"), Texture->GetSurfaceWidth());
                        TextureEntry->SetNumberField(TEXT("height"), Texture->GetSurfaceHeight());
                        TextureEntry->SetNumberField(TEXT("depth"), Texture->GetSurfaceDepth());
                        TextureEntry->SetBoolField(TEXT("volume_render_target"), Texture->IsA<UTextureRenderTargetVolume>());
                        TextureEntry->SetBoolField(TEXT("texture_resource_present"), Texture->GetResource() != nullptr);
                    }
                    Textures.Add(MakeShared<FJsonValueObject>(TextureEntry));
                }
                MaterialEntry->SetArrayField(TEXT("live_texture_bindings"), Textures);
                Infos.Reset();
                Ids.Reset();
                Material->GetAllScalarParameterInfo(Infos, Ids);
                TSharedRef<FJsonObject> Scalars = MakeShared<FJsonObject>();
                for (const FMaterialParameterInfo& Info : Infos)
                {
                    float Value = 0.f;
                    if (Material->GetScalarParameterValue(Info, Value) && FMath::IsFinite(Value)) Scalars->SetNumberField(Info.Name.ToString(), Value);
                }
                MaterialEntry->SetObjectField(TEXT("live_scalar_parameters"), Scalars);
                Materials.Add(MakeShared<FJsonValueObject>(MaterialEntry));
            }
            Sample->SetArrayField(TEXT("live_renderer_materials"), Materials);
        }
        NinjaVisibilityDiagnostics.Add(MakeShared<FJsonValueObject>(Sample));
        return true;
    }

    bool ObserveLiveEffect()
    {
        if (Cases[CaseIndex].bMonster) return ObserveMonsterAttack() && ObserveImpacts();
        const bool bMontagePlaying = Source->GetMesh() && Source->GetMesh()->GetAnimInstance() && Source->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying();
        bSawMontage |= bMontagePlaying;
        if (Cases[CaseIndex].bBasicAttack && bMontagePlaying)
        {
            const FCombatRoundUnitView* Body = Controller->GetRoundCoordinator()->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.Unit == Source; });
            if (Body && (Body->ActionPhase == ECombatRoundActionPhase::Casting || Body->ActionPhase == ECombatRoundActionPhase::Recovery))
            {
                const FVector TowardTarget = (Target->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D();
                if (!Check(!TowardTarget.IsNearlyZero() && FVector::DotProduct(Source->GetActorForwardVector().GetSafeNormal2D(), TowardTarget) > 0.95, TEXT("The retained basic attack's actual casting body faces its selected target."))) return false;
                bOrientationVerified = true;
            }
        }
        int32 Particles = 0;
        float Age = 0.f;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* Component = *It;
            if (Component->GetWorld() != Controller->GetWorld() || Component->GetAsset() != Definition.Vfx.Niagara.Get() || !Component->GetAsset() || !Component->IsActive()) continue;
            bSawActive = true;
            bSawReady |= Component->GetAsset()->IsReadyToRun();
            const FNiagaraSystemInstanceControllerPtr Instance = Component->GetSystemInstanceController();
            if (!Instance || !Instance->IsValid()) continue;
            // Wait only for the engine's pending work before reading live emitter data; no manual Niagara tick is used.
            // 수동 Niagara Tick 없이 엔진의 진행 중 작업만 기다린 뒤 활성 이미터 데이터를 읽습니다.
            Instance->WaitForConcurrentTickAndFinalize();
            if (!ObserveNinjaVisibility(Component, Instance)) return false;
            if (bFocusedReview)
            {
                const float LODDistance = Instance->GetLODDistance();
                if (!Check(FMath::IsFinite(LODDistance) && LODDistance >= 0.f, TEXT("Focused review observes a finite actual Niagara camera LOD distance without overriding it."))) return false;
                FocusedMinLODDistance = FMath::Min(FocusedMinLODDistance, LODDistance);
                FocusedMaxLODDistance = FMath::Max(FocusedMaxLODDistance, LODDistance);
                bFocusedLODDistanceObserved = true;
            }
            int32 ComponentParticles = 0;
            for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetSystemInstance_Unsafe()->GetEmitters()) ComponentParticles += FMath::Max(Emitter->GetNumParticles(), 0);
            Particles += ComponentParticles;
            Age = FMath::Max(Age, Instance->GetAge());
            if (Cases[CaseIndex].bChain)
            {
                if (!ObserveChain(Component, Instance, ComponentParticles)) return false;
            }
            else if (!bOrientationVerified && IsDirectional() && !ExpectedAimDirection.IsNearlyZero())
            {
                const FVector Forward = Component->GetForwardVector().GetSafeNormal2D();
                if (!Check(FVector::DotProduct(Forward, ExpectedAimDirection) > 0.95, TEXT("The actual main Niagara component points toward the requested target direction."))) return false;
                bOrientationVerified = true;
            }
            if (!Cases[CaseIndex].bChain && !bCenterVerified && Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Area))
            {
                const FVector Anchor = Definition.TargetRule == ESkillTargetRule::EnemyTile ? Target->GetCurrentTile()->GetActorLocation() + FVector(0, 0, 100) : Target->GetCapsuleComponent()->GetComponentLocation();
                const FVector ExpectedCenter = Anchor + Component->GetComponentQuat().RotateVector(Definition.Vfx.RelativeTransform.GetLocation());
                if (!Check(Component->GetComponentLocation().Equals(ExpectedCenter, 2.0), TEXT("The actual nondirectional area, healing or shield effect stays centered on the selected target."))) return false;
                bCenterVerified = true;
            }
            if (Cases[CaseIndex].bFalling && !Definition.Vfx.StartPositionParameter.IsNone())
            {
                FVector Launch;
                const FVector ExpectedLaunch = Source->GetCapsuleComponent()->GetComponentLocation() + Component->GetComponentQuat().RotateVector(Definition.Vfx.StartPositionOffset);
                if (!Check(ReadWorldEndpoint(Component, Definition.Vfx.StartPositionParameter, Definition.Vfx.StartPositionSpace, Launch) && Launch.Equals(ExpectedLaunch, 2.0), TEXT("The authored component-local falling launch parameter decodes above the original caster."))) return false;
            }
            if (!Cases[CaseIndex].bChain && ComponentParticles > 0 && GenericCapturesQueued == 0) Test->AddInfo(FString::Printf(TEXT("%s: component=%s; capsule=%s; tile=%s; componentZ-floorZ=%.2f; rendered bounds center=%s extent=%s."), *Cases[CaseIndex].Label, *Component->GetComponentLocation().ToString(), *Target->GetCapsuleComponent()->GetComponentLocation().ToString(), *Target->GetCurrentTile()->GetActorLocation().ToString(), Component->GetComponentLocation().Z - FloorZ, *Component->Bounds.Origin.ToString(), *Component->Bounds.BoxExtent.ToString()));
        }
        if (ChainActor.IsValid()) ChainHitIds = ChainActor->GetChainRuntimeData().HitUnitIds;
        if (!ObserveImpacts()) return false;
        MaxParticles = FMath::Max(MaxParticles, Particles);
        MaxAge = FMath::Max(MaxAge, Age);
        if (Cases[CaseIndex].bChain || !PendingScreenshot.IsEmpty()) return true;
        const bool bRenderable = Cases[CaseIndex].bBasicAttack ? bMontagePlaying : Particles > 0;
        const float ObservationAge = Cases[CaseIndex].bBasicAttack ? static_cast<float>(FPlatformTime::Seconds() - StageStarted) : Age;
        if (!bRenderable) return true;
        if (Cases[CaseIndex].bNinjaShortPhases && GenericCapturesQueued > 0)
        {
            // Sample short authored phases only while their real particles exist; never reactivate or extend playback for a screenshot.
            // 실제 파티클이 존재할 때만 짧은 원본 구간을 읽고 캡처를 위해 재활성화하거나 재생을 늘리지 않습니다.
            while (ShortPhasesConsidered < 2 && Age > (ShortPhasesConsidered == 0 ? 0.08f : 0.12f) + 0.03f) ++ShortPhasesConsidered;
            if (ShortPhasesConsidered < 2 && Age >= (ShortPhasesConsidered == 0 ? 0.08f : 0.12f))
            {
                if (QueueCapture(ShortPhasesConsidered == 0 ? TEXT("active_age008") : TEXT("active_age012"), Age))
                {
                    ++ShortCapturesQueued;
                    ++ShortPhasesConsidered;
                }
                return !bCaptureFailed;
            }
        }
        if (GenericCapturesQueued >= 2)
        {
            // Observe later authored particle phases without extending the original system lifetime or changing combat timing.
            // 원본 시스템 수명이나 전투 시간을 늘리지 않고 작성된 파티클의 이후 구간을 관찰합니다.
            if (NeedsLateCaptures(Cases[CaseIndex], Definition) && LateCapturesQueued < 2 && Age >= (LateCapturesQueued == 0 ? 0.3f : 0.6f) && QueueCapture(LateCapturesQueued == 0 ? TEXT("active_age030") : TEXT("active_age060"), Age)) ++LateCapturesQueued;
            else if (Cases[CaseIndex].bFocusedFinalBurstPhase && LateCapturesQueued == 2 && !bFocusedFinalBurstCaptureQueued && Age >= 1.25f && QueueCapture(TEXT("active_age125"), Age)) bFocusedFinalBurstCaptureQueued = true;
            return !bCaptureFailed;
        }
        if (GenericCapturesQueued == 1 && ObservationAge - FirstParticleAge < 0.1f) return true;
        if (QueueCapture(GenericCapturesQueued == 0 ? TEXT("first") : TEXT("next"), ObservationAge))
        {
            if (GenericCapturesQueued == 0) FirstParticleAge = ObservationAge;
            ++GenericCapturesQueued;
        }
        return !bCaptureFailed;
    }

    bool FinishCase()
    {
        const FCase& Current = Cases[CaseIndex];
        TArray<TSharedPtr<FJsonValue>> DeferredPhases;
        if (bFocusedReview)
        {
            TArray<FString> PlannedPhases{TEXT("first"), TEXT("next"), TEXT("active_age030"), TEXT("active_age060")};
            if (Current.bNinjaShortPhases) PlannedPhases.Append({TEXT("active_age008"), TEXT("active_age012")});
            if (Current.bFocusedFinalBurstPhase) PlannedPhases.Add(TEXT("active_age125"));
            if (!Definition.ImpactVfx.Niagara.IsNull()) PlannedPhases.Add(TEXT("actual_impact"));
            for (const FString& Phase : PlannedPhases)
            {
                const FString Suffix = TEXT("_") + Phase + TEXT(".png");
                if (CaseCaptureRecords.ContainsByPredicate([&Suffix](const TSharedPtr<FJsonValue>& Entry) { return Entry->AsObject()->GetStringField(TEXT("file")).EndsWith(Suffix); })) continue;
                DeferredPhases.Add(MakeShared<FJsonValueString>(Phase));
                Test->AddInfo(FString::Printf(TEXT("Focused phase unobserved during natural playback: %s/%s. No manual tick, reactivation, particle lifetime or authored timing changes were used; visual completion is deferred."), *Current.Label, *Phase));
            }
            DeferredCaptures += DeferredPhases.Num();
        }
        bool bValid = Check(CapturedThisCase + DeferredPhases.Num() == AttackCaptureCount(Current), bFocusedReview ? TEXT("Every focused PNG request is captured during natural playback or explicitly deferred without claiming visual completion.") : TEXT("The case has every requested completed PNG from the live game viewport."));
        bValid &= Check(bOriginalGasContext && AppliedEffects > 0, TEXT("Every applied GAS spec retains the original caster as instigator and source object."));
        bValid &= Check(FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&AuthoredSkill->RoundDefinition, &OriginalAuthoredDefinition, 0), TEXT("The authored DataAsset remains identical after the transient test."));
        if (Current.bBasicAttack) bValid &= Check(bSawMontage && bOrientationVerified, TEXT("The retained basic or original monster attack plays its authored character montage toward the actual target."));
        else bValid &= Check(bSawActive && bSawReady && MaxParticles > 0 && MaxAge > 0.f, TEXT("The original main VFX has active, ready, advancing real-frame particle simulation."));
        if (bFocusedReview)
        {
            bValid &= Check(bFocusedViewObserved && bFocusedLODDistanceObserved, TEXT("Focused playback observes the real view and Niagara LOD distance while preserving the original component quality settings."));
            if (Current.FocusedAudioCondition != EFocusedAudioCondition::None)
            {
                const bool bNearCamera = Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraOnly || Current.FocusedAudioCondition == EFocusedAudioCondition::NearCameraAndListener;
                bValid &= Check(bNearCamera ? FocusedMaxLODDistance < 2000.f : FocusedMinLODDistance > 2000.f, TEXT("The actual authored audio-emitter camera LOD distance remains on the intended side of its original boundary throughout observation."));
            }
        }
        if (!Current.bBasicAttack && !Current.bChain && IsDirectional() && !ExpectedAimDirection.IsNearlyZero()) bValid &= Check(bOrientationVerified, TEXT("The directional main Niagara orientation was inspected in its actual targeting direction."));
        if (!Current.bChain && Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Area)) bValid &= Check(bCenterVerified, TEXT("Nondirectional target-centered effects use their selected target center."));
        if (!Definition.ImpactVfx.Niagara.IsNull()) bValid &= Check(ImpactInstances.Num() >= (Current.bChain ? 3 : 1) && MaxImpactParticles > 0, TEXT("The original impact VFX is observed in actual active particle instances after the real GAS hit."));
        if (Current.bMonster)
        {
            bValid &= Check(bApproached && bReturned && AppliedEffects == 1 && FMath::IsNearlyEqual(Target->GetAttributeSet()->GetHP(), InitialHP.FindChecked(Target) - Definition.Power), TEXT("The original monster approaches, applies one unchanged GAS hit and returns without duplicate damage."));
            bValid &= Check(Source->GetActorLocation().Equals(OriginalMonsterPosition, 2.0) && Source->GetActorQuat().Equals(OriginalMonsterRotation, 0.01) && Source->GetCurrentTile() && Source->GetCurrentTile()->GetOccupyingUnit() == Source, TEXT("Monster recovery restores its authored facing, home location and occupancy."));
        }
        if (Current.bChain)
        {
            const TArray<int32> Expected{Enemies[0]->UnitIndex, Enemies[1]->UnitIndex, Enemies[2]->UnitIndex};
            bValid &= Check(AppliedEffects == 3 && ChainHitIds == Expected && Segments.Num() == 3, TEXT("The real coordinator chains to the nearest three distinct enemies in order and honors its total limit."));
            for (int32 Index = 0; Index < 3; ++Index) bValid &= Check(FMath::IsNearlyEqual(Enemies[Index]->GetAttributeSet()->GetHP(), InitialHP.FindChecked(Enemies[Index]) - Definition.Power), TEXT("Every actual chained GAS hit applies the unchanged original power once."));
            bValid &= Check(Enemies[3]->GetAttributeSet()->GetHP() == InitialHP.FindChecked(Enemies[3]), TEXT("The fourth enemy remains untouched."));
            for (const FObservedSegment& Segment : Segments) bValid &= Check(Segment.bFollowingVerified && Segment.bCaptured, TEXT("Every chain segment follows the moving target, retains height and supplies its own rendered PNG."));
            bValid &= Check(OriginalChainSettings.MaxTargets == 1 && AuthoredSkill->RoundDefinition.Chain.MaxTargets == 1, TEXT("Prototype values never replace the pending production balance default."));
        }
        else if (Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal)) bValid &= Check(Target->GetAttributeSet()->GetHP() > InitialHP.FindChecked(Target), TEXT("The representative healing effect changes actual authoritative HP."));
        else if (Definition.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield)) bValid &= Check(bAppliedShield, TEXT("The representative shield is observed in GAS before the next round expires it."));
        else bValid &= Check(Target->GetAttributeSet()->GetHP() < InitialHP.FindChecked(Target), TEXT("The representative attack reduces the intended body's actual authoritative HP."));
        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("case"), Current.Label);
        Record->SetStringField(TEXT("asset"), Current.Asset);
        Record->SetStringField(TEXT("main_niagara"), Definition.Vfx.Niagara.ToSoftObjectPath().ToString());
        Record->SetStringField(TEXT("impact_niagara"), Definition.ImpactVfx.Niagara.ToSoftObjectPath().ToString());
        Record->SetNumberField(TEXT("paid_ap_cost"), Definition.ActionPointCost);
        Record->SetBoolField(TEXT("original_gas_caster"), bOriginalGasContext);
        if (Current.bMonster)
        {
            Record->SetStringField(TEXT("monster_class"), Current.MonsterClass);
            Record->SetStringField(TEXT("original_mesh"), Source->GetMesh()->GetSkeletalMeshAsset()->GetPathName());
            Record->SetStringField(TEXT("resolved_attack_montage"), ExpectedMonsterMontage.IsValid() ? ExpectedMonsterMontage->GetPathName() : TEXT("none"));
            Record->SetBoolField(TEXT("approach_and_return_observed"), bApproached && bReturned);
        }
        Record->SetBoolField(TEXT("transient_multi_target_prototype"), Current.bChain);
        Record->SetBoolField(TEXT("large_world"), Current.bLargeWorld);
        Record->SetBoolField(TEXT("passed_runtime_contract"), bValid);
        Record->SetNumberField(TEXT("captures"), CapturedThisCase);
        Record->SetNumberField(TEXT("gas_effects"), AppliedEffects);
        Record->SetNumberField(TEXT("observed_impact_instances"), ImpactInstances.Num());
        Record->SetNumberField(TEXT("max_impact_particles_delayed_gpu_estimate"), MaxImpactParticles);
        Record->SetNumberField(TEXT("max_particles_delayed_gpu_estimate"), MaxParticles);
        Record->SetNumberField(TEXT("max_live_age"), MaxAge);
        Record->SetArrayField(TEXT("capture_phases"), CaseCaptureRecords);
        Record->SetBoolField(TEXT("late_main_phases_required"), NeedsLateCaptures(Current, Definition));
        Record->SetBoolField(TEXT("late_main_phases_captured"), LateCapturesQueued == 2);
        Record->SetBoolField(TEXT("actual_impact_frame_captured"), bImpactCaptureQueued);
        if (bFocusedReview)
        {
            Record->SetBoolField(TEXT("focused_review"), true);
            Record->SetStringField(TEXT("focused_condition"), Current.Label);
            Record->SetStringField(TEXT("actual_view_position"), FocusedViewPosition.ToString());
            Record->SetNumberField(TEXT("actual_view_to_target_distance_cm"), FocusedViewTargetDistance);
            Record->SetBoolField(TEXT("actual_niagara_lod_distance_observed"), bFocusedLODDistanceObserved);
            if (bFocusedLODDistanceObserved)
            {
                Record->SetNumberField(TEXT("actual_niagara_lod_distance_min_cm"), FocusedMinLODDistance);
                Record->SetNumberField(TEXT("actual_niagara_lod_distance_max_cm"), FocusedMaxLODDistance);
            }
            Record->SetNumberField(TEXT("requested_captures"), AttackCaptureCount(Current));
            Record->SetArrayField(TEXT("captures_deferred"), DeferredPhases);
            Record->SetBoolField(TEXT("capture_observation_complete"), DeferredPhases.IsEmpty());
            Record->SetBoolField(TEXT("short_main_phases_captured"), Current.bNinjaShortPhases && ShortCapturesQueued == 2);
            if (Current.bFocusedFinalBurstPhase) Record->SetBoolField(TEXT("focused_final_burst_phase_captured"), bFocusedFinalBurstCaptureQueued);
            if (Current.bNinjaShortPhases) Record->SetNumberField(TEXT("short_phase_age_overshoot_limit_seconds"), 0.03);
            Record->SetNumberField(TEXT("max_active_world_sounds_observed"), FocusedMaxWorldSounds.FindRef(Current.Label));
            Record->SetBoolField(TEXT("audio_device_diagnostics_observed"), FocusedMaxWorldSounds.Contains(Current.Label));
            Record->SetStringField(TEXT("audio_signal_scope"), Definition.Vfx.Sound.IsNull() ? TEXT("Observe authored Niagara audio without changing its LOD, emitter or sound parameters. Camera-distance culling may affect embedded audio; runtime success does not establish signal or listening quality.") : TEXT("The existing project profile plays its external original cue while its embedded audio is disabled by authored parameters. The Niagara emitter's camera-distance boundary does not imply an expected silent external cue. PCM signal and clipping require independent analysis; runtime success does not establish listening quality."));
            Record->SetBoolField(TEXT("natural_cue_completion_observation_required"), bNaturalCueObservationRequired);
            if (bNaturalCueObservationRequired)
            {
                bValid &= Check(bNaturalCueCompletionObserved && !bNaturalCueObservationFailed, TEXT("Optional focused recording retains the actual original cue through its finite-duration active-to-absent observation and real master-submix tail."));
                Record->SetBoolField(TEXT("passed_runtime_contract"), bValid);
                Record->SetStringField(TEXT("natural_cue_path"), NaturalCuePath);
                Record->SetNumberField(TEXT("natural_cue_authored_duration_seconds"), NaturalCueDuration);
                Record->SetNumberField(TEXT("natural_cue_expected_playback_seconds"), NaturalCueExpectedSeconds);
                Record->SetNumberField(TEXT("natural_cue_first_seen_audio_clock"), NaturalCueFirstSeenClock);
                Record->SetNumberField(TEXT("natural_cue_inferred_start_audio_clock"), NaturalCueInferredStartClock);
                Record->SetNumberField(TEXT("natural_cue_first_absent_audio_clock"), NaturalCueFirstAbsentClock);
                Record->SetNumberField(TEXT("natural_cue_last_active_audio_clock"), NaturalCueLastActiveClock);
                Record->SetNumberField(TEXT("natural_cue_max_observed_playback_age"), NaturalCueMaxPlaybackTime);
                Record->SetNumberField(TEXT("natural_cue_absent_snapshots"), NaturalCueAbsentSamples);
                Record->SetNumberField(TEXT("natural_cue_sampling_tolerance_seconds"), 0.1);
                Record->SetNumberField(TEXT("natural_cue_minimum_submix_tail_seconds"), 0.15);
                Record->SetBoolField(TEXT("natural_cue_completion_observed"), bNaturalCueCompletionObserved);
            }
            if (bNinjaVisibilityReview)
            {
                bValid &= Check(!NinjaVisibilityDiagnostics.IsEmpty(), TEXT("The Ninja comparison records actual emitter and renderer observations rather than treating its combined particle count as visibility evidence."));
                Record->SetBoolField(TEXT("passed_runtime_contract"), bValid);
                Record->SetNumberField(TEXT("ninja_visibility_condition"), static_cast<int32>(Current.NinjaVisibilityCondition));
                Record->SetStringField(TEXT("original_visual_translation"), OriginalNinjaDefinition.Vfx.RelativeTransform.GetLocation().ToString());
                Record->SetStringField(TEXT("transient_visual_translation"), Definition.Vfx.RelativeTransform.GetLocation().ToString());
                Record->SetNumberField(TEXT("original_source_height_cm"), -20.0);
                Record->SetNumberField(TEXT("requested_source_height_cm"), Current.NinjaVisibilityCondition == ENinjaVisibilityCondition::SourceHeight20 ? 20.0 : -20.0);
                Record->SetArrayField(TEXT("ninja_visibility_diagnostics"), NinjaVisibilityDiagnostics);
                Record->SetBoolField(TEXT("diagnostic_sample_limit_reached"), NinjaVisibilityDiagnostics.Num() == 64);
                Record->SetStringField(TEXT("ninja_visibility_scope"), TEXT("Original unchanged baseline versus one transient visual translation +30cm or source HeightOffset=20. GPU counts are delayed estimates; raw cached emitter bounds and renderer flags are observations. Live volume texture bindings and material scalars do not establish nonzero density or visible output; compare actual PNGs. No manual tick, original content save, quality, global CVar or rendering readback changes."));
            }
        }
        if (!AudioFilename.IsEmpty()) Record->SetStringField(TEXT("master_submix_wav"), AudioFilename);
        Records.Add(MakeShared<FJsonValueObject>(Record));
        Test->AddInfo(FString::Printf(TEXT("Completed %s: PNG=%d; GAS=%d; original caster=%d; max particles=%d; age=%.3f. Particle counts do not alone establish visual visibility or audible quality."), *Current.Label, CapturedThisCase, AppliedEffects, bOriginalGasContext, MaxParticles, MaxAge));
        return bValid;
    }

    bool QueueCapture(const FString& Suffix, float Age)
    {
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        if (!Check(Viewport && Viewport->Viewport && Viewport->GetWorld() == Controller->GetWorld(), TEXT("VFX capture targets the actual PIE viewport.")))
        {
            bCaptureFailed = true;
            return false;
        }
        CaptureViewport = Viewport;
        ExpectedSize = Viewport->Viewport->GetRenderTargetTextureSizeXY();
        PendingScreenshot = OutputDirectory / FString::Printf(TEXT("%02d_%s_%s.png"), CaseIndex + 1, *Cases[CaseIndex].Label, *Suffix);
        PendingAge = Age;
        return true;
    }

    void CaptureRenderedViewport(FViewport* RenderedViewport)
    {
        UGameViewportClient* Viewport = CaptureViewport.Get();
        if (PendingScreenshot.IsEmpty() || !Viewport || Viewport->Viewport != RenderedViewport) return;
        const FIntPoint Size = RenderedViewport->GetRenderTargetTextureSizeXY();
        TArray<FColor> Pixels;
        // Save the matching game render target after draw so Slate debug tools cannot conceal the effects under review.
        // 검수 효과가 Slate 디버그 도구에 가리지 않도록 그리기 이후 해당 게임 렌더 타깃을 저장합니다.
        if (Check(Viewport->GetWorld() == Controller->GetWorld() && Size == ExpectedSize && Size.X == 1280 && Size.Y == 720 && GetViewportScreenShot(RenderedViewport, Pixels) && Pixels.Num() == int64(Size.X) * Size.Y, TEXT("The rendered capture supplies all pixels from the expected 1280x720 PIE viewport.")))
        {
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
            if (Check(!Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *PendingScreenshot) && IFileManager::Get().FileSize(*PendingScreenshot) == Png.Num(), TEXT("The requested VFX PNG is fully saved before restart or teardown.")))
            {
                ++CapturedThisCase;
                ++CompletedCaptures;
                TSharedRef<FJsonObject> CaptureRecord = MakeShared<FJsonObject>();
                CaptureRecord->SetStringField(TEXT("file"), PendingScreenshot);
                CaptureRecord->SetNumberField(TEXT("observed_age"), PendingAge);
                CaseCaptureRecords.Add(MakeShared<FJsonValueObject>(CaptureRecord));
                Test->AddInfo(FString::Printf(TEXT("Rendered VFX PNG: %s; particle observation age=%.3f."), *PendingScreenshot, PendingAge));
            }
            else bCaptureFailed = true;
        }
        else bCaptureFailed = true;
        PendingScreenshot.Reset();
        CaptureViewport.Reset();
    }

    void RemoveGasObserver()
    {
        if (GasSource.IsValid() && GasSource->GetAbilitySystemComponent()) GasSource->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.Remove(GasHandle);
        GasSource.Reset();
        GasHandle.Reset();
    }

    void RestoreMovedActors()
    {
        for (int32 Index = MovedActors.Num() - 1; Index >= 0; --Index)
        {
            if (!MovedActors[Index].Actor.IsValid()) continue;
            MovedActors[Index].Actor->SetActorTransform(MovedActors[Index].Original, false, nullptr, ETeleportType::TeleportPhysics);
            if (bFocusedReview && MovedActors[Index].Actor->IsA<ACameraActor>()) Check(MovedActors[Index].Actor->GetActorTransform().Equals(MovedActors[Index].Original), TEXT("Focused camera restoration preserves the exact original location, rotation and scale before restart or teardown."));
        }
        MovedActors.Reset();
    }

    bool End()
    {
        RetainViewport();
        if (bRecordingAudio && IsValid(Controller))
        {
            UAudioMixerBlueprintLibrary::StopRecordingOutput(Controller, EAudioRecordingExportType::WavFile, AudioBasename, OutputDirectory);
            bRecordingAudio = false;
        }
        RemoveGasObserver();
        UnregisterAudioReviewClock();
        DrainAudioDiagnostics();
        RestoreAudioReviewEnvironment();
        RestoreFocusedView();
        RestoreMovedActors();
        if (FixtureMonster.IsValid()) FixtureMonster->Destroy();
        FixtureMonster.Reset();
        if (LargeWorldFloor.IsValid()) LargeWorldFloor->Destroy();
        LargeWorldFloor.Reset();
        TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
        Summary->SetNumberField(TEXT("planned_casts"), Cases.Num());
        Summary->SetNumberField(TEXT("planned_captures"), PlannedCaptures);
        Summary->SetNumberField(TEXT("completed_captures"), CompletedCaptures);
        Summary->SetBoolField(TEXT("focusScope"), bFocusedReview);
        Summary->SetBoolField(TEXT("ninja_visibility_review"), bNinjaVisibilityReview);
        if (bNinjaVisibilityReview) Summary->SetArrayField(TEXT("current_ninja_visibility_diagnostics"), NinjaVisibilityDiagnostics);
        if (bFocusedReview)
        {
            Summary->SetNumberField(TEXT("deferred_captures"), DeferredCaptures);
            Summary->SetArrayField(TEXT("focused_audio_diagnostics"), FocusedAudioDiagnostics);
            Summary->SetStringField(TEXT("focused_scope"), TEXT("Four current authored PoisonCarousel profiles: unchanged fixture camera, attenuation listener only, lower camera height only, and both. Camera XY and original Niagara/scalability remain unchanged; restore the default listener and camera transform before restart and teardown. External original cue playback is separate from the embedded audio emitter's camera culling. Optional recording observes that finite cue's active-to-absent transition and real submix tail before restart. Three original visibility profiles: FeudFang, Line_Lava and Spawn_Ninja_Root; preserve first/next and request actual particle ages 0.3/0.6, plus Ninja 0.08/0.12 and Line_Lava 1.25 after its authored fifth burst at 1.2. Naturally unavailable phases are explicitly deferred. Original AP, GAS caster, damage and DataAsset comparisons remain required. PCM signal, visible output and listening quality remain independent conclusions."));
            if (bNinjaVisibilityReview) Summary->SetStringField(TEXT("focused_scope"), TEXT("Three isolated Ninja comparisons: original unchanged baseline, one transient visual Z +30cm, and one transient User.HeightOffset=20. Original Niagara, DataAsset, tags, combat timing, AP, GAS, quality and user saves are preserved. Request first/next and natural ages 0.08/0.12/0.3/0.6; unavailable phases remain deferred. Record per-emitter execution, GPU count estimates, raw cached bounds, renderer flags and live material texture/scalar bindings. Visible volume density and the below-floor hypothesis require direct PNG comparison; runtime and particle success are not visibility proof."));
        }
        Summary->SetBoolField(TEXT("optional_master_submix_recording_requested"), bRecordRequested);
        Summary->SetStringField(TEXT("optional_audio_environment"), TEXT("Recording only: temporarily set official au.DisableAppVolume and au.NeverDisableSubmixes with console value/priority restoration; wait for the audio-thread start fence, actual master-submix sample callbacks and one real second before casting. Source audio, saved editor settings and FApp unfocused-volume configuration remain unchanged. Live device/listener/sound diagnostics and PCM signal analysis are separate from listening quality."));
        Summary->SetStringField(TEXT("scope"), bMonsterReview ? TEXT("Original 12 monster classes plus the retained skeleton; real enemy AI, authored montage, approach, recovery, GAS and AP; subsequent lethal GAS ragdoll, tile release and Restart restores original four-enemy roster. Preserve four early PNG and add a fifth at 2.5 actual world seconds with finite physical body velocities and awake state. A late sample does not establish final settling or floor penetration. Transient actors and human observation HP only; original physics, assets and saves stay unchanged. PNG review remains required for body pose and floor contact; audio listening, FPS and multiplayer are not measured.") : TEXT("Rendered local standalone PIE; preserve the 39 casts and append 48 distinct official DrGame assets. All 60 DrGame plus two retained human attacks. Original early observations plus required active main ages 0.3/0.6 for falling/area/healing/shield and actual impact PNG where authored; per-case phase ages are recorded. Chain capture starts at live age 0.15. Transient chain prototype=3/400cm/0.4s/1. PNG review remains required for visible direction and floor height; audio listening, FPS baseline and multiplayer are not measured."));
        if (bFocusedReview) Summary->SetStringField(TEXT("scope"), Summary->GetStringField(TEXT("focused_scope")));
        if (bSettlingReview)
        {
            Summary->SetBoolField(TEXT("ragdoll_settling_review"), true);
            Summary->SetStringField(TEXT("scope"), TEXT("Thirteen original monster attack/GAS/death/restart contracts and all 65 existing frames remain required. Add thirteen naturally simulated eight-world-second ragdoll PNGs and finite-bounds/velocity plus engine-awake observations on each available advancing world frame from 2.5 to eight seconds. Engine sleep and visible pose/floor contact are separate from combat contract success. Awake originals stay awake in the report; no force sleep, physics settings, manual tick, original asset or user-save changes. Final settlement is unconfirmed without engine sleep and actual screenshot review."));
        }
        Summary->SetArrayField(TEXT("cases"), Records);
        Summary->SetNumberField(TEXT("shutdown_stage"), Stage);
        Summary->SetStringField(TEXT("shutdown_case"), Cases.IsValidIndex(CaseIndex) ? Cases[CaseIndex].Label : TEXT("completed or awaiting catalog"));
        FAutomationTestExecutionInfo ExecutionInfo;
        Test->GetExecutionInfo(ExecutionInfo);
        TArray<TSharedPtr<FJsonValue>> Errors;
        for (const FAutomationExecutionEntry& Entry : ExecutionInfo.GetEntries())
        {
            if (Entry.Event.Type == EAutomationEventType::Error) Errors.Add(MakeShared<FJsonValueString>(Entry.Event.Message));
        }
        Summary->SetArrayField(TEXT("automation_errors_before_teardown"), Errors);
        Summary->SetArrayField(TEXT("chain_endpoint_failure_diagnostics"), EndpointDiagnostics);
        FString Json;
        FJsonSerializer::Serialize(Summary, TJsonWriterFactory<>::Create(&Json));
        Check(FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("summary.json"))), TEXT("The capture index is saved for visual review."));
        Test->AddInfo(TEXT("VFX direction capture directory: ") + OutputDirectory);
        // Release strong fixture references and rendering callbacks before PIE performs its world-reference GC audit.
        // PIE가 월드 참조 GC 검사를 수행하기 전에 검수의 강한 참조와 렌더링 콜백을 해제합니다.
        Prototype.Reset();
        AuthoredSkill.Reset();
        Settings.Reset();
        UGameViewportClient::OnViewportRendered().Remove(CaptureHandle);
        CaptureHandle.Reset();
        PendingScreenshot.Reset();
        CaptureViewport.Reset();
        ReviewWindow.Reset();
        Source = nullptr;
        Target = nullptr;
        Controller = nullptr;
        Mode = nullptr;
        Enemies.Reset();
        InitialHP.Reset();
        GEditor->RequestEndPlayMap();
        Advance(9);
        return false;
    }

    // Keep a game-thread owner while PIE removes its window so queued Slate draws cannot destroy the viewport on the render thread.
    // PIE가 창을 제거하는 동안 게임 스레드 소유 참조를 유지하여 대기 중인 Slate 렌더 작업이 뷰포트를 렌더 스레드에서 파괴하지 않게 합니다.
    void RetainViewport()
    {
        check(IsInGameThread());
        if (RetainedViewport.IsValid() || !IsValid(Controller) || !Controller->GetWorld()) return;
        UGameViewportClient* Viewport = Controller->GetWorld()->GetGameViewport();
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (Widget.IsValid()) RetainedViewport = Widget->GetViewportInterface().Pin();
    }

    // Drain queued Slate work after teardown, then release the last fixture reference on the required game thread.
    // 종료 후 대기 중인 Slate 작업을 비운 뒤 검수 참조를 필수 게임 스레드에서 해제합니다.
    void ReleaseRetainedViewport()
    {
        check(IsInGameThread());
        if (!RetainedViewport.IsValid()) return;
        FlushRenderingCommands();
        RetainedViewport.Reset();
    }

    bool Check(bool bValue, const FString& Message)
    {
        if (!bValue) UE_LOG(LogTemp, Display, TEXT("VFX review assertion failed: stage=%d case=%d (%s): %s"), Stage, CaseIndex + 1, Cases.IsValidIndex(CaseIndex) ? *Cases[CaseIndex].Label : TEXT("awaiting catalog"), *Message);
        return Test->TestTrue(Message, bValue);
    }
    void Advance(int32 NextStage) { Stage = NextStage; StageStarted = FPlatformTime::Seconds(); }
    FAutomationTestBase* Test;
    FString SaveSlot;
    TArray<FCase> Cases;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    TStrongObjectPtr<USkillDefinitionDataAsset> AuthoredSkill;
    TStrongObjectPtr<USkillDefinitionDataAsset> Prototype;
    ACombatDebugGameMode* Mode = nullptr;
    ACombatDebugPlayerController* Controller = nullptr;
    AUnitBase* Source = nullptr;
    AUnitBase* Target = nullptr;
    TArray<AUnitBase*> Enemies;
    TWeakObjectPtr<AUnitBase> GasSource;
    FDelegateHandle GasHandle;
    FCombatRoundSkill Definition;
    FCombatRoundSkill OriginalAuthoredDefinition;
    FCombatRoundSkill OriginalNinjaDefinition;
    FCombatChainSettings OriginalChainSettings;
    FVector ExpectedAimDirection = FVector::ZeroVector;
    TMap<AUnitBase*, float> InitialHP;
    TArray<FMovedActor> MovedActors;
    TWeakObjectPtr<AStaticMeshActor> LargeWorldFloor;
    TWeakObjectPtr<ACombatChainEffectActor> ChainActor;
    TWeakObjectPtr<AUnitBase> FixtureMonster;
    TWeakObjectPtr<ACombatGridTile> DeathHomeTile;
    TArray<FString> DefaultEnemyClasses;
    TWeakObjectPtr<UAnimMontage> ExpectedMonsterMontage;
    FVector OriginalMonsterPosition = FVector::ZeroVector;
    FQuat OriginalMonsterRotation = FQuat::Identity;
    TArray<int32> ChainHitIds;
    TArray<FObservedSegment> Segments;
    TSet<TWeakObjectPtr<UNiagaraComponent>> PriorImpactComponents;
    TSet<TWeakObjectPtr<UNiagaraComponent>> ImpactInstances;
    TArray<TSharedPtr<FJsonValue>> Records;
    TArray<TSharedPtr<FJsonValue>> CaseCaptureRecords;
    TArray<TSharedPtr<FJsonValue>> EndpointDiagnostics;
    TArray<TSharedPtr<FJsonValue>> FocusedAudioDiagnostics;
    TArray<TSharedPtr<FJsonValue>> NinjaVisibilityDiagnostics;
    TArray<TSharedPtr<FJsonValue>> SettlingFrameSamples;
    TMap<FString, int32> FocusedMaxWorldSounds;
    TSharedRef<FAudioReviewMailbox, ESPMode::ThreadSafe> AudioMailbox = MakeShared<FAudioReviewMailbox, ESPMode::ThreadSafe>();
    TSharedPtr<FAudioReviewClock, ESPMode::ThreadSafe> RecordingClock;
    FAudioDeviceHandle RecordingDevice;
    FAudioCommandFence AudioStartFence;
    TWeakObjectPtr<UGameViewportClient> CaptureViewport;
    TWeakPtr<SWindow> ReviewWindow;
    TSharedPtr<ISlateViewport> RetainedViewport;
    FDelegateHandle CaptureHandle;
    FIntPoint ExpectedSize = FIntPoint::ZeroValue;
    FString OutputDirectory;
    FString PendingScreenshot;
    FString AudioBasename;
    FString AudioFilename;
    FString NaturalCuePath;
    int32 Stage = 0;
    int32 CaseIndex = 0;
    int32 InitialRound = 0;
    int32 InitialAP = 0;
    int32 WarmFrames = 0;
    int32 ResizeAttempts = 0;
    int32 AudioStableFrames = 0;
    int32 NaturalCueAbsentSamples = 0;
    int32 PreviousAppVolumeBypass = 0;
    int32 PreviousNeverDisableSubmixes = 0;
    EConsoleVariableFlags PreviousAppVolumePriority = ECVF_SetByConstructor;
    EConsoleVariableFlags PreviousNeverDisablePriority = ECVF_SetByConstructor;
    int64 AudioLastBytes = -1;
    int32 AppliedEffects = 0;
    int32 CapturedThisCase = 0;
    int32 CompletedCaptures = 0;
    int32 PlannedCaptures = 0;
    int32 DeferredCaptures = 0;
    int32 MaxParticles = 0;
    int32 MaxImpactParticles = 0;
    int32 MonsterCapturesQueued = 0;
    int32 GenericCapturesQueued = 0;
    int32 LateCapturesQueued = 0;
    bool bFocusedFinalBurstCaptureQueued = false;
    int32 ShortCapturesQueued = 0;
    int32 ShortPhasesConsidered = 0;
    float MaxAge = 0.f;
    float FirstParticleAge = 0.f;
    float PendingAge = 0.f;
    float LastNinjaDiagnosticAge = -1.f;
    float InitialShield = 0.f;
    float NaturalCueDuration = 0.f;
    float NaturalCueMaxPlaybackTime = 0.f;
    FVector FocusedViewPosition = FVector::ZeroVector;
    double FocusedViewTargetDistance = 0.0;
    float FocusedMinLODDistance = TNumericLimits<float>::Max();
    float FocusedMaxLODDistance = 0.f;
    double FloorZ = 0.0;
    double StageStarted = 0.0;
    double AudioStoppedAt = 0.0;
    double AudioStartedAt = 0.0;
    double DeathStartedWorldTime = 0.0;
    double LastAudioDiagnosticAt = 0.0;
    double NaturalCueExpectedSeconds = 0.0;
    double NaturalCueFirstSeenClock = -1.0;
    double NaturalCueInferredStartClock = -1.0;
    double NaturalCueLastActiveClock = -1.0;
    double NaturalCueFirstAbsentClock = -1.0;
    double LastSettlingObservedWorldTime = -1.0;
    bool bOriginalGasContext = true;
    bool bCaptureFailed = false;
    bool bSawMontage = false;
    bool bSawActive = false;
    bool bSawReady = false;
    bool bAppliedShield = false;
    bool bOrientationVerified = false;
    bool bCenterVerified = false;
    bool bApproached = false;
    bool bReturned = false;
    bool bDeathCaptureQueued = false;
    bool bLateDeathCaptureQueued = false;
    bool bSettlingDeathCaptureQueued = false;
    bool bCatalogExpanded = false;
    bool bMonsterReview = false;
    bool bFocusedReview = false;
    bool bNinjaVisibilityReview = false;
    bool bSettlingReview = false;
    bool bFocusedViewObserved = false;
    bool bFocusedLODDistanceObserved = false;
    bool bFocusedListenerChanged = false;
    bool bRecordRequested = false;
    bool bAudioEnvironmentChanged = false;
    bool bRecordingAudio = false;
    bool bAudioExportPending = false;
    bool bNaturalCueObservationRequired = false;
    bool bNaturalCueSeen = false;
    bool bNaturalCueCompletionObserved = false;
    bool bNaturalCueObservationFailed = false;
    bool bImpactCaptureQueued = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTodoVfxDirectionsPIETest, "ProjectA.TodoReview.VfxDirections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTodoVfxDirectionsPIETest::RunTest(const FString& Parameters)
{
    if (!GEditor || !GEngine || !FApp::CanEverRender() || !FSlateApplication::IsInitialized() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("VFX direction review requires a rendering editor; -nullrhi cannot verify real pixels."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            AddError(TEXT("Close existing PIE before the isolated VFX direction review."));
            return false;
        }
    }
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_TodoReview_");
    bool bSuffixValid = Slot.Len() > Prefix.Len();
    for (TCHAR Character : Slot.Mid(Prefix.Len()))
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_')) bSuffixValid = false;
    }
    if (!Slot.StartsWith(Prefix) || !bSuffixValid || UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        AddError(TEXT("Supply a fresh -ProjectASaveSlot=ProjectA_Automation_TodoReview_<suffix>; existing user saves are preserved."));
        return false;
    }
    TestEqual(TEXT("The approved render scope contains exactly 39 casts."), TodoVfxDirections::MakeCases().Num(), 39);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<TodoVfxDirections::FDirectionReview>(this, Slot));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTodoMonsterAttacksPIETest, "ProjectA.TodoReview.MonsterAttacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTodoMonsterAttacksPIETest::RunTest(const FString& Parameters)
{
    if (!GEditor || !GEngine || !FApp::CanEverRender() || !FSlateApplication::IsInitialized() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddError(TEXT("Original monster attack review requires a rendering editor; -nullrhi cannot verify real pixels."));
        return false;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            AddError(TEXT("Close existing PIE before the isolated original monster attack review."));
            return false;
        }
    }
    const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(FCommandLine::Get());
    const FString Prefix = TEXT("ProjectA_Automation_TodoReview_");
    bool bSuffixValid = Slot.Len() > Prefix.Len();
    for (TCHAR Character : Slot.Mid(Prefix.Len()))
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_')) bSuffixValid = false;
    }
    if (!Slot.StartsWith(Prefix) || !bSuffixValid || UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        AddError(TEXT("Supply a fresh -ProjectASaveSlot=ProjectA_Automation_TodoReview_<suffix>; existing user saves are preserved."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Development/DebugCombat")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<TodoVfxDirections::FDirectionReview>(this, Slot, true));
    return true;
}
#endif
