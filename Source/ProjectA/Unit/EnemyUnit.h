#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "EnemyUnit.generated.h"

class ACombatGridTile;
class UGameplayAbility;
class USkillDefinitionDataAsset;

UENUM(BlueprintType)
enum class EEnemyTurnState : uint8
{
    None,
    StartTurn,
    DecideAction,
    Move,
    WaitMoveComplete,
    Skill,
    WaitSkillComplete,
    EndTurn
};

UENUM(BlueprintType)
enum class EEnemyActionType : uint8
{
    None,
    Skill,
    Move,
    Item,
    Wait
};

USTRUCT(BlueprintType)
struct FEnemyActionDecision
{
    GENERATED_BODY()

public:
    UPROPERTY()
    EEnemyActionType ActionType = EEnemyActionType::None;

    UPROPERTY()
    AUnitBase* TargetUnit = nullptr;

    UPROPERTY()
    ACombatGridTile* TargetTile = nullptr;

    UPROPERTY()
    TObjectPtr<USkillDefinitionDataAsset> SkillData = nullptr;

    UPROPERTY()
    float Score = -TNumericLimits<float>::Max();
};

UCLASS()
class PROJECTA_API AEnemyUnit : public AUnitBase
{
    GENERATED_BODY()

public:
    AEnemyUnit();

public:
    // Turn-related events
    virtual void OnTurnStart() override;
    virtual void OnSkillFinished() override;
    virtual void OnReturnToOriginalTileFinished() override;

public:
    // Set and query the current turn state
    void SetTurnState(EEnemyTurnState NewState);
    EEnemyTurnState GetTurnState() const;

protected:
    // State entry handlers
    void EnterStartTurnState();
    void EnterDecideActionState();
    void EnterMoveState();
    void EnterSkillState();
    void EnterEndTurnState();
    void EnterWaitMoveCompleteState();
    void EnterWaitSkillCompleteState();

    // Turn end handling
    void FinishEnemyTurn();

protected:
    // Evaluate and apply action candidates
    FEnemyActionDecision DecideBestAction() const;
    FEnemyActionDecision EvaluateSkillAction() const;
    FEnemyActionDecision EvaluateSkillCandidate(USkillDefinitionDataAsset* SkillData) const;
    FEnemyActionDecision EvaluateWaitAction() const;
    void ApplyDecision(const FEnemyActionDecision& Decision);

protected:
    // Target evaluation for skill actions
    float EvaluateSkillTargetScore(USkillDefinitionDataAsset* SkillData, AUnitBase* Candidate) const;
    AUnitBase* FindBestSkillTarget(USkillDefinitionDataAsset* SkillData) const;
    float EvaluateDefaultAttackScore(AUnitBase* Candidate) const;
    float EvaluateSkillSlotScore(USkillDefinitionDataAsset* SkillData, AUnitBase* Candidate) const;
    float EvaluateLowHPScore(AUnitBase* Candidate) const;
    float EvaluateHighHPScore(AUnitBase* Candidate) const;

protected:
    // Currently selected action context
    UPROPERTY()
    AUnitBase* CurrentTarget = nullptr;

    UPROPERTY()
    ACombatGridTile* CurrentTargetTile = nullptr;

    UPROPERTY()
    bool bPendingNextActionAfterReturn = false;

    UPROPERTY()
    FEnemyActionDecision CurrentDecision;

protected:
    // Current FSM state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    EEnemyTurnState CurrentTurnState = EEnemyTurnState::None;

protected:
    // Base score values used for action evaluation
    // Base score added to all skill action candidates
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float SkillBaseScore = 100.0f;

    // Base score for the wait action that ends the turn without doing anything
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float WaitBaseScore = 0.0f;

    // Distance penalty weight
    // Higher values make the AI prefer closer targets more strongly
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float DistanceWeight = 10.0f;

    // Low HP target preference weight
    // Lower HP ratios produce higher scores
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float LowHPWeight = 50.0f;

    // High HP target preference weight
    // Higher HP ratios produce higher scores
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float HighHPWeight = 30.0f;
};