#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameModeBase.generated.h"

class ACombatManager;
class AUnitBase;
class ACombatGridManager;

// Game mode that spawns and initializes combat setup.
// 전투 구성을 스폰하고 초기화하는 게임 모드입니다.
UCLASS()
class PROJECTA_API ACombatGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    // Sets combat game mode defaults.
    // 전투 게임 모드 기본값을 설정합니다.
    ACombatGameModeBase();

protected:
    // Starts combat setup after game mode startup.
    // 게임 모드 시작 후 전투 구성을 시작합니다.
    virtual void BeginPlay() override;

public:
    // CombatManager class to spawn
    // 스폰할 전투 매니저 클래스입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TSubclassOf<ACombatManager> CombatManagerClass;

    // Player unit class list
    // 플레이어 유닛 클래스 목록입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // Enemy unit class list
    // 적 유닛 클래스 목록입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

    // CombatManager instance
    // 생성된 전투 매니저 인스턴스입니다.
    UPROPERTY()
    ACombatManager* CombatManager;

    // Spawn coordinates for player units
    // 플레이어 유닛 스폰 좌표 목록입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<FIntPoint> PlayerCoords;

    // Spawn coordinates for enemy units
    // 적 유닛 스폰 좌표 목록입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<FIntPoint> EnemyCoords;

    // Initialize and spawn combat setup
    // 전투 구성을 초기화하고 스폰합니다.
    void SpawnCombat();
};
