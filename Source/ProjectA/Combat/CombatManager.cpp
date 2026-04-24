#include "Combat/CombatManager.h"
#include "Net/UnrealNetwork.h"
#include "Game/Turn/TurnManager.h"
#include "Controller/PartyPlayerController.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Queue.h"

ACombatManager::ACombatManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    CombatGridManager = nullptr;
}

void ACombatManager::BeginPlay()
{
    Super::BeginPlay();

    CombatGridManager = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatGridManager::StaticClass()));

    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CombatGridManager not found"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatManager] CombatGridManager initialized"));
}

void ACombatManager::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACombatManager, CurrentTurnIndex);
}


bool ACombatManager::CanUnitEnterTile(AUnitBase* Unit, ACombatGridTile* Tile) const
{
    if (!Unit || !Tile)
    {
        return false;
    }

    if (Tile == Unit->CurrentTile)
    {
        return true;
    }

    if (Tile->GetOccupyingUnit())
    {
        return false;
    }

    const ETileTerritory TileTerritory = Tile->GetTerritory();

    if (Unit->GetTeam() == ETeam::Player && TileTerritory != ETileTerritory::Player)
    {
        return false;
    }

    if (Unit->GetTeam() == ETeam::Enemy && TileTerritory != ETileTerritory::Enemy)
    {
        return false;
    }

    return true;
}

void ACombatManager::Server_StartCombat_Implementation()
{
    if (!HasAuthority()) return;

    StartCombat_Internal();
}

void ACombatManager::StartCombat_Internal()
{
    if (!HasAuthority())
    {
        return;
    }

    TurnManager = NewObject<UTurnManager>(this);

    if (!TurnManager)
    {
        return;
    }

    TurnManager->InitializeTurnOrder(CombatUnits);
    CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();
    RefreshReachableMoveTiles();
    RefreshTileProtectedByFront();
}

void ACombatManager::RegisterUnits(const TArray<AUnitBase*>& Units)
{
    if (!HasAuthority()) return;  

    CombatUnits = Units;
}

void ACombatManager::AdvanceTurn()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!TurnManager)
    {
        return;
    }

    TurnManager->EndTurn();
    //Ŭ�󵿱�ȭ
    CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();
    RefreshReachableMoveTiles();
    RefreshTileProtectedByFront();
}

void ACombatManager::RequestEndTurn()
{
    if (!HasAuthority())
    {
        return;
    }

    AdvanceTurn();
}

AUnitBase* ACombatManager::GetCurrentUnit() const
{
    if (!CombatUnits.IsValidIndex(CurrentTurnIndex))
        return nullptr;

    return CombatUnits[CurrentTurnIndex];
}

void ACombatManager::RefreshReachableMoveTiles()
{
    ReachableMoveTiles.Empty();

    AUnitBase* CurrentUnit = GetCurrentUnit();

    if (!CurrentUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshReachableMoveTiles failed | CurrentUnit is null"));
        return;
    }

    //UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshReachableMoveTiles | CurrentUnit=%s | MoveRange=%d"), *GetNameSafe(CurrentUnit), CurrentUnit->GetMoveRange());

    ReachableMoveTiles = CalculateReachableMoveTiles(CurrentUnit);

    //UE_LOG(LogTemp, Log, TEXT("[CombatManager] ReachableMoveTiles refreshed | Unit=%s | Count=%d"), *GetNameSafe(CurrentUnit), ReachableMoveTiles.Num());
}

bool ACombatManager::IsReachableMoveTile(ACombatGridTile* Tile) const
{
    if (!Tile)
    {
        return false;
    }

    return ReachableMoveTiles.Contains(Tile);
}

void ACombatManager::HighlightMovableTiles()
{
    for (ACombatGridTile* Tile : ReachableMoveTiles)
    {
        if (!Tile)
        {
            continue;
        }

        //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Highlight Tile | Coord=(%d,%d)"), Tile->GridCoord.X, Tile->GridCoord.Y);

        Tile->ApplyMovableTileVisual();
    }
}

void ACombatManager::ClearMovableTilesHighlight()
{
    for (ACombatGridTile* Tile : ReachableMoveTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ClearHighlightVisual();
    }
}

void ACombatManager::RefreshSkillTargetTiles()
{
    SkillTargetTiles.Empty();

    AUnitBase* CurrentUnit = GetCurrentUnit();

    if (!CurrentUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshSkillTargetTiles failed | CurrentUnit is null"));
        return;
    }

    SkillTargetTiles = CalculateSkillTargetTiles(CurrentUnit);

    //UE_LOG(LogTemp, Log, TEXT("[CombatManager] SkillTargetTiles refreshed | Unit=%s | Count=%d"), *GetNameSafe(CurrentUnit), SkillTargetTiles.Num());
}

bool ACombatManager::IsSkillTargetTile(ACombatGridTile* Tile) const
{
    if (!Tile)
    {
        return false;
    }

    return SkillTargetTiles.Contains(Tile);
}

void ACombatManager::HighlightSkillTargetTiles()
{
    for (ACombatGridTile* Tile : SkillTargetTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ApplySkillTargetTileVisual();
    }
}

void ACombatManager::ClearSkillTargetTilesHighlight()
{
    for (ACombatGridTile* Tile : SkillTargetTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ClearHighlightVisual();
    }

    SkillTargetTiles.Empty();
}

ACombatGridTile* ACombatManager::GetTileByCoord(FIntPoint Coord) const
{
    if (!CombatGridManager)
    {
        return nullptr;
    }

    ACombatGridTile* const* FoundTile = CombatGridManager->TileMap.Find(Coord);

    if (!FoundTile)
    {
        return nullptr;
    }

    return *FoundTile;
}

void ACombatManager::RefreshTileProtectedByFront()
{
    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshTileProtectedByFront failed | CombatGridManager is null"));
        return;
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& TilePair : CombatGridManager->TileMap)
    {
        ACombatGridTile* Tile = TilePair.Value;

        if (!Tile)
        {
            continue;
        }

        Tile->SetProtectedByFront(false);
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& TilePair : CombatGridManager->TileMap)
    {
        ACombatGridTile* Tile = TilePair.Value;

        if (!Tile)
        {
            continue;
        }

        const ETileTerritory Territory = Tile->GetTerritory();
        const FIntPoint Coord = Tile->GridCoord;

        ACombatGridTile* FrontTile = nullptr;

        if (Territory == ETileTerritory::Player)
        {
            if (Coord.Y != 0)
            {
                continue;
            }

            FrontTile = GetTileByCoord(FIntPoint(Coord.X, 1));
        }
        else if (Territory == ETileTerritory::Enemy)
        {
            if (Coord.Y != 3)
            {
                continue;
            }

            FrontTile = GetTileByCoord(FIntPoint(Coord.X, 2));
        }
        else
        {
            continue;
        }

        if (!FrontTile)
        {
            continue;
        }

        AUnitBase* FrontUnit = FrontTile->GetOccupyingUnit();

        if (!FrontUnit)
        {
            continue;
        }

        if (!FrontUnit->IsUnitAlive())
        {
            continue;
        }

        Tile->SetProtectedByFront(true);

        //UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Tile Check | Coord=(%d,%d) | Territory=%d"), Coord.X, Coord.Y, static_cast<int32>(Territory));
    }
}

TArray<ACombatGridTile*> ACombatManager::CalculateSkillTargetTiles(AUnitBase* Unit) const
{
    TArray<ACombatGridTile*> Result;

    if (!Unit)
    {
        return Result;
    }

    if (!CombatGridManager)
    {
        return Result;
    }

    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());

    if (!PC)
    {
        return Result;
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& TilePair : CombatGridManager->TileMap)
    {
        ACombatGridTile* Tile = TilePair.Value;

        if (!Tile)
        {
            continue;
        }

        if (PC->IsValidTileForPendingSkill(Tile))
        {
            Result.Add(Tile);
        }
    }

    return Result;
}


TArray<ACombatGridTile*> ACombatManager::CalculateReachableMoveTiles(AUnitBase* Unit) const
{
    TArray<ACombatGridTile*> Result;

    if (!Unit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CalculateReachableMoveTiles failed | Unit is null"));
        return Result;
    }

    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CalculateReachableMoveTiles failed | CombatGridManager is null"));
        return Result;
    }

    ACombatGridTile* StartTile = Unit->CurrentTile;

    if (!StartTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CalculateReachableMoveTiles failed | StartTile is null | Unit=%s"), *GetNameSafe(Unit));
        return Result;
    }

    //UE_LOG(LogTemp, Log, TEXT("[CombatManager] CalculateReachableMoveTiles | StartTile=(%d,%d)"), StartTile->GridCoord.X, StartTile->GridCoord.Y);

    const int32 MaxMoveRange = Unit->GetMoveRange();

    TQueue<TPair<ACombatGridTile*, int32>> SearchQueue;
    TSet<ACombatGridTile*> VisitedTiles;

    SearchQueue.Enqueue(TPair<ACombatGridTile*, int32>(StartTile, 0));
    VisitedTiles.Add(StartTile);

    while (!SearchQueue.IsEmpty())
    {
        TPair<ACombatGridTile*, int32> CurrentPair;
        SearchQueue.Dequeue(CurrentPair);

        ACombatGridTile* CurrentTile = CurrentPair.Key;
        const int32 CurrentDistance = CurrentPair.Value;

        if (!CurrentTile)
        {
            continue;
        }

        if (CurrentTile != StartTile)
        {
            Result.Add(CurrentTile);
        }

        if (CurrentDistance >= MaxMoveRange)
        {
            continue;
        }

        const TArray<ACombatGridTile*> AdjacentTiles = CombatGridManager->GetAdjacentTiles(CurrentTile);

        for (ACombatGridTile* NextTile : AdjacentTiles)
        {
            if (!NextTile)
            {
                continue;
            }

            //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Check Adjacent Tile | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);

            if (VisitedTiles.Contains(NextTile))
            {
                //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Skip Tile | Reason=Visited | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);
                continue;
            }

            if (!CanUnitEnterTile(Unit, NextTile))
            {
                //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Skip Tile | Reason=Blocked | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);
                continue;
            }

            //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Add Reachable Tile | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);

            VisitedTiles.Add(NextTile);
            SearchQueue.Enqueue(TPair<ACombatGridTile*, int32>(NextTile, CurrentDistance + 1));
        }
    }

    return Result;
}