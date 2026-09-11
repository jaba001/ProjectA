#include "Game/GameModes/MainMenuGameModeBase.h"

#include "Controller/MainMenuPlayerController.h"
#include "GameFramework/HUD.h"

AMainMenuGameModeBase::AMainMenuGameModeBase()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}

void AMainMenuGameModeBase::InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer)
{
    // Skip the null AHUD spawn request while preserving optional engine HUD support.
    // 비어 있는 AHUD 생성 요청을 생략하고 선택적으로 지정한 엔진 HUD 지원은 유지합니다.
    if (NewPlayer && HUDClass)
    {
        Super::InitializeHUDForPlayer_Implementation(NewPlayer);
    }
}
