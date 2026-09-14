#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "EnemyUnit.generated.h"

// Deprecated values remain readable for existing Blueprint and development references.
// 기존 블루프린트와 개발 참조를 읽을 수 있도록 사용 중단된 값을 유지합니다.
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

// Enemy identity and presentation remain; the round coordinator owns AI planning and execution.
// 적 식별과 표현은 유지하며 라운드 조정자가 AI 계획과 실행을 소유합니다.
UCLASS()
class PROJECTA_API AEnemyUnit : public AUnitBase
{
    GENERATED_BODY()

public:
    AEnemyUnit();
    virtual void OnTurnStart() override;
    void SetTurnState(EEnemyTurnState NewState);
    EEnemyTurnState GetTurnState() const { return EEnemyTurnState::None; }

private:
    // Retained serialized data has no effect on round AI decisions.
    // 유지한 직렬화 데이터는 라운드 AI 판단에 영향을 주지 않습니다.
    UPROPERTY(meta = (DeprecatedProperty))
    float SkillBaseScore = 100.0f;
};
