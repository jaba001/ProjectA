#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameModeBase.generated.h"

class ACombatManager;
class AUnitBase;
class ACombatGridManager;

UCLASS()
class PROJECTA_API ACombatGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACombatGameModeBase();

protected:
    virtual void BeginPlay() override;

public:
    // CombatManager 클래스 지정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TSubclassOf<ACombatManager> CombatManagerClass;

    // 플레이어 유닛 클래스 목록
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

    // CombatManager 인스턴스
    UPROPERTY()
    ACombatManager* CombatManager;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<FIntPoint> PlayerCoords;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<FIntPoint> EnemyCoords;

    void SpawnCombat();
};