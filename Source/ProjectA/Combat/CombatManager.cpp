#include "Combat/CombatManager.h"
#include "Net/UnrealNetwork.h"
#include "Game/Turn/TurnManager.h"
#include "Unit/UnitBase.h"

ACombatManager::ACombatManager()
{
    PrimaryActorTick.bCanEverTick = false;
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

    if (TurnManager)
    {
        // 실제 유닛 전달
        TurnManager->InitializeTurnOrder(CombatUnits);   
        // 동기화
        CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();
    }
}

void ACombatManager::RegisterUnits(const TArray<AUnitBase*>& Units)
{
    // 서버 전용
    if (!HasAuthority()) return;  

    CombatUnits = Units;
}

void ACombatManager::AdvanceTurn()
{
    // 서버 전용
    if (!HasAuthority()) return;  

    if (TurnManager)
    {
        TurnManager->EndTurn();
        // 클라 동기화
        CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();  
    }
}

void ACombatManager::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACombatManager, CurrentTurnIndex);
}

AUnitBase* ACombatManager::GetCurrentUnit() const
{
    if (!CombatUnits.IsValidIndex(CurrentTurnIndex))
        return nullptr;

    return CombatUnits[CurrentTurnIndex];
}

void ACombatManager::RequestEndTurn()
{
    if (!HasAuthority())
    {
        return;
    }

    AdvanceTurn();
}