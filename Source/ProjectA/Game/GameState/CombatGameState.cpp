#include "CombatGameState.h"
#include "Game/Turn/TurnManager.h"

ACombatGameState::ACombatGameState()
{
    TurnManager = nullptr;
}

void ACombatGameState::BeginPlay()
{
    Super::BeginPlay();

    CreateTurnManager();
}

void ACombatGameState::CreateTurnManager()
{
    if (TurnManager == nullptr)
    {
        TurnManager = NewObject<UTurnManager>(this, UTurnManager::StaticClass());

        if (TurnManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ACombatGameState::CreateTurnManager] TurnManager created."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[ACombatGameState::CreateTurnManager] Failed to create TurnManager."));
        }
    }
}
