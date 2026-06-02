#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MainMenuRootWidget.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

// CommonUI root widget that owns menu stacks.
// 메뉴 스택들을 소유하는 CommonUI 루트 위젯입니다.
UCLASS()
class PROJECTA_API UMainMenuRootWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // Pushes a widget to the main stack.
    // 위젯을 메인 스택에 추가합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    UCommonActivatableWidget* PushMainScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass);

    // Pushes a widget to the menu stack.
    // 위젯을 메뉴 스택에 추가합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    UCommonActivatableWidget* PushMenuScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass);

    // Pushes a widget to the modal stack.
    // 위젯을 모달 스택에 추가합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    UCommonActivatableWidget* PushModalScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass);

    // Clears all widgets from the menu stack.
    // 메뉴 스택의 모든 위젯을 제거합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void ClearMenuStack();

    // Clears all widgets from the modal stack.
    // 모달 스택의 모든 위젯을 제거합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void ClearModalStack();

protected:
    // Primary screen stack bound from WBP_MainMenuRootWidget.
    // WBP_MainMenuRootWidget에서 바인딩되는 기본 화면 스택입니다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> MainStack;

    // Menu flow stack bound from WBP_MainMenuRootWidget.
    // WBP_MainMenuRootWidget에서 바인딩되는 메뉴 흐름 스택입니다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

    // Modal screen stack bound from WBP_MainMenuRootWidget.
    // WBP_MainMenuRootWidget에서 바인딩되는 모달 화면 스택입니다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> ModalStack;

private:
    // Shared stack push helper used by public push functions.
    // 공개 Push 함수들이 사용하는 공용 스택 추가 헬퍼입니다.
    UCommonActivatableWidget* PushScreen(UCommonActivatableWidgetStack* Stack, TSubclassOf<UCommonActivatableWidget> WidgetClass, const TCHAR* StackName);
};
