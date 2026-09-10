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

    // Hides or restores the main stack while a menu flow screen is active.
    // 메뉴 흐름 화면이 활성화된 동안 메인 스택을 숨기거나 복원합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu")
    void SetMainStackHiddenByMenu(bool bShouldHide);

protected:
    // Initializes fallback stack layout when the designer layout is empty.
    // 디자이너 레이아웃이 비어 있을 때 대체 스택 레이아웃을 초기화합니다.
    virtual void NativeOnInitialized() override;

    // Primary screen stack bound from WBP_MainMenuRootWidget.
    // WBP_MainMenuRootWidget에서 바인딩되는 기본 화면 스택입니다.
    // T12: Verified MainStack binding in the saved Designer WBP.
    // T12: 저장된 Designer WBP의 MainStack 바인딩을 검증했습니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> MainStack;

    // Menu flow stack bound from WBP_MainMenuRootWidget.
    // WBP_MainMenuRootWidget에서 바인딩되는 메뉴 흐름 스택입니다.
    // T12: Verified MenuStack binding in the saved Designer WBP.
    // T12: 저장된 Designer WBP의 MenuStack 바인딩을 검증했습니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

    // Modal screen stack bound from WBP_MainMenuRootWidget.
    // WBP_MainMenuRootWidget에서 바인딩되는 모달 화면 스택입니다.
    // T12: Verified ModalStack binding in the saved Designer WBP.
    // T12: 저장된 Designer WBP의 ModalStack 바인딩을 검증했습니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> ModalStack;

    // Creates CommonUI stacks in C++ when no bound designer stacks exist.
    // 바인딩된 디자이너 스택이 없을 때 C++에서 CommonUI 스택을 생성합니다.
    UPROPERTY(EditDefaultsOnly, Category = "Main Menu|Code UI")
    bool bCreateStacksInCode = true;

private:
    // Ensures root overlay and stack widgets exist for native-only usage.
    // 네이티브 전용 사용을 위해 루트 오버레이와 스택 위젯이 존재하도록 보장합니다.
    void EnsureCodeGeneratedRootLayout();

    // Shared stack push helper used by public push functions.
    // 공개 Push 함수들이 사용하는 공용 스택 추가 헬퍼입니다.
    UCommonActivatableWidget* PushScreen(UCommonActivatableWidgetStack* Stack, TSubclassOf<UCommonActivatableWidget> WidgetClass, const TCHAR* StackName);

    // Applies the stored main stack visibility state.
    // 저장된 메인 스택 표시 상태를 적용합니다.
    void ApplyMainStackVisibility();

    UPROPERTY(Transient)
    bool bMainStackHiddenByMenu = false;
};
