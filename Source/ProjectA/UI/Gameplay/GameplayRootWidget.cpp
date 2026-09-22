#include "UI/Gameplay/GameplayRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CommonUITypes.h"
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
#include "UI/Gameplay/InventoryWidget.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Game/Development/DevelopmentCoopLobby.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "UI/MainMenu/DevelopmentCoopWidget.h"
#include "UI/Theme/DemonicUITheme.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Input/CommonUIInputTypes.h"

namespace
{
    struct FGameplayShortcutAction : public FCommonInputActionDataBase
    {
        FGameplayShortcutAction(const FText& Label, FKey Key)
        {
            DisplayName = Label;
            KeyboardInputTypeInfo.SetKey(Key);
        }
    };
}

void UGameplayRootWidget::RegisterGameplayShortcuts()
{
    // Persistent CommonUI actions also work on menu-only screens where controller keys are blocked.
    // 컨트롤러 키가 차단되는 메뉴 전용 화면에서도 동작하도록 CommonUI 지속 액션을 사용합니다.
    ShortcutActions = NewObject<UDataTable>(this);
    ShortcutActions->RowStruct = FCommonInputActionDataBase::StaticStruct();
    ShortcutActions->AddRow(TEXT("Inventory"), FGameplayShortcutAction(NSLOCTEXT("GameplayShortcuts", "Inventory", "인벤토리"), EKeys::I));
    ShortcutActions->AddRow(TEXT("Settings"), FGameplayShortcutAction(NSLOCTEXT("GameplayShortcuts", "Settings", "설정"), EKeys::Escape));
    const auto BindShortcut = [this](FName RowName, const FSimpleDelegate& Callback)
    {
        FDataTableRowHandle Action;
        Action.DataTable = ShortcutActions;
        Action.RowName = RowName;
        FBindUIActionArgs Args(Action, false, Callback);
        Args.bIsPersistent = true;
        Args.bConsumeInput = true;
        RegisterUIActionBinding(Args);
    };
    BindShortcut(TEXT("Inventory"), FSimpleDelegate::CreateUObject(this, &UGameplayRootWidget::ToggleInventory));
    BindShortcut(TEXT("Settings"), FSimpleDelegate::CreateUObject(this, &UGameplayRootWidget::ToggleSettings));
}

bool UGameplayRootWidget::IsUtilityMenuOpen() const
{
    return UtilityLayer && UtilityLayer->GetActiveWidget() && UtilityLayer->GetActiveWidget()->IsActivated();
}

void UGameplayRootWidget::HandleUtilityWidgetChanged(UCommonActivatableWidget* ActiveWidget)
{
    const bool bOpen = ActiveWidget != nullptr;
    UtilityLayer->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    for (UWidget* Layer : { static_cast<UWidget*>(RunLayer.Get()), static_cast<UWidget*>(CombatLayer.Get()), static_cast<UWidget*>(ModalLayer.Get()), static_cast<UWidget*>(DevelopmentLayer.Get()), static_cast<UWidget*>(DevelopmentBar.Get()), static_cast<UWidget*>(CheckpointNotice.Get()) })
    {
        if (Layer) Layer->SetIsEnabled(!bOpen);
    }
    RefreshInventory();
}

void UGameplayRootWidget::RefreshInventory()
{
    UInventoryWidget* Inventory = UtilityLayer ? Cast<UInventoryWidget>(UtilityLayer->GetActiveWidget()) : nullptr;
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (Inventory) Inventory->RefreshInventory(CurrentView, Controller ? Controller->GetInventoryCharacterId(CurrentView) : FGuid());
}

void UGameplayRootWidget::RefreshGold()
{
    if (!GoldPanel || !GoldText) return;
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    const FGuid CharacterId = Controller ? Controller->GetInventoryCharacterId(CurrentView) : FGuid();
    const FRunPartyMember* Member = CharacterId.IsValid() ? CurrentView.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId && Candidate.bCreated; }) : nullptr;
    const bool bVisible = Member && CurrentView.Phase != ERunPhase::None && !(DevelopmentWidget && DevelopmentWidget->IsActivated());
    GoldPanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    // Display only the confirmed personal balance shared with inventory and replicated Run views.
    // 인벤토리 및 복제 Run 뷰와 같은 확정 개인 잔액만 표시합니다.
    GoldText->SetText(bVisible ? FText::Format(NSLOCTEXT("GameplayHUD", "PersonalGold", "보유 골드  {0}G"), FText::AsNumber(Member->Gold)) : FText::GetEmpty());
}

void UGameplayRootWidget::ToggleInventory()
{
    if (!UtilityLayer || !bHasDisplayedPhase || DisplayedPhase == ERunPhase::None || (DevelopmentWidget && DevelopmentWidget->IsActivated())) return;
    UCommonActivatableWidget* Active = UtilityLayer->GetActiveWidget();
    if (Cast<UOptionsWidget>(Active)) return;
    if (UInventoryWidget* Inventory = Cast<UInventoryWidget>(Active))
    {
        Inventory->DeactivateWidget();
        return;
    }
    UtilityLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UtilityLayer->AddWidget<UInventoryWidget>(UInventoryWidget::StaticClass());
    RefreshInventory();
}

void UGameplayRootWidget::ToggleSettings()
{
    if (!UtilityLayer) return;
    if (UOptionsWidget* Options = Cast<UOptionsWidget>(UtilityLayer->GetActiveWidget()))
    {
        Options->RequestBack();
        return;
    }
    UtilityLayer->ClearWidgets();
    UtilityLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UtilityLayer->AddWidget<UOptionsWidget>(UOptionsWidget::StaticClass());
}

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
    RefreshGold();
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
    GoldPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PersonalGoldPanel"));
    GoldPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.016f, 0.022f, 0.92f), 8.f, FLinearColor(0.3f, 0.25f, 0.18f, 0.85f), 1.f));
    GoldPanel->SetPadding(FMargin(16.f, 10.f));
    GoldPanel->SetVisibility(ESlateVisibility::Collapsed);
    UOverlaySlot* GoldSlot = NoticeRoot->AddChildToOverlay(GoldPanel);
    GoldSlot->SetHorizontalAlignment(HAlign_Left);
    GoldSlot->SetVerticalAlignment(VAlign_Top);
    GoldSlot->SetPadding(FMargin(24.f, 16.f));
    GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PersonalGold"));
    UDemonicUITheme::Get().StyleText(GoldText, true, 20);
    GoldText->SetColorAndOpacity(FLinearColor(0.95f, 0.76f, 0.34f));
    GoldPanel->SetContent(GoldText);
    CheckpointNotice = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CheckpointNotice"));
    UOverlaySlot* NoticeSlot = NoticeRoot->AddChildToOverlay(CheckpointNotice);
    NoticeSlot->SetHorizontalAlignment(HAlign_Center);
    NoticeSlot->SetVerticalAlignment(VAlign_Top);
    NoticeSlot->SetPadding(FMargin(24.f, 128.f, 24.f, 0.f));
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
    UtilityLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("GameplayUtilityLayer"));
    UtilityLayer->SetTransitionDuration(0.f);
    UtilityLayer->SetVisibility(ESlateVisibility::Collapsed);
    UtilityLayer->OnDisplayedWidgetChanged().AddUObject(this, &UGameplayRootWidget::HandleUtilityWidgetChanged);
    UOverlaySlot* UtilitySlot = NoticeRoot->AddChildToOverlay(UtilityLayer);
    UtilitySlot->SetHorizontalAlignment(HAlign_Fill);
    UtilitySlot->SetVerticalAlignment(VAlign_Fill);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    Theme.ApplyControls(WidgetTree);
    Theme.StylePanel(CheckpointNotice);
    Theme.StylePanel(DevelopmentBar);

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    RegisterGameplayShortcuts();
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
    CurrentView = View;
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
    RefreshInventory();
    RefreshGold();
}
