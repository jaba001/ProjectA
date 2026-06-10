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

};
