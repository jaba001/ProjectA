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
    // CombatManager class to spawn
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TSubclassOf<ACombatManager> CombatManagerClass;

    // Player unit class list
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // Enemy unit class list
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

    // CombatManager instance
    UPROPERTY()
    ACombatManager* CombatManager;

    // Spawn coordinates for player units
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<FIntPoint> PlayerCoords;

    // Spawn coordinates for enemy units
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatGameMode")
    TArray<FIntPoint> EnemyCoords;

    // Initialize and spawn combat setup
    void SpawnCombat();
};