#include "Combat/CombatManager.h"
#include "Net/UnrealNetwork.h"
#include "Game/Turn/TurnManager.h"

ACombatManager::ACombatManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // 네트워크 복제 활성화
    bReplicates = true;
}

void ACombatManager::BeginPlay()
{
    Super::BeginPlay();
}

void ACombatManager::Server_StartCombat_Implementation()
{
    if (!HasAuthority()) return;

    StartCombat_Internal();
}

void ACombatManager::StartCombat_Internal()
{
    if (!HasAuthority()) return;

    TurnManager = NewObject<UTurnManager>(this);

    // TODO: 유닛 배열 전달
    // TurnManager->InitializeTurnOrder(Units);

    CurrentTurnIndex = 0;
}

void ACombatManager::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACombatManager, CurrentTurnIndex);
}