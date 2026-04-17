#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatManager.generated.h"

class UTurnManager;
class AUnitBase;
class ACombatGridManager;
class ACombatGridTile;

UCLASS()
class PROJECTA_API ACombatManager : public AActor
{
    GENERATED_BODY()

public:
    ACombatManager();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


private:
    // Player unit classes
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // Enemy unit classes
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

    TArray<ACombatGridTile*> CalculateReachableMoveTiles(AUnitBase* Unit) const;
    bool CanUnitEnterTile(AUnitBase* Unit, ACombatGridTile* Tile) const;

private:
    // Server-only turn manager
    UPROPERTY()
    UTurnManager* TurnManager;

    // Combat unit list (owned by server)
    UPROPERTY()
    TArray<AUnitBase*> CombatUnits;

    // Current turn index (replicated to clients)
    UPROPERTY(Replicated)
    int32 CurrentTurnIndex;

    UPROPERTY()
    ACombatGridManager* CombatGridManager;

    UPROPERTY()
    TArray<ACombatGridTile*> ReachableMoveTiles;

public:
    // Current turn unit
    UFUNCTION(BlueprintCallable)
    UTurnManager* GetTurnManager() const { return TurnManager; }

    UFUNCTION(Server, Reliable)
    void Server_StartCombat();

    void StartCombat_Internal();

    // Request to advance turn (server only)
    void AdvanceTurn();

    // Request to end turn (server only)
    UFUNCTION(BlueprintCallable)
    void RequestEndTurn();

    // Register units (server only)
    void RegisterUnits(const TArray<AUnitBase*>& Units);

    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }

    UFUNCTION(BlueprintCallable)
    AUnitBase* GetCurrentUnit() const;

public:
    // Move
    UFUNCTION(BlueprintCallable, Category = "Move")
    void RefreshReachableMoveTiles();

    UFUNCTION(BlueprintCallable, Category = "Move")
    bool IsReachableMoveTile(ACombatGridTile* Tile) const;

    UFUNCTION(BlueprintCallable, Category = "Move")
    void HighlightMovableTiles();

    UFUNCTION(BlueprintCallable, Category = "Move")
    void ClearMovableTilesHighlight();

    UFUNCTION(BlueprintCallable, Category = "Move")
    const TArray<ACombatGridTile*>& GetReachableMoveTiles() const { return ReachableMoveTiles; }

    // Skill
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void RefreshSkillTargetTiles();

    UFUNCTION(BlueprintCallable, Category = "Skill")
    bool IsSkillTargetTile(ACombatGridTile* Tile) const;

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void HighlightSkillTargetTiles();

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void ClearSkillTargetTilesHighlight();

    // Tile
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatGridTile* GetTileByCoord(FIntPoint Coord) const;

    // Refresh front-line protection state
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RefreshTileProtectedByFront();

private:
    TArray<ACombatGridTile*> CalculateSkillTargetTiles(AUnitBase* Unit) const;

    UPROPERTY()
    TArray<ACombatGridTile*> SkillTargetTiles;

};