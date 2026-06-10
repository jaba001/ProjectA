#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MainMenuScreenWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

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

protected:
    // Initializes fallback menu layout and button events.
    // 대체 메뉴 레이아웃과 버튼 이벤트를 초기화합니다.
    virtual void NativeOnInitialized() override;

    // Enables native C++ layout creation when designer widgets are absent.
    // 디자이너 위젯이 없을 때 네이티브 C++ 레이아웃 생성을 활성화합니다.
    UPROPERTY(EditDefaultsOnly, Category = "Main Menu|Code UI")
    bool bCreateLayoutInCode = true;

    // Title text optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 제목 텍스트입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Title;

    // New Game button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 새 게임 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_NewGame;

    // Continue button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 이어하기 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Continue;

    // Options button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 옵션 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Options;

    // Quit button optionally bound from a designer widget.
    // 디자이너 위젯에서 선택적으로 바인딩되는 종료 버튼입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Quit;

private:
    // Creates the fallback main menu layout in C++.
    // C++에서 대체 메인메뉴 레이아웃을 생성합니다.
    void EnsureCodeGeneratedLayout();

    // Creates a menu button and attaches it to the parent box.
    // 메뉴 버튼을 생성하고 부모 박스에 추가합니다.
    UButton* CreateMenuButton(UVerticalBox* ParentBox, const FText& ButtonText);

    // Creates button text and attaches it to the parent button.
    // 버튼 텍스트를 생성하고 부모 버튼에 추가합니다.
    UTextBlock* CreateButtonText(UButton* ParentButton, const FText& ButtonText);

    UFUNCTION()
    void HandleNewGameClicked();

    UFUNCTION()
    void HandleContinueClicked();

    UFUNCTION()
    void HandleOptionsClicked();

    UFUNCTION()
    void HandleQuitClicked();
};
