#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatGridManager.generated.h"

class ACombatGridTile;

// Actor that generates and queries the combat grid.
// 전투 그리드를 생성하고 조회하는 액터입니다.
UCLASS()
class PROJECTA_API ACombatGridManager : public AActor
{
    GENERATED_BODY()

public:
    // Sets grid manager defaults.
    // 그리드 매니저 기본값을 설정합니다.
    ACombatGridManager();

protected:
    // Generates or prepares grid data at startup.
    // 시작 시 그리드 데이터를 생성하거나 준비합니다.
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

    UPROPERTY(EditAnywhere, Category = "CombatGrid")
    TSubclassOf<ACombatGridTile> TileClass;

    UPROPERTY(EditAnywhere, Category = "CombatGrid")
    int32 RowCount = 4;

    UPROPERTY(EditAnywhere, Category = "CombatGrid")
    int32 ColCount = 4;

    UPROPERTY(EditAnywhere, Category = "CombatGrid")
    float Spacing = 200.f;

    UPROPERTY(EditAnywhere, Category = "CombatGrid")
    float GapSpacing = 200.f;

    UPROPERTY(EditAnywhere, Category = "CombatGrid")
    int32 GapStartIndex = 2;

public:
    // Generates all tiles for the configured grid size.
    // 설정된 그리드 크기에 맞춰 모든 타일을 생성합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGrid")
    void GenerateGrid();

    void ClearOccupancy();
    void SetGridActive(bool bActive);
    void DestroyGrid();

    // Map from grid coordinate to tile actor.
    // 그리드 좌표에서 타일 액터로 이어지는 맵입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FIntPoint, ACombatGridTile*> TileMap;

    // Returns tiles matching the requested coordinates.
    // 요청한 좌표에 해당하는 타일들을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGrid")
    TArray<ACombatGridTile*> GetTilesByCoords(const TArray<FIntPoint>& Coords) const;

    // Returns one tile at the requested coordinate.
    // 요청한 좌표의 단일 타일을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGrid")
    ACombatGridTile* GetTileAtCoord(const FIntPoint& Coord) const;

    // Returns orthogonally adjacent tiles.
    // 상하좌우로 인접한 타일들을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGrid")
    TArray<ACombatGridTile*> GetAdjacentTiles(ACombatGridTile* CenterTile) const;

    // Returns tiles within Chebyshev range around the center tile.
    // 중심 타일 기준 체비셰프 범위 안의 타일들을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGrid")
    TArray<ACombatGridTile*> GetTilesInChebyshevRange(ACombatGridTile* CenterTile, int32 Range) const;
};
