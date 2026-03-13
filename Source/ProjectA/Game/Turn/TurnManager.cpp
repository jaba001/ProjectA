#include "TurnManager.h"
#include "Unit/UnitBase.h"

void UTurnManager::InitializeTurnOrder(const TArray<AUnitBase*>& Units)
{
    TurnOrder = Units;
    CurrentTurnIndex = 0;
    TurnCounter = 0;

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] InitializeTurnOrder Count=%d"), TurnOrder.Num());

    // 모든 유닛 턴 상태 초기화
    for (AUnitBase* Unit : TurnOrder)
    {
        if (Unit)
        {
            Unit->OnTurnEnd();
        }
    }

    StartTurn();
}

void UTurnManager::StartTurn()
{
    if (!TurnOrder.IsValidIndex(CurrentTurnIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] StartTurn Failed | Invalid Index=%d"), CurrentTurnIndex);
        return;
    }

    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];

    if (!Unit || !Unit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] StartTurn Skip | Dead Or Null Unit | Index=%d"), CurrentTurnIndex);
        NextTurn();
        return;
    }

    TurnCounter++;

    UE_LOG(LogTemp, Log, TEXT("[Turn %d] START | Index=%d | Unit=%s"), TurnCounter, CurrentTurnIndex, *Unit->GetName());

    Unit->OnTurnStart();
}

void UTurnManager::EndTurn()
{
    if (!TurnOrder.IsValidIndex(CurrentTurnIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] EndTurn Failed | Invalid Index=%d"), CurrentTurnIndex);
        return;
    }

    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];

    if (!Unit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] EndTurn Failed | Null Unit | Index=%d"), CurrentTurnIndex);
        NextTurn();
        return;
    }

    Unit->OnTurnEnd();

    UE_LOG(LogTemp, Log, TEXT("[Turn %d] END | Index=%d | Unit=%s"), TurnCounter, CurrentTurnIndex, *Unit->GetName());

    if (CheckCombatEnd())
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat End"));
        return;
    }

    NextTurn();
}

void UTurnManager::NextTurn()
{
    if (TurnOrder.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] NextTurn Failed | TurnOrder is Empty"));
        return;
    }

    int32 AliveUnitCount = 0;

    for (AUnitBase* Unit : TurnOrder)
    {
        if (Unit && Unit->IsUnitAlive())
        {
            AliveUnitCount++;
        }
    }

    if (AliveUnitCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] NextTurn Failed | No Alive Units"));
        return;
    }

    const int32 StartIndex = CurrentTurnIndex;
    bool bFoundNextAliveUnit = false;

    do
    {
        CurrentTurnIndex = (CurrentTurnIndex + 1) % TurnOrder.Num();

        AUnitBase* Unit = TurnOrder[CurrentTurnIndex];

        if (Unit && Unit->IsUnitAlive())
        {
            bFoundNextAliveUnit = true;
            break;
        }

    } while (CurrentTurnIndex != StartIndex);

    if (!bFoundNextAliveUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] NextTurn Failed | Could Not Find Alive Unit"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] NextTurn Success | NewIndex=%d | Unit=%s"), CurrentTurnIndex, *GetNameSafe(TurnOrder[CurrentTurnIndex]));

    StartTurn();
}

AUnitBase* UTurnManager::GetCurrentUnit() const
{
    if (TurnOrder.IsValidIndex(CurrentTurnIndex))
    {
        return TurnOrder[CurrentTurnIndex];
    }

    return nullptr;
}

bool UTurnManager::CheckCombatEnd() const
{
    bool bPlayerAlive = false;
    bool bEnemyAlive = false;

    for (AUnitBase* Unit : TurnOrder)
    {
        if (!Unit || !Unit->IsUnitAlive())
        {
            continue;
        }

        if (Unit->GetTeam() == ETeam::Player)
        {
            bPlayerAlive = true;
        }
        else if (Unit->GetTeam() == ETeam::Enemy)
        {
            bEnemyAlive = true;
        }
    }

    if (!bPlayerAlive || !bEnemyAlive)
    {
        return true;
    }

    return false;
}

FString UTurnManager::GetCurrentUnitName() const
{
    AUnitBase* CurrentUnit = GetCurrentUnit();

    if (!CurrentUnit)
    {
        return TEXT("None");
    }

    return CurrentUnit->GetName();
}
