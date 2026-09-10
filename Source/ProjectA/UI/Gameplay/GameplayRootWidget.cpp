#include "UI/Gameplay/GameplayRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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

void UGameplayRootWidget::RefreshFlowView(const FGameplayViewState& View, bool bAllowRunCommands)
{
    const ERunPhase Phase = View.Phase;

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
