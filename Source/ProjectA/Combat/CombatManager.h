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
    // 플레이어 유닛 클래스들
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // 적 유닛 클래스들
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

    TArray<ACombatGridTile*> CalculateReachableMoveTiles(AUnitBase* Unit) const;
    bool CanUnitEnterTile(AUnitBase* Unit, ACombatGridTile* Tile) const;

private:
    // 서버 전용 턴 매니저
    UPROPERTY()
    UTurnManager* TurnManager;

    // 전투 유닛 목록 (서버 소유)
    UPROPERTY()
    TArray<AUnitBase*> CombatUnits;

    // 현재 턴 인덱스 (클라 동기화용)
    UPROPERTY(Replicated)
    int32 CurrentTurnIndex;

    UPROPERTY()
    ACombatGridManager* CombatGridManager;

    UPROPERTY()
    TArray<ACombatGridTile*> ReachableMoveTiles;

public:
    // 현재 턴 유닛
    UFUNCTION(BlueprintCallable)
    UTurnManager* GetTurnManager() const { return TurnManager; }

    UFUNCTION(Server, Reliable)
    void Server_StartCombat();

    void StartCombat_Internal();

    // 턴 진행 요청 (서버 전용)
    void AdvanceTurn();

	// 턴 종료 요청 (서버 전용)
    UFUNCTION(BlueprintCallable)
    void RequestEndTurn();

    // 유닛 등록 (서버 전용)
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

    // 보호여부
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RefreshTileProtectedByFront();

private:
    TArray<ACombatGridTile*> CalculateSkillTargetTiles(AUnitBase* Unit) const;

    UPROPERTY()
    TArray<ACombatGridTile*> SkillTargetTiles;

};