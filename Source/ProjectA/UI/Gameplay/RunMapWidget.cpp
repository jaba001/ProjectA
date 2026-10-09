#include "UI/Gameplay/RunMapWidget.h"
#include "UI/ProjectALocalization.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
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
#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Theme/DemonicUITheme.h"

namespace
{
    FText PveMonsterRole(const FRunMonsterDefinition& Monster)
    {
        // Describe authored classifications without inventing attacks or AI behavior from a monster's name.
        // 몬스터 이름에서 공격이나 AI 행동을 추측하지 않고 작성된 분류만 설명합니다.
        if (Monster.Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Monster.Role.Boss"), false))) return NSLOCTEXT("RunMap", "MonsterBoss", "우두머리");
        if (Monster.Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Monster.Role.Brute"), false))) return NSLOCTEXT("RunMap", "MonsterBrute", "중장형");
        if (Monster.Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Monster.Role.Skirmisher"), false))) return NSLOCTEXT("RunMap", "MonsterSkirmisher", "기동형");
        if (Monster.Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Monster.Role.Common"), false))) return NSLOCTEXT("RunMap", "MonsterCommon", "일반형");
        return NSLOCTEXT("RunMap", "MonsterUnclassified", "분류 정보 없음");
    }

    FText PveMonsterRoster(const TArray<FRunMonsterDefinition>& Monsters)
    {
        if (Monsters.IsEmpty()) return NSLOCTEXT("RunMap", "MonsterDetailsUnavailable", "등장 몬스터 상세 정보가 없습니다.");
        TArray<FText> Lines;
        FNumberFormattingOptions SpeedFormat;
        SpeedFormat.SetMaximumFractionalDigits(2);
        for (const FRunMonsterDefinition& Monster : Monsters)
        {
            const FText Name = ProjectALocalization::AssetName(Monster.UnitClass, Monster.DisplayName);
            Lines.Add(FText::Format(NSLOCTEXT("RunMap", "MonsterPreview", "{0} · {1}\nHP {2} · 속도 {3} · 이동 {4}칸"), Name, PveMonsterRole(Monster), FText::AsNumber(FMath::RoundToInt(Monster.MaxHP)), FText::AsNumber(Monster.Speed, &SpeedFormat), Monster.MoveRange));
        }
        return FText::Join(FText::FromString(TEXT("\n\n")), Lines);
    }
}

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
        PveDifficultyPanel->SetMaxDesiredHeight(520.f);
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
            FRunPveDifficultyCardWidgets& Card = PveDifficultyCards.AddDefaulted_GetRef();
            Card.Content = WidgetTree->ConstructWidget<UVerticalBox>();
            Card.Content->SetVisibility(ESlateVisibility::HitTestInvisible);
            const auto AddCardText = [this, &Theme, &Card](int32 FontSize, float BottomPadding)
            {
                UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
                Text->SetAutoWrapText(true);
                Text->SetWrapTextAt(276.f);
                Text->SetJustification(ETextJustify::Left);
                Theme.StyleText(Text, false, FontSize);
                Card.Content->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
                return Text;
            };
            Card.Heading = AddCardText(22, 8.f);
            Card.Arena = AddCardText(20, 4.f);
            Card.ArenaDescription = AddCardText(15, 12.f);
            Card.Monsters = AddCardText(16, 12.f);
            Card.Stats = AddCardText(16, 0.f);
        }
        UTextBlock* RewardHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PveRewardHint"));
        RewardHint->SetText(NSLOCTEXT("RunMap", "DifficultyLegend", "100% = 해당 구간 기본값 · 세 선택의 몬스터 종류·공격 피해·AP는 같습니다.\n승리 보상: 아이템 3개 중 1개 + 랜덤 골드 · 아이템 등급·스킬 확률은 같습니다."));
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
        const FText Difficulty = ProjectALocalization::Content(TEXT("Difficulty.") + Offer.DifficultyTag.ToString() + TEXT(".Name"), Offer.DisplayName);
        const FText Heading = bWorldPresentation ? FText::Format(NSLOCTEXT("RunMap", "DirectionDifficulty", "{0} · {1}"), EncounterDungeonLayout::GetDirectionLabel(Index), Difficulty) : Difficulty;
        const FCombatArenaEnvironmentProfile* Arena = CombatArenaEnvironment::Find(Offer.ArenaId);
        const FText ArenaName = Arena ? Arena->DisplayName : Offer.ArenaId.IsNone() ? NSLOCTEXT("RunMap", "LegacyArenaName", "기존 전투장") : NSLOCTEXT("RunMap", "UnknownArenaName", "전투장 정보 없음");
        const FText ArenaDescription = Arena ? Arena->Description : Offer.ArenaId.IsNone() ? NSLOCTEXT("RunMap", "LegacyArenaDescription", "이 Run의 기존 전투 환경을 유지합니다.") : NSLOCTEXT("RunMap", "UnknownArenaDescription", "저장된 전투 환경의 설명을 찾을 수 없습니다.");
        const FText Roster = PveMonsterRoster(Offer.EnemyRoster);
        const FText Stats = FText::Format(NSLOCTEXT("RunMap", "DifficultyStats", "적 {0}명 · 총 HP {1}\n적 HP {2}% · 적 속도 {3}%\n승리 골드 {4}~{5}G ({6}%)"), Offer.EnemyCount, FMath::RoundToInt(Offer.TotalEnemyHP), FMath::RoundToInt(Offer.HPScale * 100.f), FMath::RoundToInt(Offer.SpeedScale * 100.f), Offer.GoldMin, Offer.GoldMax, FMath::RoundToInt(Offer.GoldScale * 100.f));
        FRunPveDifficultyCardWidgets& Card = PveDifficultyCards[Index];
        if (Card.ConfiguredTag != Offer.DifficultyTag)
        {
            // Preserve the button's authoritative tag request while keeping its structured content across refreshes.
            // 새로 고침 사이에 구조화된 내용을 유지하면서 버튼의 권위 요청용 태그를 보존합니다.
            Button->Configure(Offer.DifficultyTag.GetTagName(), Heading);
            Button->SetContent(Card.Content);
            UButtonSlot* CardSlot = CastChecked<UButtonSlot>(Card.Content->Slot);
            CardSlot->SetPadding(FMargin(20.f, 14.f));
            CardSlot->SetHorizontalAlignment(HAlign_Fill);
            CardSlot->SetVerticalAlignment(VAlign_Top);
            Card.ConfiguredTag = Offer.DifficultyTag;
        }
        const FLinearColor Colors[] = {FLinearColor(0.6f, 0.95f, 0.65f), FLinearColor(1.f, 0.9f, 0.65f), FLinearColor(1.f, 0.6f, 0.55f)};
        Card.Heading->SetText(Heading);
        Card.Heading->SetColorAndOpacity(Colors[Index]);
        Card.Arena->SetText(ArenaName);
        Card.ArenaDescription->SetText(ArenaDescription);
        Card.Monsters->SetText(Roster);
        Card.Stats->SetText(Stats);
        Button->SetToolTipText(FText::Format(NSLOCTEXT("RunMap", "DifficultyTooltip", "{0} · {1}\n{2}\n\n{3}\n\n{4}\n배율은 해당 구간 기본값을 기준으로 합니다. 골드는 표시된 범위에서 정해집니다.\n이 카드를 선택하면 전투를 시작합니다."), Heading, ArenaName, ArenaDescription, Roster, Stats));
    }
    if (bPveChoice)
    {
        const FRunNodeDefinition& Node = View.Nodes[NextNodeIndex];
        PveDifficultyTitle->SetText(FText::Format(NSLOCTEXT("RunMap", "DifficultyProgress", "{0} · PvE 난이도 선택 · 진행 {1} / 80 완료"), ProjectALocalization::Content(TEXT("Node.") + Node.NodeId.ToString() + TEXT(".Name"), Node.DisplayName), View.TargetCompletedSteps));
        const FText HostMessage = bRunCommandsAllowed ? NSLOCTEXT("RunMap", "SelectDifficultyToStart", "하·중·상 중 하나를 선택하면 전투를 시작합니다.") : NSLOCTEXT("RunMap", "WaitingHostDifficulty", "선택 대기 · Host가 전투 난이도를 선택합니다.");
        PveDifficultyMessage->SetText(HostMessage.IsEmpty() ? View.FlowMessage : View.FlowMessage.IsEmpty() ? HostMessage : FText::Format(FText::FromString(TEXT("{0}\n{1}")), HostMessage, View.FlowMessage));
    }
    if (!NodeList)
    {
        return;
    }
    NodeList->ClearChildren();
    Text_FlowMessage->SetText(View.FlowMessage);
    Text_Progress->SetText(FText::Format(NSLOCTEXT("RunMap", "Progress", "진행 지도  {0} / {1}"), View.CompletedNodes.Num(), View.Nodes.Num()));
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
        FText Label = ProjectALocalization::Content(TEXT("Node.") + Node.NodeId.ToString() + TEXT(".Name"), Node.DisplayName);

        if (View.CompletedNodes.Contains(Node.NodeId))
        {
            Label = FText::Format(NSLOCTEXT("RunMap", "CompletedNode", "{0} - 완료"), Label);
        }

        Button->Configure(Node.NodeId, Label);
        Button->SetIsEnabled(!bPveChoice && bRunCommandsAllowed && View.AvailableNodes.Contains(Node.NodeId));
        Button->OnActionRequested.AddUObject(this, &URunMapWidget::HandleNodeSelected);
        NodeList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
    }

    if (View.Phase == ERunPhase::Complete)
    {
        Text_FlowMessage->SetText(NSLOCTEXT("RunMap", "TextC2BDB6BA", "Run complete. / 모든 전투를 완료했습니다."));
    }
    else if (View.Phase == ERunPhase::None)
    {
        Text_FlowMessage->SetText(NSLOCTEXT("RunMap", "TextE331F0FD", "Start a new game from MainMenu to create a party. / MainMenu에서 파티를 생성하고 시작해 주세요."));
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
