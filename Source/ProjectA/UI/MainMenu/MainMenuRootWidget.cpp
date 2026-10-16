#include "UI/MainMenu/MainMenuRootWidget.h"
#include "ProjectA.h"

#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UMainMenuRootWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureCodeGeneratedRootLayout();
    ApplyMainStackVisibility();
}

void UMainMenuRootWidget::EnsureCodeGeneratedRootLayout()
{
    if (MainStack && MenuStack && ModalStack)
    {
        return;
    }

    if (!bCreateStacksInCode)
    {
        return;
    }

    if (!WidgetTree)
    {
        UE_LOG(LogProjectA, Warning, TEXT("[MainMenuRootWidget] WidgetTree is not available."));
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CodeGeneratedRootOverlay"));

    if (!RootOverlay)
    {
        UE_LOG(LogProjectA, Warning, TEXT("[MainMenuRootWidget] Failed to create root overlay."));
        return;
    }

    MainStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("MainStack"));
    MenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("MenuStack"));
    ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalStack"));

    if (!MainStack || !MenuStack || !ModalStack)
    {
        UE_LOG(LogProjectA, Warning, TEXT("[MainMenuRootWidget] Failed to create CommonUI stacks."));
        return;
    }
    // Replace screens instantly so the previous menu never shows through the default crossfade.
    // 기본 교차 페이드로 이전 메뉴가 비쳐 보이지 않도록 화면을 즉시 교체합니다.
    for (UCommonActivatableWidgetStack* Stack : { MainStack.Get(), MenuStack.Get(), ModalStack.Get() }) Stack->SetTransitionDuration(0.f);

    WidgetTree->RootWidget = RootOverlay;

    UOverlaySlot* MainStackSlot = RootOverlay->AddChildToOverlay(MainStack);
    UOverlaySlot* MenuStackSlot = RootOverlay->AddChildToOverlay(MenuStack);
    UOverlaySlot* ModalStackSlot = RootOverlay->AddChildToOverlay(ModalStack);

    if (MainStackSlot)
    {
        MainStackSlot->SetHorizontalAlignment(HAlign_Fill);
        MainStackSlot->SetVerticalAlignment(VAlign_Fill);
    }

    if (MenuStackSlot)
    {
        MenuStackSlot->SetHorizontalAlignment(HAlign_Fill);
        MenuStackSlot->SetVerticalAlignment(VAlign_Fill);
    }

    if (ModalStackSlot)
    {
        ModalStackSlot->SetHorizontalAlignment(HAlign_Fill);
        ModalStackSlot->SetVerticalAlignment(VAlign_Fill);
    }
}

UCommonActivatableWidget* UMainMenuRootWidget::PushMainScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
    SetMainStackHiddenByMenu(false);
    return PushScreen(MainStack, WidgetClass, TEXT("MainStack"));
}

UCommonActivatableWidget* UMainMenuRootWidget::PushMenuScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
    UCommonActivatableWidget* PushedWidget = PushScreen(MenuStack, WidgetClass, TEXT("MenuStack"));

    if (PushedWidget)
    {
        SetMainStackHiddenByMenu(true);
    }

    return PushedWidget;
}

UCommonActivatableWidget* UMainMenuRootWidget::PushModalScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
    return PushScreen(ModalStack, WidgetClass, TEXT("ModalStack"));
}

void UMainMenuRootWidget::ClearMenuStack()
{
    if (!MenuStack)
    {
        UE_LOG(LogProjectA, Warning, TEXT("ClearMenuStack failed because MenuStack is not bound."));
        return;
    }

    MenuStack->ClearWidgets();
    SetMainStackHiddenByMenu(false);
}

void UMainMenuRootWidget::ClearModalStack()
{
    if (!ModalStack)
    {
        UE_LOG(LogProjectA, Warning, TEXT("ClearModalStack failed because ModalStack is not bound."));
        return;
    }

    ModalStack->ClearWidgets();
}

void UMainMenuRootWidget::SetMainStackHiddenByMenu(bool bShouldHide)
{
    bMainStackHiddenByMenu = bShouldHide;
    ApplyMainStackVisibility();
}

UCommonActivatableWidget* UMainMenuRootWidget::PushScreen(UCommonActivatableWidgetStack* Stack, TSubclassOf<UCommonActivatableWidget> WidgetClass, const TCHAR* StackName)
{
    if (!Stack)
    {
        UE_LOG(LogProjectA, Warning, TEXT("PushScreen failed because %s is not bound."), StackName);
        return nullptr;
    }

    if (!WidgetClass)
    {
        UE_LOG(LogProjectA, Warning, TEXT("PushScreen failed because WidgetClass is not set for %s."), StackName);
        return nullptr;
    }

    return Stack->AddWidget(WidgetClass);
}

void UMainMenuRootWidget::ApplyMainStackVisibility()
{
    if (!MainStack)
    {
        return;
    }

    if (bMainStackHiddenByMenu)
    {
        MainStack->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    MainStack->SetVisibility(ESlateVisibility::Visible);
}
