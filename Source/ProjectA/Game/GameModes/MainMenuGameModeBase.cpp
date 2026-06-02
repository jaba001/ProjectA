#include "Game/GameModes/MainMenuGameModeBase.h"

#include "Controller/MainMenuPlayerController.h"

AMainMenuGameModeBase::AMainMenuGameModeBase()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}
