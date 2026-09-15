#include "UI/Gameplay/RunMapWidget.h"

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

    if (Root && Background && Background->GetParent() == Root)
    {
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
        ContentSize->SetMinDesiredWidth(580.0f);
        ContentSize->SetMaxDesiredWidth(680.0f);
        Panel->SetContent(ContentSize);
        ContentSize->SetContent(Content);
        Text_FlowMessage->SetAutoWrapText(true);
        Text_FlowMessage->SetWrapTextAt(580.0f);
    }
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Text_Progress, true, 28);
}

void URunMapWidget::RefreshRunMap(const URunStateSubsystem* RunState, const FText& FlowMessage)
{
    if (!RunState)
    {
        return;
    }
    RefreshRunMapView(FGameplayViewState::FromRun(RunState, FlowMessage), true);
}

void URunMapWidget::RefreshRunMapView(const FGameplayViewState& View, bool bAllowRunCommands)
{
    bRunCommandsAllowed = bAllowRunCommands;
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
        Button->SetIsEnabled(bRunCommandsAllowed && View.AvailableNodes.Contains(Node.NodeId));
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
    if (!bRunCommandsAllowed)
    {
        return;
    }
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestStartNode(NodeId);
    }
}
