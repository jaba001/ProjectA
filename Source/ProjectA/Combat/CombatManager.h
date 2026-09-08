#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Turn/TurnManager.h"
#include "CombatManager.generated.h"

class UTurnManager;
class AUnitBase;
class ACombatGridManager;
class ACombatGridTile;

// Actor that coordinates combat units, turns, movement, and target tiles.
// 전투 유닛, 턴, 이동, 대상 타일을 조율하는 액터입니다.
UCLASS()
class PROJECTA_API ACombatManager : public AActor
{
    GENERATED_BODY()

public:
    FOnCombatResult OnCombatResult;
    bool IsCombatActive() const;
    void EndCombat();
    void ResetCombat();
    void SetCombatGrid(ACombatGridManager* Grid) { CombatGridManager = Grid; }
    const TArray<AUnitBase*>& GetRegisteredUnits() const { return CombatUnits; }

    // Sets combat manager defaults.
    // 전투 매니저 기본값을 설정합니다.
    ACombatManager();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // Initializes combat references after actor startup.
    // 액터 시작 후 전투 참조를 초기화합니다.
    virtual void BeginPlay() override;

    // Registers replicated combat state.
    // 복제되는 전투 상태를 등록합니다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


private:
    void HandleUnitDied(AUnitBase* Unit);
    void HandleCombatResult(ECombatResult Result);
    FTimerHandle DeadTurnTimer;
    // Player unit classes
    // 플레이어 유닛 클래스 목록입니다.
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // Enemy unit classes
    // 적 유닛 클래스 목록입니다.
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

    // Calculates tiles that the unit can reach with movement.
    // 유닛이 이동으로 도달할 수 있는 타일을 계산합니다.
    TArray<ACombatGridTile*> CalculateReachableMoveTiles(AUnitBase* Unit) const;

    // Checks whether the unit can enter the tile.
    // 유닛이 해당 타일에 진입할 수 있는지 확인합니다.
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
