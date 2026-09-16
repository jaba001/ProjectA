#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Types/CombatResult.h"
#include "CombatRoundCoordinator.generated.h"

class ACombatArena;
class ACombatRoundProjectile;
class ACombatGridTile;
class APlayerController;
class AUnitBase;
class ACombatManager;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundCombatFinished, ECombatResult);
DECLARE_MULTICAST_DELEGATE(FOnRoundStateChanged);

// Owns the default combat planning, scheduled actions and real-time resolution.
// 기본 전투의 계획, 예약 행동과 실시간 판정을 소유합니다.
UCLASS()
class PROJECTA_API ACombatRoundCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACombatRoundCoordinator();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool InitializeFromCombat(ACombatManager* InManager, FText& OutError);
    bool IsRoundSessionActive() const;
    void SuspendRound();
    void StopRound();
    FOnRoundCombatFinished OnCombatFinished;
    FOnRoundStateChanged OnRoundStateChanged;
    int32 GetParticipantSlot(const APlayerController* Controller) const;
    bool SubmitPlan(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, const FCombatRoundCommand& Command, FText& OutError);
    bool SetParticipantReady(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, bool bReady, FText& OutError);
    bool CanPlanCommand(const FCombatRoundCommand& Command, FText& OutError) const;
    // Reposition during planning with SUP without adding a fabricated combat skill.
    // 가상 전투 스킬을 추가하지 않고 계획 단계에서 SUP로 위치를 옮깁니다.
    bool SubmitMove(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, int32 UnitId, FIntPoint Destination, FText& OutError);
    bool CanMoveUnit(int32 UnitId, FIntPoint Destination, FText& OutError) const;
    bool IsPlanningMoveInProgress() const { return bPlanningMoveInProgress; }
    bool IsValidUnitTarget(int32 SourceUnitId, FName SkillId, int32 TargetUnitId) const;
    const FCombatRoundView& GetView() const { return View; }
    const TArray<FCombatRoundSkill>& GetSkills() const { return Skills; }
    const FCombatRoundSkill* FindSkill(FName SkillId) const;
    ACombatArena* GetArena() const { return Arena; }

private:
    struct FActionRuntime
    {
        FVector OriginalLocation = FVector::ZeroVector;
        FRotator OriginalRotation = FRotator::ZeroRotator;
        FVector AimLocation = FVector::ZeroVector;
        FVector Destination = FVector::ZeroVector;
        double PhaseStarted = 0.0;
        double MontageStartedAt = 0.0;
        double MontageRecoverySeconds = 0.0;
        bool bReleased = false;
        bool bMontageStarted = false;
        bool bTrackMontageCompletion = false;
        bool bFailed = false;
        int32 EffectiveTargetUnitId = INDEX_NONE;
    };

    UPROPERTY(Replicated)
    FCombatRoundView View;

    UPROPERTY(Replicated)
    TArray<FCombatRoundSkill> Skills;

    UPROPERTY(Replicated)
    bool bPlanningMoveInProgress = false;

    UPROPERTY(Replicated)
    TObjectPtr<ACombatArena> Arena = nullptr;

    UPROPERTY()
    TObjectPtr<ACombatManager> CombatManager = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<APlayerController>> Participants;

    UPROPERTY()
    TArray<TObjectPtr<ACombatRoundProjectile>> Projectiles;

    TArray<FActionRuntime> Actions;
    double SimulationTime = 0.0;
    double MontageClock = 0.0;
    double Accumulator = 0.0;
    bool bCleaningUp = false;
    int32 PlanningMoveIndex = INDEX_NONE;
    TArray<FIntPoint> PlanningMovePath;
    int32 PlanningMoveStep = 0;
    double PlanningMoveElapsed = 0.0;
    FVector PlanningMoveOrigin = FVector::ZeroVector;
    FRotator PlanningMoveRotation = FRotator::ZeroRotator;

    // Resolve server-only AI idling separately from the equipped skill catalogue.
    // 서버 전용 AI 대기는 장착 스킬 목록과 분리하여 해석합니다.
    const FCombatRoundSkill* FindCommandSkill(const FCombatRoundCommand& Command) const;
    bool BuildPlanningMovePath(int32 UnitId, FIntPoint Destination, TArray<FIntPoint>& OutPath, FText& OutError) const;
    void AdvancePlanningMove(float DeltaSeconds);
    void FinishPlanningMove(bool bSucceeded);
    void CleanupUnits();
    void BeginPlanning();
    void LockPlans();
    void AdvanceSimulation(float StepSeconds);
    void AdvanceAction(int32 Index, float StepSeconds);
    void ReleaseSkill(int32 Index, const FCombatRoundSkill& Skill);
    void StartRecovery(int32 Index, bool bFailed, const FText& Status);
    void StartReturn(int32 Index, bool bFailed, const FText& Status);
    void FinishRoundIfSettled();
    void PublishState();
    bool HasExecutionAuthority() const;
    bool ValidateRequest(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, FText& OutError) const;
    bool ValidateCommand(const FCombatRoundCommand& Command, FText& OutError) const;
    bool ValidateDestinations(FText& OutError) const;
    int32 FindUnitIndex(int32 UnitId) const;
    int32 FindNearestEnemy(int32 SourceIndex) const;
    bool MoveUnitToward(int32 Index, FVector Destination, float Speed, float StepSeconds);
    void ApplyHit(AUnitBase* Source, AUnitBase* Target, float Damage);
    void HandleProjectileResolved(ACombatRoundProjectile* Projectile);
};
