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
    bool IsValidUnitTarget(int32 SourceUnitId, FName SkillId, int32 TargetUnitId) const;
    const FCombatRoundView& GetView() const { return View; }
    const TArray<FCombatRoundSkill>& GetSkills() const { return Skills; }
    const FCombatRoundSkill* FindSkill(FName SkillId) const;
    ACombatArena* GetArena() const { return Arena; }

private:
    struct FActionRuntime
    {
        FVector OriginalLocation = FVector::ZeroVector;
        FVector AimLocation = FVector::ZeroVector;
        FVector Destination = FVector::ZeroVector;
        double PhaseStarted = 0.0;
        bool bReleased = false;
        bool bMontageStarted = false;
        bool bFailed = false;
        int32 EffectiveTargetUnitId = INDEX_NONE;
    };

    UPROPERTY(Replicated)
    FCombatRoundView View;

    UPROPERTY(Replicated)
    TArray<FCombatRoundSkill> Skills;

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
    double Accumulator = 0.0;
    bool bCleaningUp = false;

    void BuildPrototypeSkills();
    void CleanupUnits();
    void BeginPlanning();
    void LockPlans();
    void AdvanceSimulation(float StepSeconds);
    void AdvanceAction(int32 Index, float StepSeconds);
    void ReleaseSkill(int32 Index, const FCombatRoundSkill& Skill);
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
