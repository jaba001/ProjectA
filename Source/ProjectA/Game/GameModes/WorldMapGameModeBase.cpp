#include "Game/GameModes/WorldMapGameModeBase.h"

AWorldMapGameModeBase::AWorldMapGameModeBase()
{
    DefaultPawnClass = nullptr;
    SpectatorClass = nullptr;
}

void AWorldMapGameModeBase::RestartPlayer(AController* NewPlayer)
{
}
