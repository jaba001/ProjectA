#include "UI/MainMenu/MainMenuRootWidget.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidget* UMainMenuRootWidget::PushMainScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
    return PushScreen(MainStack, WidgetClass, TEXT("MainStack"));
}

UCommonActivatableWidget* UMainMenuRootWidget::PushMenuScreen(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
    return PushScreen(MenuStack, WidgetClass, TEXT("MenuStack"));
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
