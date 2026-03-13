#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "EnemyUnit.generated.h"

class ACombatGridTile;
class UGameplayAbility;

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
    TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

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
    // 턴 관련 이벤트
    virtual void OnTurnStart() override;
    virtual void OnSkillFinished() override;
    virtual void OnReturnToOriginalTileFinished() override;

public:
    // 현재 턴 상태 설정 및 조회
    void SetTurnState(EEnemyTurnState NewState);
    EEnemyTurnState GetTurnState() const;

protected:
    // 상태 진입 처리
    void EnterStartTurnState();
    void EnterDecideActionState();
    void EnterMoveState();
    void EnterSkillState();
    void EnterEndTurnState();
    void EnterWaitMoveCompleteState();
    void EnterWaitSkillCompleteState();

    // 턴 종료 처리
    void FinishEnemyTurn();

protected:
    // 행동 후보 평가 및 적용
    FEnemyActionDecision DecideBestAction() const;
    FEnemyActionDecision EvaluateSkillAction() const;
    FEnemyActionDecision EvaluateSkillCandidate(TSubclassOf<UGameplayAbility> AbilityClass) const;
    FEnemyActionDecision EvaluateWaitAction() const;
    void ApplyDecision(const FEnemyActionDecision& Decision);

protected:
    // 스킬 행동 전용 타겟 평가
    float EvaluateSkillTargetScore(TSubclassOf<UGameplayAbility> AbilityClass, AUnitBase* Candidate) const;
    AUnitBase* FindBestSkillTarget(TSubclassOf<UGameplayAbility> AbilityClass) const;
    float EvaluateDefaultAttackScore(AUnitBase* Candidate) const;
    float EvaluateSkillSlotScore(TSubclassOf<UGameplayAbility> AbilityClass, AUnitBase* Candidate) const;
    float EvaluateLowHPScore(AUnitBase* Candidate) const;
    float EvaluateHighHPScore(AUnitBase* Candidate) const;

protected:
    // 현재 선택된 행동 컨텍스트
    UPROPERTY()
    AUnitBase* CurrentTarget = nullptr;

    UPROPERTY()
    ACombatGridTile* CurrentTargetTile = nullptr;

    UPROPERTY()
    bool bPendingNextActionAfterReturn = false;

    UPROPERTY()
    FEnemyActionDecision CurrentDecision;

protected:
    // 현재 FSM 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    EEnemyTurnState CurrentTurnState = EEnemyTurnState::None;

protected:
    // 행동 평가용 기본 점수
    // 모든 Skill 행동 후보에 공통으로 더해지는 기본 점수
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float SkillBaseScore = 100.0f;

    // 아무 행동도 하지 않고 턴을 종료하는 대기 행동의 기본 점수
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float WaitBaseScore = 0.0f;

    // 거리 패널티 가중치
    // 값이 클수록 가까운 타겟을 더 강하게 선호한다.
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float DistanceWeight = 10.0f;

    // 저체력 대상 선호 가중치
    // HP 비율이 낮을수록 더 높은 점수를 부여한다.
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float LowHPWeight = 50.0f;

    // 고체력 대상 선호 가중치
    // HP 비율이 높을수록 더 높은 점수를 부여한다.
    UPROPERTY(EditDefaultsOnly, Category = "AI|Score")
    float HighHPWeight = 30.0f;


};