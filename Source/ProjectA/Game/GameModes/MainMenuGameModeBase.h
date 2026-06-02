#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameModeBase.generated.h"

// Main menu only game mode.
// 메인메뉴 전용 게임 모드입니다.
UCLASS()
class PROJECTA_API AMainMenuGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    // Sets main menu defaults such as player controller and no pawn.
    // 플레이어 컨트롤러와 폰 없음 같은 메인메뉴 기본값을 설정합니다.
    AMainMenuGameModeBase();
};
