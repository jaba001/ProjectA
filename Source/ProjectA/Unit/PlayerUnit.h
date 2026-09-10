#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "PlayerUnit.generated.h"

// Player-controlled combat unit implementation.
// 플레이어가 조작하는 전투 유닛 구현 클래스입니다.
UCLASS()
class PROJECTA_API APlayerUnit : public AUnitBase
{
	GENERATED_BODY()

public:
	// Sets player unit defaults.
	// 플레이어 유닛의 기본값을 설정합니다.
	APlayerUnit();

public:
	// Handles player unit turn start behavior.
	// 플레이어 유닛의 턴 시작 동작을 처리합니다.
	virtual void OnTurnStart() override;

	// Handles player unit turn end behavior.
	// 플레이어 유닛의 턴 종료 동작을 처리합니다.
    virtual void OnTurnEnd() override;

protected:
    // End exhausted turns only after action cleanup and completion notifications.
    // 행동 정리와 완료 통지 후에만 자원이 소진된 턴을 종료합니다.
    virtual void OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    FTimerHandle ExhaustedTurnTimer;

};
