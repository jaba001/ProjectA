#include "Game/Turn/TurnManager.h"

bool UTurnManager::RetryTurnCheckpoint()
{
    return false;
}

bool UTurnManager::RestoreFromBoundary(const TArray<AUnitBase*>& Units, int32 CompletedTurnSerial, int32 NextTurnIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Sequential checkpoint restoration is no longer supported."));
    return false;
}

void UTurnManager::InitializeTurnOrder(const TArray<AUnitBase*>& Units)
{
    StartTurn();
}

void UTurnManager::StartTurn()
{
    UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Sequential execution was removed. Use timed-round combat."));
}

void UTurnManager::EndTurn()
{
    StartTurn();
}

void UTurnManager::NextTurn()
{
    StartTurn();
}

void UTurnManager::SuspendForRecovery()
{
}

void UTurnManager::EvaluateCombatResult()
{
}

void UTurnManager::StopCombat()
{
}

void UTurnManager::ResetCombat()
{
    OnCombatResult.Clear();
    OnTurnChanged.Clear();
    CommitTurnBoundary.Unbind();
}
