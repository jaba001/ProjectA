#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CombatGameState.generated.h"

// Game state class for combat maps.
// 전투 맵에서 사용하는 게임 스테이트 클래스입니다.
UCLASS()
class PROJECTA_API ACombatGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    // Sets combat game state defaults.
    // 전투 게임 스테이트 기본값을 설정합니다.
    ACombatGameState();

protected:
    // Handles combat game state startup.
    // 전투 게임 스테이트 시작 처리를 수행합니다.
    virtual void BeginPlay() override;


};
