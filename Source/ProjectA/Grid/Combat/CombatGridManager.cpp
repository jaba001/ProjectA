#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ACombatGridManager::ACombatGridManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
}

void ACombatGridManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatGridManager, bGridActive);
}

void ACombatGridManager::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    if (!TileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatGridManager: TileClass not set."));
        return;
    }

    GenerateGrid();
}

void ACombatGridManager::GenerateGrid()
{
    if (!HasAuthority() || !TileClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    DestroyGrid();

    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        for (int32 Col = 0; Col < ColCount; ++Col)
        {
            // Base column position (Spacing * Col)
            float AdjustedY = Col * Spacing;

            // Add extra gap spacing when the column index reaches GapStartIndex or beyond
            if (Col >= GapStartIndex)
            {
                AdjustedY += GapSpacing;
            }

            // Columns extend along the Y axis
            float X = -Row * Spacing;
            float Y = AdjustedY;

            FVector SpawnLocation = GetActorLocation() + FVector(X, Y, 0.f);

            ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(TileClass, SpawnLocation, FRotator::ZeroRotator);

            if (Tile)
            {
                Tile->InitializeGridTile(this, FIntPoint(Row, Col), Col < ColCount / 2 ? ETileTerritory::Player : ETileTerritory::Enemy);
            }
        }
    }
}

TArray<ACombatGridTile*> ACombatGridManager::GetTilesByCoords(const TArray<FIntPoint>& Coords) const
{
    TArray<ACombatGridTile*> Result;
    Result.Reserve(Coords.Num());

    // Preserve the input coordinate order and append the matching tile for each coordinate
    for (const FIntPoint& Coord : Coords)
    {
        // TileMap.Find returns ACombatGridTile* const*, so receive it as a pointer
        if (ACombatGridTile* const* FoundTilePtr = TileMap.Find(Coord))
        {
            // Add the tile when the coordinate is mapped successfully
            Result.Add(*FoundTilePtr);
        }
        else
        {
            // Insert nullptr when the coordinate is not mapped
            // This can be changed to a skip-based flow if needed
            Result.Add(nullptr);
        }
    }

    return Result;
}

ACombatGridTile* ACombatGridManager::GetTileAtCoord(const FIntPoint& Coord) const
{
    if (ACombatGridTile* const* FoundTilePtr = TileMap.Find(Coord))
    {
        return *FoundTilePtr;
    }

    return nullptr;
}

TArray<ACombatGridTile*> ACombatGridManager::GetAdjacentTiles(ACombatGridTile* CenterTile) const
{
    TArray<ACombatGridTile*> Result;

    if (!CenterTile)
    {
        return Result;
    }

    const FIntPoint CenterCoord = CenterTile->GridCoord;

    const TArray<FIntPoint> AdjacentCoords =
    {
        FIntPoint(CenterCoord.X - 1, CenterCoord.Y),
        FIntPoint(CenterCoord.X + 1, CenterCoord.Y),
        FIntPoint(CenterCoord.X, CenterCoord.Y - 1),
        FIntPoint(CenterCoord.X, CenterCoord.Y + 1),

        FIntPoint(CenterCoord.X - 1, CenterCoord.Y - 1),
        FIntPoint(CenterCoord.X - 1, CenterCoord.Y + 1),
        FIntPoint(CenterCoord.X + 1, CenterCoord.Y - 1),
        FIntPoint(CenterCoord.X + 1, CenterCoord.Y + 1)
    };

    Result.Reserve(AdjacentCoords.Num());

    for (const FIntPoint& Coord : AdjacentCoords)
    {
        if (ACombatGridTile* AdjacentTile = GetTileAtCoord(Coord))
        {
            Result.Add(AdjacentTile);
        }
    }

    return Result;
}

TArray<ACombatGridTile*> ACombatGridManager::GetTilesInChebyshevRange(ACombatGridTile* CenterTile, int32 Range) const
{
    TArray<ACombatGridTile*> Result;

    if (!CenterTile)
    {
        return Result;
    }

    if (Range < 0)
    {
        return Result;
    }

    const FIntPoint CenterCoord = CenterTile->GridCoord;

    for (int32 RowOffset = -Range; RowOffset <= Range; ++RowOffset)
    {
        for (int32 ColOffset = -Range; ColOffset <= Range; ++ColOffset)
        {
            const FIntPoint TestCoord =
            {
                CenterCoord.X + RowOffset,
                CenterCoord.Y + ColOffset
            };

            ACombatGridTile* FoundTile = GetTileAtCoord(TestCoord);

            if (!FoundTile)
            {
                continue;
            }

            Result.Add(FoundTile);
        }
    }

    return Result;
}

void ACombatGridManager::ClearOccupancy()
{
    if (!HasAuthority())
    {
        return;
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& Entry : TileMap)
    {
        if (IsValid(Entry.Value))
        {
            Entry.Value->SetOccupyingUnit(nullptr);
            Entry.Value->SetProtectedByFront(false);
            Entry.Value->ClearHighlightVisual();
        }
    }
}

void ACombatGridManager::SetGridActive(bool bActive)
{
    if (!HasAuthority())
    {
        return;
    }

    bGridActive = bActive;
    OnRep_GridActive();
    ForceNetUpdate();
}

void ACombatGridManager::OnRep_GridActive()
{
    for (const TPair<FIntPoint, ACombatGridTile*>& Entry : TileMap)
    {
        if (IsValid(Entry.Value))
        {
            Entry.Value->SetActorHiddenInGame(!bGridActive);
            Entry.Value->SetActorEnableCollision(bGridActive);
        }
    }
}

void ACombatGridManager::DestroyGrid()
{
    if (!HasAuthority())
    {
        return;
    }

    // Tile EndPlay unregisters itself, so detach the index before destruction.
    // 타일 EndPlay가 자신을 해제하므로 파괴 전에 색인을 분리합니다.
    TArray<ACombatGridTile*> Tiles;
    TileMap.GenerateValueArray(Tiles);
    TileMap.Reset();
    for (ACombatGridTile* Tile : Tiles)
    {
        if (IsValid(Tile))
        {
            Tile->Destroy();
        }
    }
}

void ACombatGridManager::RegisterReplicatedTile(ACombatGridTile* Tile)
{
    if (!IsValid(Tile) || Tile->GetWorld() != GetWorld() || Tile->GetGridManager() != this)
    {
        return;
    }

    UnregisterReplicatedTile(Tile);
    TileMap.Add(Tile->GridCoord, Tile);
    Tile->SetActorHiddenInGame(!bGridActive);
    Tile->SetActorEnableCollision(bGridActive);
}

void ACombatGridManager::UnregisterReplicatedTile(ACombatGridTile* Tile)
{
    for (auto It = TileMap.CreateIterator(); It; ++It)
    {
        if (It.Value() == Tile)
        {
            It.RemoveCurrent();
        }
    }
}

void ACombatGridManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyGrid();
    TileMap.Reset();
    Super::EndPlay(EndPlayReason);
}
