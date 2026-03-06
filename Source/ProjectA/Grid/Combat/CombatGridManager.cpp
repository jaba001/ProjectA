#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Engine/World.h"

ACombatGridManager::ACombatGridManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACombatGridManager::BeginPlay()
{
    Super::BeginPlay();

    if (!TileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatGridManager: TileClass not set."));
        return;
    }

    GenerateGrid();
}

void ACombatGridManager::GenerateGrid()
{
    if (!TileClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    TileMap.Empty();

    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        for (int32 Col = 0; Col < ColCount; ++Col)
        {
            // 기본 Col 위치 (Spacing * Col)
            float AdjustedY = Col * Spacing;

            // Col 2 이상이면 GapSpacing 만큼 X축에 추가 공간 부여
            if (Col >= GapStartIndex)
            {
                AdjustedY += GapSpacing;
            }
            // Col은 Y축으로 나감 (그대로 유지)
            float X = -Row * Spacing;
            float Y = AdjustedY;

            FVector SpawnLocation = GetActorLocation() + FVector(X, Y, 0.f);

            ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(TileClass, SpawnLocation, FRotator::ZeroRotator);
            if (Tile)
            {
                Tile->GridCoord = FIntPoint(Row, Col);
                TileMap.Add(Tile->GridCoord, Tile);
            }
        }
    }
}

TArray<ACombatGridTile*> ACombatGridManager::GetTilesByCoords(const TArray<FIntPoint>& Coords) const
{
    TArray<ACombatGridTile*> Result;
    Result.Reserve(Coords.Num());

    // 입력된 좌표 배열(Coords)의 순서를 그대로 유지하면서
    // 각 좌표에 해당하는 타일을 찾아 Result에 추가한다
    for (const FIntPoint& Coord : Coords)
    {
        // TileMap.Find는 ACombatGridTile* const*를 반환하므로 아래와 같이 포인터로 받는다
        if (ACombatGridTile* const* FoundTilePtr = TileMap.Find(Coord))
        {
            // 정상적으로 타일을 찾은 경우 해당 타일을 결과 배열에 추가
            Result.Add(*FoundTilePtr);
        }
        else
        {
            // 매핑에 없는 좌표는 nullptr을 넣어준다
            // (필요하면 스킵 방식으로 변경할 수 있다)
            Result.Add(nullptr);
        }
    }

    return Result;
}
