#include "UI/MainMenu/MainMenuRootWidget.h"

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
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuRootWidget] WidgetTree is not available."));
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CodeGeneratedRootOverlay"));

    if (!RootOverlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuRootWidget] Failed to create root overlay."));
        return;
    }

    MainStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("MainStack"));
    MenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("MenuStack"));
    ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalStack"));

    if (!MainStack || !MenuStack || !ModalStack)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuRootWidget] Failed to create CommonUI stacks."));
        return;
    }

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
        UE_LOG(LogTemp, Warning, TEXT("ClearMenuStack failed because MenuStack is not bound."));
        return;
    }

    MenuStack->ClearWidgets();
    SetMainStackHiddenByMenu(false);
}

void UMainMenuRootWidget::ClearModalStack()
{
    if (!ModalStack)
    {
        UE_LOG(LogTemp, Warning, TEXT("ClearModalStack failed because ModalStack is not bound."));
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
        UE_LOG(LogTemp, Warning, TEXT("PushScreen failed because %s is not bound."), StackName);
        return nullptr;
    }

    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("PushScreen failed because WidgetClass is not set for %s."), StackName);
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
