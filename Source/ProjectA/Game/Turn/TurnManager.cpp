#include "TurnManager.h"
#include "Unit/UnitBase.h"

void UTurnManager::InitializeTurnOrder(const TArray<AUnitBase*>& Units)
{
    TurnOrder = Units;
    CurrentTurnIndex = 0;
    TurnCounter = 0;
    CombatResult = ECombatResult::None;
    bCombatActive = true;

    UE_LOG(LogTemp, Log, TEXT("[TurnManager] InitializeTurnOrder Count=%d"), TurnOrder.Num());

    // Reset the turn state of all units
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
    EvaluateCombatResult();
    if (!bCombatActive)
    {
        return;
    }

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
    OnTurnChanged.Broadcast();
}

void UTurnManager::EndTurn()
{
    if (!bCombatActive)
    {
        return;
    }

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

    EvaluateCombatResult();
    if (!bCombatActive)
    {
        UE_LOG(LogTemp, Log, TEXT("[TurnManager] Combat End"));
        return;
    }

    NextTurn();
}

void UTurnManager::NextTurn()
{
    EvaluateCombatResult();
    if (!bCombatActive)
    {
        return;
    }

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
    if (!bCombatActive)
    {
        return nullptr;
    }

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

    if (!CurrentUnit->RuntimeCharacterName.IsEmpty())
    {
        return CurrentUnit->RuntimeCharacterName.ToString();
    }
    return CurrentUnit->GetName();
}

void UTurnManager::EvaluateCombatResult()
{
    if (!bCombatActive || !CheckCombatEnd())
    {
        return;
    }

    CombatResult = ECombatResult::Defeat;
    for (AUnitBase* Unit : TurnOrder)
    {
        if (IsValid(Unit) && Unit->IsUnitAlive() && Unit->GetTeam() == ETeam::Player)
        {
            CombatResult = ECombatResult::Victory;
            break;
        }
    }

    // Lock turns before notifying listeners that may cancel abilities or destroy actors.
    // 어빌리티 취소나 액터 정리를 수행하는 수신자에게 알리기 전에 턴을 잠급니다.
    StopCombat();
    OnCombatResult.Broadcast(CombatResult);
}

void UTurnManager::StopCombat()
{
    bCombatActive = false;
    for (AUnitBase* Unit : TurnOrder)
    {
        if (IsValid(Unit))
        {
            Unit->OnTurnEnd();
        }
    }
    OnTurnChanged.Broadcast();
}

void UTurnManager::ResetCombat()
{
    StopCombat();
    TurnOrder.Reset();
    CurrentTurnIndex = INDEX_NONE;
    TurnCounter = 0;
    CombatResult = ECombatResult::None;
    OnCombatResult.Clear();
    OnTurnChanged.Clear();
}
