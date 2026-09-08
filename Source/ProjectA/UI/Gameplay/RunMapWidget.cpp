#include "UI/Gameplay/RunMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/GameplayActionButton.h"

TOptional<FUIInputConfig> URunMapWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void URunMapWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Text_Progress && Text_Party && Text_FlowMessage && NodeList)
    {
        return;
    }

    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
    Background->SetBrushColor(FLinearColor(0.025f, 0.04f, 0.06f, 0.98f));
    Root->AddChildToOverlay(Background);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Content);
    ContentSlot->SetHorizontalAlignment(HAlign_Center);
    ContentSlot->SetVerticalAlignment(VAlign_Center);
    Text_Progress = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Progress"));
    Text_Party = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Party"));
    NodeList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NodeList"));
    Text_FlowMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_FlowMessage"));
    Content->AddChildToVerticalBox(Text_Progress);
    Content->AddChildToVerticalBox(Text_Party);
    Content->AddChildToVerticalBox(NodeList);
    Content->AddChildToVerticalBox(Text_FlowMessage);
}

void URunMapWidget::RefreshRunMap(const URunStateSubsystem* RunState, const FText& FlowMessage)
{
    if (!RunState || !NodeList)
    {
        return;
    }

    NodeList->ClearChildren();
    Text_FlowMessage->SetText(FlowMessage);
    Text_Progress->SetText(FText::FromString(FString::Printf(TEXT("RUN MAP / 진행 지도  %d / %d"), RunState->GetCompletedNodes().Num(), RunState->GetNodes().Num())));
    FString PartyText;

    for (const FRunPartyMember& Member : RunState->GetPartyMembers())
    {
        if (Member.bCreated)
        {
            PartyText += FString::Printf(TEXT("[%d] %s (%s)\n"), Member.SlotIndex + 1, *Member.CharacterName.ToString(), *Member.ClassId.ToString());
        }
    }

    Text_Party->SetText(FText::FromString(PartyText));

    for (const FRunNodeDefinition& Node : RunState->GetNodes())
    {
        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
        FString Label = Node.DisplayName.ToString();

        if (RunState->GetCompletedNodes().Contains(Node.NodeId))
        {
            Label += TEXT(" - Complete / 완료");
        }

        Button->Configure(Node.NodeId, FText::FromString(Label));
        Button->SetIsEnabled(RunState->CanStartNode(Node.NodeId));
        Button->OnActionRequested.AddUObject(this, &URunMapWidget::HandleNodeSelected);
        NodeList->AddChildToVerticalBox(Button);
    }

    if (RunState->GetPhase() == ERunPhase::Complete)
    {
        Text_FlowMessage->SetText(FText::FromString(TEXT("Run complete. / 모든 전투를 완료했습니다.")));
    }
    else if (RunState->GetPhase() == ERunPhase::None)
    {
        Text_FlowMessage->SetText(FText::FromString(TEXT("Start a new game from MainMenu to create a party. / MainMenu에서 파티를 생성하고 시작해 주세요.")));
    }
}

void URunMapWidget::HandleNodeSelected(FName NodeId)
{
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestStartNode(NodeId);
    }
}
