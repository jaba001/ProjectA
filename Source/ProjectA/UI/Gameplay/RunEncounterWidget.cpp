#include "UI/Gameplay/RunEncounterWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Theme/DemonicUITheme.h"

TOptional<FUIInputConfig> URunEncounterWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void URunEncounterWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleBackdrop(Background);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EncounterPanel"));
    Theme.StylePanel(Panel);
    Panel->SetPadding(FMargin(32.0f));
    UScaleBox* ContentScale = WidgetTree->ConstructWidget<UScaleBox>();
    ContentScale->SetStretch(EStretch::ScaleToFit);
    ContentScale->SetStretchDirection(EStretchDirection::DownOnly);
    UScaleBoxSlot* ScaleSlot = CastChecked<UScaleBoxSlot>(ContentScale->AddChild(Panel));
    ScaleSlot->SetHorizontalAlignment(HAlign_Center);
    ScaleSlot->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(ContentScale);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    ContentSlot->SetPadding(FMargin(24.0f));
    USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>();
    ContentSize->SetMinDesiredWidth(600.0f);
    ContentSize->SetMaxDesiredWidth(680.0f);
    Panel->SetContent(ContentSize);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentSize->SetContent(Content);
    Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_EncounterTitle"));
    Content->AddChildToVerticalBox(Title);
    Theme.AddDivider(WidgetTree, Content);
    Actions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EncounterActions"));
    Content->AddChildToVerticalBox(Actions);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
        Button->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandleSelection);
        Actions->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
        ChoiceButtons.Add(Button);
    }
    LeaveButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_LeaveShop"));
    LeaveButton->Configure(TEXT("Leave"), NSLOCTEXT("RunEncounter", "Leave", "나가기"));
    LeaveButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandleLeave);
    Actions->AddChildToVerticalBox(LeaveButton)->SetPadding(FMargin(0.0f, 5.0f));
    Message = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_EncounterMessage"));
    Message->SetAutoWrapText(true);
    Message->SetWrapTextAt(600.f);
    Content->AddChildToVerticalBox(Message)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Title, true, 28);
}

void URunEncounterWidget::RefreshEncounter(const FGameplayViewState& View, bool bAllowRunCommands)
{
    if (!Actions) return;
    bRunCommandsAllowed = bAllowRunCommands;
    for (int32 Index = 0; Index < ChoiceButtons.Num(); ++Index)
    {
        UGameplayActionButton* Button = ChoiceButtons[Index];
        const bool bVisible = View.Phase == ERunPhase::EncounterChoice && View.EncounterProgress.Offers.IsValidIndex(Index);
        Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetIsEnabled(bVisible && bAllowRunCommands);
        if (bVisible)
        {
            const FRunEncounterOffer& Offer = View.EncounterProgress.Offers[Index];
            Button->Configure(Offer.EncounterId, Offer.DisplayName);
        }
    }
    LeaveButton->SetVisibility(View.Phase == ERunPhase::Shop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    LeaveButton->SetIsEnabled(View.Phase == ERunPhase::Shop && bAllowRunCommands);
    Message->SetText(View.FlowMessage.IsEmpty() && !bAllowRunCommands ? NSLOCTEXT("RunEncounter", "HostOnly", "Host의 진행을 기다리는 중입니다.") : View.FlowMessage);
    if (View.Phase == ERunPhase::EncounterChoice)
    {
        Title->SetText(NSLOCTEXT("RunEncounter", "Choose", "인카운터 선택"));
    }
    else if (View.Phase == ERunPhase::Shop)
    {
        const FRunEncounterOffer* Selected = View.EncounterProgress.Offers.FindByPredicate([&View](const FRunEncounterOffer& Offer) { return Offer.EncounterId == View.EncounterProgress.SelectedEncounterId; });
        Title->SetText(Selected ? Selected->DisplayName : NSLOCTEXT("RunEncounter", "Shop", "상점"));
    }
}

void URunEncounterWidget::HandleSelection(FName EncounterId)
{
    if (!bRunCommandsAllowed) return;
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>()) Controller->RequestSelectRunEncounter(EncounterId);
}

void URunEncounterWidget::HandleLeave(FName)
{
    if (!bRunCommandsAllowed) return;
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>()) Controller->RequestLeaveRunEncounter();
}
