#include "TurnManager.h"
#include "Unit/UnitBase.h"

void UTurnManager::InitializeTurnOrder(const TArray<AUnitBase*>& Units)
{
    TurnOrder = Units;
    CurrentTurnIndex = 0;

    UE_LOG(LogTemp, Warning, TEXT("[UTurnManager::InitializeTurnOrder] Count=%d"), TurnOrder.Num());

    StartTurn();
}

void UTurnManager::StartTurn()
{
    if (!TurnOrder.IsValidIndex(CurrentTurnIndex))
        return;

    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];
    if (Unit)
    {
        Unit->OnTurnStart();
        UE_LOG(LogTemp, Warning, TEXT("[UTurnManager::StartTurn] %s Turn Begin"), *Unit->GetName());
    }
}

void UTurnManager::EndTurn()
{
    if (!TurnOrder.IsValidIndex(CurrentTurnIndex))
        return;

    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];
    if (Unit)
    {
        Unit->OnTurnEnd();
        UE_LOG(LogTemp, Warning, TEXT("[UTurnManager::EndTurn] %s Turn End"), *Unit->GetName());
    }

    NextTurn();
}

void UTurnManager::NextTurn()
{
    if (TurnOrder.Num() == 0)
        return;

    CurrentTurnIndex = (CurrentTurnIndex + 1) % TurnOrder.Num();

    UE_LOG(LogTemp, Warning, TEXT("[UTurnManager::NextTurn] Next Index=%d"), CurrentTurnIndex);

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
