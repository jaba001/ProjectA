#include "TurnManager.h"
#include "Unit/UnitBase.h"

void UTurnManager::InitializeTurnOrder(const TArray<AUnitBase*>& Units)
{
    TurnOrder = Units;
    CurrentTurnIndex = 0;

    UE_LOG(LogTemp, Warning,
        TEXT("[TurnManager] InitializeTurnOrder Count=%d"),
        TurnOrder.Num());

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
        //UE_LOG(LogTemp, Warning,
        //    TEXT("[TurnManager] INVALID INDEX | %d"),
        //    CurrentTurnIndex);
        return;
    }

    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];

    if (!Unit || !Unit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TurnManager] SKIP DEAD UNIT | Index=%d"),
            CurrentTurnIndex);

        NextTurn();
        return;
    }

    TurnCounter++;

    UE_LOG(LogTemp, Warning,
        TEXT("[Turn %d] START | Index=%d | Unit=%s"),
        TurnCounter,
        CurrentTurnIndex,
        *Unit->GetName());

    Unit->OnTurnStart();
}

void UTurnManager::EndTurn()
{
    if (!TurnOrder.IsValidIndex(CurrentTurnIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TurnManager] EndTurn Invalid Index=%d"),
            CurrentTurnIndex);
        return;
    }

    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];

    if (!Unit)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TurnManager] END | NULL UNIT | Index=%d"),
            CurrentTurnIndex);

        NextTurn();
        return;
    }

    Unit->OnTurnEnd();

    UE_LOG(LogTemp, Warning,
        TEXT("[Turn %d] END   | Index=%d | Unit=%s"),
        TurnCounter,
        CurrentTurnIndex,
        *Unit->GetName());


    if (CheckCombatEnd())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== COMBAT END ==="));
        return;
    }

    NextTurn();
}

void UTurnManager::NextTurn()
{
    if (TurnOrder.Num() == 0)
        return;

    int32 StartIndex = CurrentTurnIndex;

    do
    {
        CurrentTurnIndex = (CurrentTurnIndex + 1) % TurnOrder.Num();

        AUnitBase* Unit = TurnOrder[CurrentTurnIndex];

        if (Unit && Unit->IsUnitAlive())
        {
            break;
        }

    } while (CurrentTurnIndex != StartIndex);

    //UE_LOG(LogTemp, Warning,
    //    TEXT("[TurnManager] NextTurn -> Index=%d"),
    //    CurrentTurnIndex);

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
            continue;

        if (Unit->GetTeam() == ETeam::Player)
        {
            bPlayerAlive = true;
        }
        else if (Unit->GetTeam() == ETeam::Enemy)
        {
            bEnemyAlive = true;
        }
    }

    // 한 팀만 남았으면 전투 종료
    if (!bPlayerAlive || !bEnemyAlive)
    {
        return true;
    }

    return false;
}