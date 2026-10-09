#include "UI/Gameplay/RunMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Theme/DemonicUITheme.h"

TOptional<FUIInputConfig> URunMapWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void URunMapWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("RootOverlay")));
    UBorder* Background = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Background")));
    UVerticalBox* Content = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ContentBox")));

    if (!Text_Progress || !Text_Party || !Text_FlowMessage || !NodeList)
    {
        Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = Root;
        Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
        Root->AddChildToOverlay(Background);
        Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
        Root->AddChildToOverlay(Content);
        Text_Progress = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Progress"));
        Text_Party = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Party"));
        NodeList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NodeList"));
        Text_FlowMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_FlowMessage"));
        Content->AddChildToVerticalBox(Text_Progress);
        Theme.AddDivider(WidgetTree, Content);
        Content->AddChildToVerticalBox(Text_Party)->SetPadding(FMargin(0.0f, 8.0f));
        Content->AddChildToVerticalBox(NodeList);
        Content->AddChildToVerticalBox(Text_FlowMessage)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
    }

    // Preserve custom map roots while always providing an overlay for required difficulty choices.
    // 필수 난이도 선택을 위한 오버레이를 항상 제공하면서 사용자 지도 루트를 보존합니다.
    if (!Root)
    {
        UWidget* PreviousRoot = WidgetTree->RootWidget;
        Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RunMapPresentationRoot"));
        WidgetTree->RootWidget = Root;
        if (PreviousRoot)
        {
            Root->AddChildToOverlay(PreviousRoot);
            MapPanel = PreviousRoot;
        }
    }

    // Keep every node reachable while retaining the known scaffold's list binding and slot layout.
    // 알려진 생성 구조의 목록 바인딩과 슬롯 배치를 유지하면서 모든 노드를 스크롤로 확인할 수 있게 합니다.
    if (Root && Content && Content->GetParent() == Root && NodeList->GetParent() == Content)
    {
        const int32 NodeIndex = Content->GetChildIndex(NodeList);
        UVerticalBoxSlot* NodeSlot = CastChecked<UVerticalBoxSlot>(NodeList->Slot);
        USizeBox* NodeListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RunMapNodeListSize"));
        NodeListSize->SetMaxDesiredHeight(300.0f);
        UScrollBox* NodeScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RunMapNodeScroll"));
        NodeScroll->SetAllowOverscroll(false);
        NodeScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
        NodeList->RemoveFromParent();
        Content->InsertChildAt(NodeIndex, NodeListSize, NodeSlot);
        NodeListSize->SetContent(NodeScroll);
        NodeScroll->AddChild(NodeList);
    }

    if (Root && Background && Background->GetParent() == Root)
    {
        WorldBackdrop = Background;
        Theme.StyleBackdrop(Background);
        UOverlaySlot* BackgroundSlot = CastChecked<UOverlaySlot>(Background->Slot);
        BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
        BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // Frame the known scaffold while retaining its bound widgets and any custom layouts.
    // 바인딩된 위젯과 별도 사용자 레이아웃을 유지하며 알려진 생성 구조만 프레임으로 감쌉니다.
    if (Root && Content && Content->GetParent() == Root)
    {
        Content->RemoveFromParent();
        UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunMapPanel"));
        MapPanel = Panel;
        Theme.StylePanel(Panel);
        Panel->SetPadding(FMargin(32.0f));
        UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Panel);
        ContentSlot->SetHorizontalAlignment(HAlign_Center);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
        ContentSlot->SetPadding(FMargin(24.0f));
        USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>();
        ContentSize->SetMinDesiredWidth(580.0f);
        ContentSize->SetMaxDesiredWidth(680.0f);
        Panel->SetContent(ContentSize);
        ContentSize->SetContent(Content);
        Text_FlowMessage->SetAutoWrapText(true);
        Text_FlowMessage->SetWrapTextAt(580.0f);
    }
    if (!MapPanel)
    {
        UWidget* LegacyContent = NodeList;
        while (LegacyContent && LegacyContent->GetParent() && LegacyContent->GetParent() != Root) LegacyContent = LegacyContent->GetParent();
        if (LegacyContent && LegacyContent->GetParent() == Root) MapPanel = LegacyContent;
    }

    if (Root)
    {
        // Keep the three choices in direction order while allowing narrow screens to scroll instead of clipping.
        // 세 선택의 방향 순서를 유지하고 좁은 화면에서는 잘리지 않고 스크롤로 접근할 수 있게 합니다.
        PveDifficultyPanel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PveDifficultyPanel"));
        PveDifficultyPanel->SetMaxDesiredHeight(360.f);
        PveDifficultyPanel->SetVisibility(ESlateVisibility::Collapsed);
        UOverlaySlot* DifficultySlot = Root->AddChildToOverlay(PveDifficultyPanel);
        DifficultySlot->SetHorizontalAlignment(HAlign_Fill);
        DifficultySlot->SetVerticalAlignment(VAlign_Bottom);
        DifficultySlot->SetPadding(FMargin(24.f));
        UBorder* DifficultyBorder = WidgetTree->ConstructWidget<UBorder>();
        Theme.StylePanel(DifficultyBorder);
        DifficultyBorder->SetPadding(FMargin(16.f, 12.f));
        PveDifficultyPanel->SetContent(DifficultyBorder);
        UScrollBox* DifficultyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PveDifficultyScroll"));
        DifficultyScroll->SetAllowOverscroll(false);
        DifficultyScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
        DifficultyBorder->SetContent(DifficultyScroll);
        UVerticalBox* DifficultyContent = WidgetTree->ConstructWidget<UVerticalBox>();
        DifficultyScroll->AddChild(DifficultyContent);
        PveDifficultyTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PveDifficultyTitle"));
        PveDifficultyTitle->SetJustification(ETextJustify::Center);
        PveDifficultyTitle->SetAutoWrapText(true);
        DifficultyContent->AddChildToVerticalBox(PveDifficultyTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        UScrollBox* ChoiceScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PveDifficultyChoiceScroll"));
        ChoiceScroll->SetOrientation(Orient_Horizontal);
        ChoiceScroll->SetAllowOverscroll(false);
        ChoiceScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
        DifficultyContent->AddChildToVerticalBox(ChoiceScroll);
        UHorizontalBox* Choices = WidgetTree->ConstructWidget<UHorizontalBox>();
        CastChecked<UScrollBoxSlot>(ChoiceScroll->AddChild(Choices))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        for (int32 Index = 0; Index < 3; ++Index)
        {
            USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
            CardSize->SetMinDesiredWidth(320.f);
            CardSize->SetMinDesiredHeight(196.f);
            UHorizontalBoxSlot* CardSlot = Choices->AddChildToHorizontalBox(CardSize);
            CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            CardSlot->SetPadding(FMargin(6.f, 0.f));
            UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), FName(*FString::Printf(TEXT("Button_PveDifficulty_%d"), Index)));
            Button->OnActionRequested.AddUObject(this, &URunMapWidget::HandlePveDifficultySelected);
            CardSize->SetContent(Button);
            PveDifficultyButtons.Add(Button);
        }
        UTextBlock* RewardHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PveRewardHint"));
        RewardHint->SetText(NSLOCTEXT("RunMap", "DifficultyLegend", "100% = 해당 구간 기본값 · 적 구성·공격 피해·AP는 동일\n승리 보상: 아이템 3개 중 1개 + 랜덤 골드 · 아이템 등급·스킬 확률은 동일합니다."));
        RewardHint->SetJustification(ETextJustify::Center);
        RewardHint->SetAutoWrapText(true);
        DifficultyContent->AddChildToVerticalBox(RewardHint)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
        PveDifficultyMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PveDifficultyMessage"));
        PveDifficultyMessage->SetJustification(ETextJustify::Center);
        PveDifficultyMessage->SetAutoWrapText(true);
        DifficultyContent->AddChildToVerticalBox(PveDifficultyMessage)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
        Theme.StyleText(RewardHint, false, 16);
    }
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Text_Progress, true, 28);
    Theme.StyleText(PveDifficultyTitle, true, 22);
    Theme.StyleText(PveDifficultyMessage, false, 16);
}

void URunMapWidget::RefreshRunMap(const URunStateSubsystem* RunState, const FText& FlowMessage)
{
    if (!RunState)
    {
        return;
    }
    RefreshRunMapView(FGameplayViewState::FromRun(RunState, FlowMessage), true);
}

void URunMapWidget::RefreshRunMapView(const FGameplayViewState& View, bool bAllowRunCommands, bool bWorldPresentation)
{
    bRunCommandsAllowed = bAllowRunCommands;
    const int32 NextNodeIndex = View.CompletedNodes.Num();
    const bool bPveChoice = View.Phase == ERunPhase::Map && View.PveDifficultyOffers.Num() == 3 && View.Nodes.IsValidIndex(NextNodeIndex) && View.AvailableNodes.Contains(View.Nodes[NextNodeIndex].NodeId);
    PveNodeId = bPveChoice ? View.Nodes[NextNodeIndex].NodeId : NAME_None;
    PveDifficultyTags.Reset();
    if (WorldBackdrop)
    {
        WorldBackdrop->SetVisibility(bPveChoice && bWorldPresentation ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
        WorldBackdrop->SetRenderOpacity(bWorldPresentation ? 0.65f : 1.f);
    }
    if (MapPanel) MapPanel->SetVisibility(bPveChoice ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (PveDifficultyPanel)
    {
        PveDifficultyPanel->SetVisibility(bPveChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        CastChecked<UOverlaySlot>(PveDifficultyPanel->Slot)->SetVerticalAlignment(bWorldPresentation ? VAlign_Bottom : VAlign_Center);
    }
    for (int32 Index = 0; Index < PveDifficultyButtons.Num(); ++Index)
    {
        UGameplayActionButton* Button = PveDifficultyButtons[Index];
        Button->SetIsEnabled(bPveChoice && bRunCommandsAllowed);
        if (!bPveChoice) continue;
        const FRunPveDifficultyOffer& Offer = View.PveDifficultyOffers[Index];
        PveDifficultyTags.Add(Offer.DifficultyTag);
        const FString Heading = bWorldPresentation ? FString::Printf(TEXT("%s · %s"), *EncounterDungeonLayout::GetDirectionLabel(Index).ToString(), *Offer.DisplayName.ToString()) : Offer.DisplayName.ToString();
        const FString Label = FString::Printf(TEXT("%s\n적 %d명 · 총 HP %.0f\n적 HP %d%% · 적 속도 %d%%\n승리 골드 %d~%dG (%d%%)"), *Heading, Offer.EnemyCount, Offer.TotalEnemyHP, FMath::RoundToInt(Offer.HPScale * 100.f), FMath::RoundToInt(Offer.SpeedScale * 100.f), Offer.GoldMin, Offer.GoldMax, FMath::RoundToInt(Offer.GoldScale * 100.f));
        Button->Configure(Offer.DifficultyTag.GetTagName(), FText::FromString(Label));
        Button->SetToolTipText(FText::FromString(Label + TEXT("\n배율은 해당 구간 기본값을 기준으로 합니다. 골드는 표시된 범위에서 정해집니다.\n이 난이도를 선택하면 전투를 시작합니다.")));
        if (UTextBlock* LabelText = Cast<UTextBlock>(Button->GetContent()))
        {
            const FLinearColor Colors[] = {FLinearColor(0.6f, 0.95f, 0.65f), FLinearColor(1.f, 0.9f, 0.65f), FLinearColor(1.f, 0.6f, 0.55f)};
            LabelText->SetColorAndOpacity(Colors[Index]);
            LabelText->SetWrapTextAt(276.f);
        }
    }
    if (bPveChoice)
    {
        PveDifficultyTitle->SetText(FText::FromString(FString::Printf(TEXT("%s · PvE 난이도 선택 · 진행 %d / 80 완료"), *View.Nodes[NextNodeIndex].DisplayName.ToString(), View.TargetCompletedSteps)));
        const FText HostMessage = bRunCommandsAllowed ? NSLOCTEXT("RunMap", "SelectDifficultyToStart", "하·중·상 중 하나를 선택하면 전투를 시작합니다.") : NSLOCTEXT("RunMap", "WaitingHostDifficulty", "선택 대기 · Host가 전투 난이도를 선택합니다.");
        PveDifficultyMessage->SetText(HostMessage.IsEmpty() ? View.FlowMessage : View.FlowMessage.IsEmpty() ? HostMessage : FText::Format(FText::FromString(TEXT("{0}\n{1}")), HostMessage, View.FlowMessage));
    }
    if (!NodeList)
    {
        return;
    }
    NodeList->ClearChildren();
    Text_FlowMessage->SetText(View.FlowMessage);
    Text_Progress->SetText(FText::FromString(FString::Printf(TEXT("RUN MAP / 진행 지도  %d / %d"), View.CompletedNodes.Num(), View.Nodes.Num())));
    FString PartyText;

    for (const FRunPartyMember& Member : View.PartyMembers)
    {
        if (Member.bCreated)
        {
            PartyText += FString::Printf(TEXT("[%d] %s (%s)\n"), Member.SlotIndex + 1, *Member.CharacterName.ToString(), *Member.ClassId.ToString());
        }
    }

    Text_Party->SetText(FText::FromString(PartyText));

    for (const FRunNodeDefinition& Node : View.Nodes)
    {
        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
        FString Label = Node.DisplayName.ToString();

        if (View.CompletedNodes.Contains(Node.NodeId))
        {
            Label += TEXT(" - Complete / 완료");
        }

        Button->Configure(Node.NodeId, FText::FromString(Label));
        Button->SetIsEnabled(!bPveChoice && bRunCommandsAllowed && View.AvailableNodes.Contains(Node.NodeId));
        Button->OnActionRequested.AddUObject(this, &URunMapWidget::HandleNodeSelected);
        NodeList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
    }

    if (View.Phase == ERunPhase::Complete)
    {
        Text_FlowMessage->SetText(FText::FromString(TEXT("Run complete. / 모든 전투를 완료했습니다.")));
    }
    else if (View.Phase == ERunPhase::None)
    {
        Text_FlowMessage->SetText(FText::FromString(TEXT("Start a new game from MainMenu to create a party. / MainMenu에서 파티를 생성하고 시작해 주세요.")));
    }
}

void URunMapWidget::HandleNodeSelected(FName NodeId)
{
    if (!bRunCommandsAllowed || !PveNodeId.IsNone())
    {
        return;
    }
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestStartNode(NodeId);
    }
}

void URunMapWidget::HandlePveDifficultySelected(FName DifficultyName)
{
    if (!bRunCommandsAllowed || PveNodeId.IsNone()) return;
    const FGameplayTag* Difficulty = PveDifficultyTags.FindByPredicate([DifficultyName](FGameplayTag Tag) { return Tag.GetTagName() == DifficultyName; });
    if (!Difficulty) return;
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestStartNode(PveNodeId, *Difficulty);
    }
}
