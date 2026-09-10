#include "TurnManager.h"
#include "Unit/UnitBase.h"

void UTurnManager::InitializeTurnOrder(const TArray<AUnitBase*>& Units)
{
    TurnOrder = Units;
    CurrentTurnIndex = 0;
    TurnCounter = 0;
    CombatResult = ECombatResult::None;
    bCombatActive = true;
    bAwaitingCheckpoint = false;
    bSuspended = false;
    bTurnStarted = false;

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
    if (!bCombatActive || bAwaitingCheckpoint || bSuspended || bTurnStarted || bCommittingCheckpoint)
    {
        return;
    }
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

    // Commit the inactive boundary before AP reset or synchronous enemy decisions can run.
    // AP 초기화나 동기 적 판단이 실행되기 전에 비활성 턴 경계를 확정합니다.
    if (CommitTurnBoundary.IsBound())
    {
        bAwaitingCheckpoint = true;
        OnTurnChanged.Broadcast();
        RetryTurnCheckpoint();
        return;
    }
    ActivatePreparedTurn();
}

void UTurnManager::ActivatePreparedTurn()
{
    if (!bCombatActive || bSuspended || bTurnStarted || !TurnOrder.IsValidIndex(CurrentTurnIndex) || !IsValid(TurnOrder[CurrentTurnIndex]) || !TurnOrder[CurrentTurnIndex]->IsUnitAlive() || TurnCounter == MAX_int32)
    {
        return;
    }
    AUnitBase* Unit = TurnOrder[CurrentTurnIndex];
    bAwaitingCheckpoint = false;
    bTurnStarted = true;
    TurnCounter++;

    UE_LOG(LogTemp, Log, TEXT("[Turn %d] START | Index=%d | Unit=%s"), TurnCounter, CurrentTurnIndex, *Unit->GetName());

    Unit->OnTurnStart();
    OnTurnChanged.Broadcast();
}

bool UTurnManager::RetryTurnCheckpoint()
{
    if (!bCombatActive || !bAwaitingCheckpoint || bSuspended || bCommittingCheckpoint || !CommitTurnBoundary.IsBound())
    {
        return false;
    }
    const int32 ExpectedCounter = TurnCounter;
    const int32 ExpectedIndex = CurrentTurnIndex;
    bool bSaved = false;
    {
        TGuardValue<bool> CommitGuard(bCommittingCheckpoint, true);
        bSaved = CommitTurnBoundary.Execute(ExpectedCounter, ExpectedIndex);
    }
    if (!bSaved || !bCombatActive || bSuspended || !bAwaitingCheckpoint || TurnCounter != ExpectedCounter || CurrentTurnIndex != ExpectedIndex)
    {
        return false;
    }
    ActivatePreparedTurn();
    return true;
}

bool UTurnManager::RestoreFromBoundary(const TArray<AUnitBase*>& Units, int32 CompletedTurnSerial, int32 NextTurnIndex)
{
    if (bCombatActive || bCommittingCheckpoint || CompletedTurnSerial < 0 || CompletedTurnSerial == MAX_int32 || !Units.IsValidIndex(NextTurnIndex) || !IsValid(Units[NextTurnIndex]) || !Units[NextTurnIndex]->IsUnitAlive())
    {
        return false;
    }
    TSet<AUnitBase*> UniqueUnits;
    for (AUnitBase* Unit : Units)
    {
        if (!IsValid(Unit) || Unit->IsActiveTurn() || Unit->IsBusy() || UniqueUnits.Contains(Unit))
        {
            return false;
        }
        UniqueUnits.Add(Unit);
    }
    TurnOrder = Units;
    CurrentTurnIndex = NextTurnIndex;
    TurnCounter = CompletedTurnSerial;
    CombatResult = ECombatResult::None;
    bCombatActive = true;
    bAwaitingCheckpoint = false;
    bSuspended = false;
    bTurnStarted = false;
    // The imported boundary is already durable; activate it once without writing or rerolling it again.
    // 가져온 경계는 이미 저장됐으므로 다시 저장하거나 추첨하지 않고 한 번 활성화합니다.
    ActivatePreparedTurn();
    return true;
}

void UTurnManager::SuspendForRecovery()
{
    bSuspended = true;
    bAwaitingCheckpoint = false;
    bTurnStarted = false;
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

void UTurnManager::EndTurn()
{
    if (!IsCombatActive() || !bTurnStarted || bCommittingCheckpoint)
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
        bTurnStarted = false;
        NextTurn();
        return;
    }

    Unit->OnTurnEnd();
    bTurnStarted = false;

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
    if (!bCombatActive || bAwaitingCheckpoint || bSuspended || bTurnStarted || bCommittingCheckpoint)
    {
        return;
    }
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
    if (!IsCombatActive())
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
    if (!bCombatActive || bAwaitingCheckpoint || bSuspended || bCommittingCheckpoint || !CheckCombatEnd())
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
    bAwaitingCheckpoint = false;
    bTurnStarted = false;
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
    bSuspended = false;
    OnCombatResult.Clear();
    OnTurnChanged.Clear();
    CommitTurnBoundary.Unbind();
}
