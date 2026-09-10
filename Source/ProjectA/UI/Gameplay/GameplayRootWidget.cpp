#include "UI/Gameplay/GameplayRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Combat/CombatHUDWidget.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UGameplayRootWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!RunMapWidgetClass)
    {
        RunMapWidgetClass = URunMapWidget::StaticClass();
    }

    if (!CombatHUDWidgetClass)
    {
        CombatHUDWidgetClass = UCombatHUDWidget::StaticClass();
    }

    if (!ResultWidgetClass)
    {
        ResultWidgetClass = UEncounterResultWidget::StaticClass();
    }

    if (!RunLayer || !CombatLayer || !ModalLayer)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = Root;
        RunLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("RunLayer"));
        CombatLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("CombatLayer"));
        ModalLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalLayer"));

        for (UCommonActivatableWidgetStack* Layer : { RunLayer.Get(), CombatLayer.Get(), ModalLayer.Get() })
        {
            UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer);
            LayerSlot->SetHorizontalAlignment(HAlign_Fill);
            LayerSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }

    UWidget* ExistingRoot = WidgetTree->RootWidget;
    ExistingRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UOverlay* NoticeRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CheckpointOverlay"));
    WidgetTree->RootWidget = NoticeRoot;
    UOverlaySlot* ContentSlot = NoticeRoot->AddChildToOverlay(ExistingRoot);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    CheckpointNotice = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CheckpointNotice"));
    UOverlaySlot* NoticeSlot = NoticeRoot->AddChildToOverlay(CheckpointNotice);
    NoticeSlot->SetHorizontalAlignment(HAlign_Center);
    NoticeSlot->SetVerticalAlignment(VAlign_Top);
    CheckpointNotice->SetPadding(FMargin(16.f));
    UVerticalBox* NoticeContent = WidgetTree->ConstructWidget<UVerticalBox>();
    CheckpointNotice->SetContent(NoticeContent);
    CheckpointMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_CheckpointMessage"));
    CheckpointMessage->SetAutoWrapText(true);
    CheckpointMessage->SetWrapTextAt(680.f);
    NoticeContent->AddChildToVerticalBox(CheckpointMessage);
    RetryCheckpointButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_RetryCheckpoint"));
    UTextBlock* RetryLabel = WidgetTree->ConstructWidget<UTextBlock>();
    RetryLabel->SetText(FText::FromString(TEXT("저장 다시 시도")));
    RetryCheckpointButton->SetContent(RetryLabel);
    RetryCheckpointButton->OnClicked.AddDynamic(this, &UGameplayRootWidget::HandleRetryCheckpoint);
    NoticeContent->AddChildToVerticalBox(RetryCheckpointButton);
    CheckpointNotice->SetVisibility(ESlateVisibility::Collapsed);

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UGameplayRootWidget::RefreshFlow(const URunStateSubsystem* RunState, const FText& FlowMessage)
{
    if (!RunState)
    {
        return;
    }

    RefreshFlowView(FGameplayViewState::FromRun(RunState, FlowMessage), true);
}

void UGameplayRootWidget::HandleRetryCheckpoint()
{
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>())
    {
        Controller->RequestRetryCombatCheckpoint();
    }
}

void UGameplayRootWidget::RefreshFlowView(const FGameplayViewState& View, bool bAllowRunCommands, bool bCanRetryCheckpoint)
{
    const ERunPhase Phase = View.Phase;
    CheckpointNotice->SetVisibility(Phase == ERunPhase::Combat && !View.FlowMessage.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CheckpointMessage->SetText(View.FlowMessage);
    RetryCheckpointButton->SetVisibility(bCanRetryCheckpoint ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (!bHasDisplayedPhase || DisplayedPhase != Phase)
    {
        bHasDisplayedPhase = true;
        DisplayedPhase = Phase;
        RunLayer->ClearWidgets();
        CombatLayer->ClearWidgets();
        ModalLayer->ClearWidgets();
        RunMapWidget = nullptr;
        ResultWidget = nullptr;
        RunLayer->SetVisibility(ESlateVisibility::Collapsed);
        CombatLayer->SetVisibility(ESlateVisibility::Collapsed);
        ModalLayer->SetVisibility(ESlateVisibility::Collapsed);

        if (Phase == ERunPhase::Combat)
        {
            CombatLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            CombatLayer->AddWidget(CombatHUDWidgetClass);
        }
        else if (Phase == ERunPhase::Result || Phase == ERunPhase::Defeat)
        {
            ModalLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            ResultWidget = Cast<UEncounterResultWidget>(ModalLayer->AddWidget(ResultWidgetClass));
        }
        else
        {
            RunLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            RunMapWidget = Cast<URunMapWidget>(RunLayer->AddWidget(RunMapWidgetClass));
        }
    }

    if (RunMapWidget)
    {
        RunMapWidget->RefreshRunMapView(View, bAllowRunCommands);
        RunMapWidget->SetIsEnabled(Phase != ERunPhase::Preparing);
    }

    if (ResultWidget)
    {
        ResultWidget->ShowResult(View.LastResult, View.FlowMessage);
        ResultWidget->SetContinueEnabled(bAllowRunCommands);
    }
}
