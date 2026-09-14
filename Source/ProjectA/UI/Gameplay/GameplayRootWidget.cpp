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
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "UI/Gameplay/RunEncounterWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Game/Development/DevelopmentCoopLobby.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "UI/MainMenu/DevelopmentCoopWidget.h"
#include "Engine/GameInstance.h"

void UGameplayRootWidget::HandleLeaveDevelopmentCoop()
{
    GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>()->Leave(GetOwningPlayer());
}

void UGameplayRootWidget::RefreshDevelopmentLobby(ADevelopmentCoopLobby* Lobby)
{
    const bool bEnabled = Lobby && UDevelopmentCoopSubsystem::IsAvailable();
    DevelopmentBar->SetVisibility(bEnabled && Lobby->HasStarted() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bEnabled) DevelopmentMessage->SetText(Lobby->GetMessage());
    if (bEnabled && !Lobby->HasStarted())
    {
        DevelopmentLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (!DevelopmentWidget) DevelopmentWidget = DevelopmentLayer->AddWidget<UDevelopmentCoopWidget>(UDevelopmentCoopWidget::StaticClass());
        DevelopmentWidget->RefreshLobby(Lobby);
    }
    else
    {
        if (DevelopmentWidget) DevelopmentWidget->DeactivateWidget();
        DevelopmentWidget = nullptr;
        DevelopmentLayer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UGameplayRootWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!RunMapWidgetClass)
    {
        RunMapWidgetClass = URunMapWidget::StaticClass();
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
    DevelopmentLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
    UOverlaySlot* DevelopmentLayerSlot = NoticeRoot->AddChildToOverlay(DevelopmentLayer);
    DevelopmentLayerSlot->SetHorizontalAlignment(HAlign_Fill);
    DevelopmentLayerSlot->SetVerticalAlignment(VAlign_Fill);
    DevelopmentLayer->SetVisibility(ESlateVisibility::Collapsed);
    DevelopmentBar = WidgetTree->ConstructWidget<UBorder>();
    DevelopmentBar->SetPadding(FMargin(8.f));
    UOverlaySlot* DevelopmentSlot = NoticeRoot->AddChildToOverlay(DevelopmentBar);
    DevelopmentSlot->SetHorizontalAlignment(HAlign_Right);
    DevelopmentSlot->SetVerticalAlignment(VAlign_Top);
    UVerticalBox* DevelopmentContent = WidgetTree->ConstructWidget<UVerticalBox>();
    DevelopmentBar->SetContent(DevelopmentContent);
    DevelopmentMessage = WidgetTree->ConstructWidget<UTextBlock>();
    DevelopmentMessage->SetAutoWrapText(true);
    DevelopmentMessage->SetWrapTextAt(280.f);
    DevelopmentContent->AddChildToVerticalBox(DevelopmentMessage);
    UButton* Leave = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_LeaveDevelopmentCoop"));
    UTextBlock* LeaveLabel = WidgetTree->ConstructWidget<UTextBlock>();
    LeaveLabel->SetText(FText::FromString(TEXT("개발 협동 나가기 · Host는 방 종료")));
    Leave->SetContent(LeaveLabel);
    Leave->OnClicked.AddDynamic(this, &UGameplayRootWidget::HandleLeaveDevelopmentCoop);
    DevelopmentContent->AddChildToVerticalBox(Leave);
    DevelopmentBar->SetVisibility(ESlateVisibility::Collapsed);

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
    CheckpointNotice->SetVisibility((Phase == ERunPhase::Combat && !View.FlowMessage.IsEmpty()) || bCanRetryCheckpoint ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
        RunEncounterWidget = nullptr;
        RunLayer->SetVisibility(ESlateVisibility::Collapsed);
        CombatLayer->SetVisibility(ESlateVisibility::Collapsed);
        ModalLayer->SetVisibility(ESlateVisibility::Collapsed);

        if (Phase == ERunPhase::Combat)
        {
            CombatLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            CombatLayer->AddWidget<UCombatRoundPlanningWidget>(UCombatRoundPlanningWidget::StaticClass());
        }
        else if (Phase == ERunPhase::Result || Phase == ERunPhase::Defeat)
        {
            ModalLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            ResultWidget = Cast<UEncounterResultWidget>(ModalLayer->AddWidget(ResultWidgetClass));
        }
        else if (Phase == ERunPhase::EncounterChoice || Phase == ERunPhase::Shop)
        {
            RunLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            RunEncounterWidget = RunLayer->AddWidget<URunEncounterWidget>(URunEncounterWidget::StaticClass());
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

    if (RunEncounterWidget) RunEncounterWidget->RefreshEncounter(View, bAllowRunCommands);

    if (ResultWidget)
    {
        ResultWidget->ShowResult(View.LastResult, View.FlowMessage);
        ResultWidget->SetContinueEnabled(bAllowRunCommands);
    }
}
