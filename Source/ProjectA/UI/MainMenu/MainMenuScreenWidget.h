#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MainMenuScreenWidget.generated.h"

// Main menu screen widget base for blueprint button callbacks.
// 블루프린트 버튼 콜백을 위한 메인메뉴 화면 위젯 기반 클래스입니다.
UCLASS()
class PROJECTA_API UMainMenuScreenWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // Requests transition to character creation.
    // 캐릭터 생성 화면으로 전환을 요청합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void RequestNewGame();

    // Requests game quit behavior from the main menu.
    // 메인메뉴에서 게임 종료 동작을 요청합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void RequestQuitGame();
};
