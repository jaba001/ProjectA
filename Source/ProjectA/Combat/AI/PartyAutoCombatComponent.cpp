#include "Combat/AI/PartyAutoCombatComponent.h"

UPartyAutoCombatComponent::UPartyAutoCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = false;
}

void UPartyAutoCombatComponent::InitializeCombat(ACombatManager* InCombatManager)
{
}

void UPartyAutoCombatComponent::StartTurn()
{
    UE_LOG(LogTemp, Warning, TEXT("[PartyAutoCombat] Sequential AI was removed. The round coordinator selects one locked plan."));
}

void UPartyAutoCombatComponent::Stop()
{
}

void UPartyAutoCombatComponent::HandleActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
}
