#include "Unit/EnemyUnit.h"

AEnemyUnit::AEnemyUnit()
{
    InitMaxHP = 150.0f;
    MaxActionPoint = 2;
}

void AEnemyUnit::OnTurnStart()
{
    // A legacy callback must never reactivate the removed automatic turn loop.
    // 기존 콜백이 제거된 자동 턴 루프를 다시 활성화하지 않도록 합니다.
    Super::OnTurnEnd();
}

void AEnemyUnit::SetTurnState(EEnemyTurnState NewState)
{
    UE_LOG(LogTemp, Warning, TEXT("[EnemyUnit] Sequential AI state changes are unsupported."));
}
