#include "UI/MainMenu/RunSurrenderWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"
#include "UI/Theme/DemonicUITheme.h"

URunSurrenderWidget::URunSurrenderWidget()
{
    bIsBackHandler = true;
    bIsModal = true;
}

TOptional<FUIInputConfig> URunSurrenderWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UButton* URunSurrenderWidget::AddButton(UVerticalBox* Parent, FName Name, const FText& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetJustification(ETextJustify::Center);
    CastChecked<UButtonSlot>(Button->AddChild(Label))->SetPadding(FMargin(24.0f, 14.0f));
    Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 6.0f));
    return Button;
}

void URunSurrenderWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    Root->SetVisibility(ESlateVisibility::Visible);
    WidgetTree->RootWidget = Root;
    // A full-screen hit-testable backdrop prevents clicks from reaching the menu underneath.
    // 전체 화면 히트 테스트 배경으로 아래 메뉴에 클릭이 전달되지 않도록 합니다.
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SurrenderInputBlocker"));
    Background->SetVisibility(ESlateVisibility::Visible);
    Theme.StyleBackdrop(Background);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(620.0f);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Size);
    ContentSlot->SetHorizontalAlignment(HAlign_Center);
    ContentSlot->SetVerticalAlignment(VAlign_Center);
    ContentSlot->SetPadding(FMargin(24.0f));
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Theme.StylePanel(Panel);
    Panel->SetPadding(FMargin(40.0f));
    Size->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("RunSurrender", "Title", "여정 항복"));
    Title->SetJustification(ETextJustify::Center);
    Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
    UTextBlock* Description = WidgetTree->ConstructWidget<UTextBlock>();
    Description->SetText(NSLOCTEXT("RunSurrender", "Description", "현재 저장된 싱글플레이 여정을 포기합니다. 이후에는 이어할 수 없습니다."));
    Description->SetAutoWrapText(true);
    Description->SetJustification(ETextJustify::Center);
    Content->AddChildToVerticalBox(Description)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
    ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_SurrenderError"));
    ErrorText->SetAutoWrapText(true);
    ErrorText->SetVisibility(ESlateVisibility::Collapsed);
    Content->AddChildToVerticalBox(ErrorText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

    CancelButton = AddButton(Content, TEXT("Button_SurrenderCancel"), NSLOCTEXT("RunSurrender", "Cancel", "돌아가기"));
    CancelButton->OnClicked.AddUniqueDynamic(this, &URunSurrenderWidget::HandleCancel);
    ConfirmButton = AddButton(Content, TEXT("Button_SurrenderConfirm"), NSLOCTEXT("RunSurrender", "Confirm", "항복하기"));
    ConfirmButton->OnClicked.AddUniqueDynamic(this, &URunSurrenderWidget::HandleConfirm);
    ConfirmButton->SetIsEnabled(!ConfirmationToken.IsEmpty());
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Title, true, 30);
    Theme.StyleText(Description, false, 18);
    Theme.StyleText(ErrorText, false, 16);
    Theme.StyleButton(CancelButton, true);
}

void URunSurrenderWidget::ConfigureConfirmation(const FString& Token)
{
    if (bSubmitting) return;
    ConfirmationToken = Token;
    if (ErrorText)
    {
        ErrorText->SetText(FText::GetEmpty());
        ErrorText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ConfirmButton) ConfirmButton->SetIsEnabled(!ConfirmationToken.IsEmpty());
}

void URunSurrenderWidget::HandleConfirm()
{
    if (!IsActivated() || bSubmitting || ConfirmationToken.IsEmpty()) return;
    TGuardValue<bool> SubmissionGuard(bSubmitting, true);
    const FString SubmittedToken = ConfirmationToken;
    ConfirmButton->SetIsEnabled(false);
    CancelButton->SetIsEnabled(false);
    FText Error;
    AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    const bool bSucceeded = Controller && Controller->SurrenderSavedGame(SubmittedToken, Error);
    if (!IsActivated()) return;
    if (!bSucceeded)
    {
        ErrorText->SetText(Error.IsEmpty() ? NSLOCTEXT("RunSurrender", "Failed", "항복 요청을 처리하지 못했습니다. 다시 시도해 주세요.") : Error);
        ErrorText->SetVisibility(ESlateVisibility::Visible);
        ConfirmButton->SetIsEnabled(!ConfirmationToken.IsEmpty());
        CancelButton->SetIsEnabled(true);
        return;
    }
    // Keep confirmation blocked while listeners react to the completed surrender.
    // 항복 완료에 반응하는 처리 중에도 확인 요청을 차단합니다.
    OnSurrendered.Broadcast();
    DeactivateWidget();
}

void URunSurrenderWidget::HandleCancel()
{
    if (IsActivated() && !bSubmitting) DeactivateWidget();
}

bool URunSurrenderWidget::NativeOnHandleBackAction()
{
    HandleCancel();
    return true;
}

UWidget* URunSurrenderWidget::NativeGetDesiredFocusTarget() const
{
    return CancelButton;
}

FReply URunSurrenderWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    return FReply::Handled();
}

FReply URunSurrenderWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    return FReply::Handled();
}

void URunSurrenderWidget::ResetConfirmation()
{
    ConfirmationToken.Empty();
    OnSurrendered.Clear();
    if (ConfirmButton) ConfirmButton->SetIsEnabled(false);
    if (CancelButton) CancelButton->SetIsEnabled(true);
    if (ErrorText)
    {
        ErrorText->SetText(FText::GetEmpty());
        ErrorText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void URunSurrenderWidget::NativeOnDeactivated()
{
    ResetConfirmation();
    Super::NativeOnDeactivated();
}

void URunSurrenderWidget::NativeDestruct()
{
    ResetConfirmation();
    Super::NativeDestruct();
}
