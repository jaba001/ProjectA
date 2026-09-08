#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "EnemyUnit.generated.h"

class ACombatGridTile;
class UGameplayAbility;
class USkillDefinitionDataAsset;

// Enemy turn state used by the simple AI state machine.
// 단순 AI 상태 머신에서 사용하는 적 턴 상태입니다.
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

// Enemy action type selected by AI decision scoring.
// AI 결정 점수 계산으로 선택되는 적 행동 종류입니다.
UENUM(BlueprintType)
enum class EEnemyActionType : uint8
{
    None,
    Skill,
    Move,
    Item,
    Wait
};

// Result of enemy AI action evaluation.
// 적 AI 행동 평가 결과입니다.
USTRUCT(BlueprintType)
struct FEnemyActionDecision
{
    GENERATED_BODY()

public:
    // Chosen action type.
    // 선택된 행동 종류입니다.
    UPROPERTY()
    EEnemyActionType ActionType = EEnemyActionType::None;

    // Target unit for the chosen action.
    // 선택된 행동의 대상 유닛입니다.
    UPROPERTY()
    AUnitBase* TargetUnit = nullptr;

    // Target tile for the chosen action.
    // 선택된 행동의 대상 타일입니다.
    UPROPERTY()
    ACombatGridTile* TargetTile = nullptr;

    // Skill data used by the chosen action.
    // 선택된 행동에서 사용할 스킬 데이터입니다.
    UPROPERTY()
    TObjectPtr<USkillDefinitionDataAsset> SkillData = nullptr;

    // Score produced by action evaluation.
    // 행동 평가에서 산출된 점수입니다.
    UPROPERTY()
    float Score = -TNumericLimits<float>::Max();
};

// Enemy combat unit with turn-state AI decision logic.
// 턴 상태 기반 AI 결정 로직을 가진 적 전투 유닛입니다.
UCLASS()
class PROJECTA_API AEnemyUnit : public AUnitBase
{
    GENERATED_BODY()

public:
    AEnemyUnit();

public:
    // Turn-related events
    // 턴 관련 이벤트 처리입니다.
    virtual void OnTurnStart() override;
    virtual void OnTurnEnd() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    virtual void OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result) override;

public:
    // Set and query the current turn state
    // 현재 턴 상태를 설정하고 조회합니다.
    void SetTurnState(EEnemyTurnState NewState);
    EEnemyTurnState GetTurnState() const;

protected:
    // State entry handlers
    // 상태 진입 처리 함수들입니다.
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
    // 행동 후보를 평가하고 선택 결과를 적용합니다.
    FEnemyActionDecision DecideBestAction() const;
    FEnemyActionDecision EvaluateSkillAction() const;
    FEnemyActionDecision EvaluateSkillCandidate(USkillDefinitionDataAsset* SkillData) const;
    FEnemyActionDecision EvaluateWaitAction() const;
    void ApplyDecision(const FEnemyActionDecision& Decision);

protected:
    // Target evaluation for skill actions
    // 스킬 행동의 대상 평가 로직입니다.
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

    FTimerHandle ActionContinuationTimer;

    UPROPERTY()
    FEnemyActionDecision CurrentDecision;

protected:
    // Current FSM state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UnitBase|EnemyUnit|AI")
    EEnemyTurnState CurrentTurnState = EEnemyTurnState::None;

protected:
    // Base score values used for action evaluation
    // Base score added to all skill action candidates
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|EnemyUnit|AI|Score")
    float SkillBaseScore = 100.0f;

    // Base score for the wait action that ends the turn without doing anything
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|EnemyUnit|AI|Score")
    float WaitBaseScore = 0.0f;

    // Distance penalty weight
    // Higher values make the AI prefer closer targets more strongly
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|EnemyUnit|AI|Score")
    float DistanceWeight = 10.0f;

    // Low HP target preference weight
    // Lower HP ratios produce higher scores
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|EnemyUnit|AI|Score")
    float LowHPWeight = 50.0f;

    // High HP target preference weight
    // Higher HP ratios produce higher scores
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|EnemyUnit|AI|Score")
    float HighHPWeight = 30.0f;
};
