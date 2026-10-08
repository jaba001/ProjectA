#include "UI/Gameplay/EncounterResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/RunItemPresentation.h"
#include "UI/Theme/DemonicUITheme.h"

TOptional<FUIInputConfig> UEncounterResultWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UEncounterResultWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("RootOverlay")));
    UBorder* Background = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Background")));
    UVerticalBox* Content = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("ContentBox")));

    if (!Text_Result || !Button_Continue)
    {
        Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = Root;
        Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
        Root->AddChildToOverlay(Background);
        Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
        Root->AddChildToOverlay(Content);
        Text_Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Result"));
        Button_Continue = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Continue"));
        UTextBlock* ContinueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Continue"));
        ContinueText->SetText(FText::FromString(TEXT("Continue / 계속")));
        UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button_Continue->AddChild(ContinueText));
        ButtonSlot->SetPadding(FMargin(24.0f, 12.0f));
        Content->AddChildToVerticalBox(Text_Result);
        Theme.AddDivider(WidgetTree, Content);
        Content->AddChildToVerticalBox(Button_Continue)->SetPadding(FMargin(48.0f, 12.0f, 48.0f, 0.0f));
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
        UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultPanel"));
        Theme.StylePanel(Panel);
        Panel->SetPadding(FMargin(40.0f));
        UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Panel);
        ContentSlot->SetHorizontalAlignment(HAlign_Center);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
        ContentSlot->SetPadding(FMargin(24.0f));
        ResultContentSize = WidgetTree->ConstructWidget<USizeBox>();
        ResultContentSize->SetMinDesiredWidth(540.0f);
        ResultContentSize->SetMaxDesiredWidth(640.0f);
        Panel->SetContent(ResultContentSize);
        ResultContentSize->SetContent(Content);
        Text_Result->SetJustification(ETextJustify::Center);
        Text_Result->SetAutoWrapText(true);
        Text_Result->SetWrapTextAt(540.0f);
    }

    RewardsContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GoldRewardsContent"));
    RewardsContainer = RewardsContent;
    if (Content)
    {
        const int32 ContinueIndex = Content->GetChildIndex(Button_Continue);
        Content->InsertChildAt(ContinueIndex == INDEX_NONE ? Content->GetChildrenCount() : ContinueIndex, RewardsContent);
    }
    else
    {
        // Keep custom Blueprint bindings intact and attach rewards without rebuilding their widget tree.
        // 사용자 Blueprint 바인딩을 유지하며 위젯 트리를 재작성하지 않고 보상 영역을 추가합니다.
        if (!Root)
        {
            UWidget* ExistingRoot = WidgetTree->RootWidget;
            Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RewardRootOverlay"));
            WidgetTree->RootWidget = Root;
            UOverlaySlot* ExistingSlot = Root->AddChildToOverlay(ExistingRoot);
            ExistingSlot->SetHorizontalAlignment(HAlign_Fill);
            ExistingSlot->SetVerticalAlignment(VAlign_Fill);
        }
        UBorder* RewardsPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GoldRewardsPanel"));
        Theme.StylePanel(RewardsPanel);
        RewardsPanel->SetPadding(FMargin(24.f));
        RewardsPanel->SetContent(RewardsContent);
        RewardsContainer = RewardsPanel;
        UOverlaySlot* RewardsSlot = Root->AddChildToOverlay(RewardsPanel);
        RewardsSlot->SetHorizontalAlignment(HAlign_Center);
        RewardsSlot->SetVerticalAlignment(VAlign_Bottom);
        RewardsSlot->SetPadding(FMargin(24.f));
    }
    RewardsContainer->SetVisibility(ESlateVisibility::Collapsed);
    RewardInstruction = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RewardInstruction"));
    RewardInstruction->SetText(NSLOCTEXT("CombatGoldReward", "ChooseOne", "전투 보상 · 3개 중 1개 선택"));
    RewardInstruction->SetJustification(ETextJustify::Center);
    RewardsContent->AddChildToVerticalBox(RewardInstruction)->SetPadding(FMargin(0.f, 16.f, 0.f, 12.f));
    RewardBonusGold = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RewardBonusGold"));
    RewardBonusGold->SetJustification(ETextJustify::Center);
    RewardBonusGold->SetAutoWrapText(true);
    RewardBonusGold->SetWrapTextAt(780.f);
    RewardBonusGold->SetVisibility(ESlateVisibility::Collapsed);
    RewardsContent->AddChildToVerticalBox(RewardBonusGold)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    UHorizontalBox* RewardCards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRewardCards"));
    RewardsContent->AddChildToVerticalBox(RewardCards);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FName ChoiceId(*FString::Printf(TEXT("GoldReward%d"), Index + 1));
        RewardChoiceIds.Add(ChoiceId);
        USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
        CardSize->SetMinDesiredWidth(160.f);
        CardSize->SetMinDesiredHeight(160.f);
        UHorizontalBoxSlot* CardSlot = RewardCards->AddChildToHorizontalBox(CardSize);
        CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        CardSlot->SetPadding(FMargin(Index == 0 ? 0.f : 6.f, 0.f, Index == 2 ? 0.f : 6.f, 0.f));
        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), ChoiceId);
        Button->Configure(ChoiceId, FText::GetEmpty());
        Button->OnActionRequested.AddUObject(this, &UEncounterResultWidget::HandleRewardSelection);
        Button->SetToolTipText(NSLOCTEXT("CombatGoldReward", "ClaimOnSelection", "선택하면 표시된 골드가 본인 캐릭터에게 지급됩니다. 한 번만 선택할 수 있습니다."));
        CardSize->SetContent(Button);
        UVerticalBox* CardContent = WidgetTree->ConstructWidget<UVerticalBox>();
        Button->SetContent(CardContent);
        UButtonSlot* ButtonSlot = CastChecked<UButtonSlot>(CardContent->Slot);
        ButtonSlot->SetPadding(FMargin(16.f));
        ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>();
        Caption->SetText(FText::Format(NSLOCTEXT("CombatGoldReward", "ChoiceNumber", "선택지 {0}"), FText::AsNumber(Index + 1)));
        Caption->SetJustification(ETextJustify::Center);
        CardContent->AddChildToVerticalBox(Caption);
        USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
        IconSize->SetWidthOverride(48.f);
        IconSize->SetHeightOverride(48.f);
        IconSize->SetVisibility(ESlateVisibility::Collapsed);
        UVerticalBoxSlot* IconSlot = CardContent->AddChildToVerticalBox(IconSize);
        IconSlot->SetHorizontalAlignment(HAlign_Center);
        IconSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
        UImage* Icon = WidgetTree->ConstructWidget<UImage>();
        Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
        IconSize->SetContent(Icon);
        UTextBlock* Amount = WidgetTree->ConstructWidget<UTextBlock>();
        Amount->SetJustification(ETextJustify::Center);
        Amount->SetAutoWrapText(true);
        Amount->SetWrapTextAt(220.f);
        Amount->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
        CardContent->AddChildToVerticalBox(Amount)->SetPadding(FMargin(0.f, 12.f));
        UTextBlock* Details = WidgetTree->ConstructWidget<UTextBlock>();
        Details->SetJustification(ETextJustify::Center);
        Details->SetAutoWrapText(true);
        Details->SetWrapTextAt(220.f);
        Details->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
        Details->SetVisibility(ESlateVisibility::Collapsed);
        CardContent->AddChildToVerticalBox(Details)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
        UTextBlock* Status = WidgetTree->ConstructWidget<UTextBlock>();
        Status->SetJustification(ETextJustify::Center);
        CardContent->AddChildToVerticalBox(Status);
        RewardButtons.Add(Button);
        RewardAmounts.Add(Amount);
        RewardIconSizes.Add(IconSize);
        RewardIcons.Add(Icon);
        RewardItemDetails.Add(Details);
        RewardStatuses.Add(Status);
    }
    RewardBalance = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RewardBalance"));
    RewardBalance->SetJustification(ETextJustify::Center);
    RewardBalance->SetAutoWrapText(true);
    RewardBalance->SetWrapTextAt(540.f);
    RewardsContent->AddChildToVerticalBox(RewardBalance)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
    RewardMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RewardMessage"));
    RewardMessage->SetJustification(ETextJustify::Center);
    RewardMessage->SetAutoWrapText(true);
    RewardMessage->SetWrapTextAt(540.f);
    RewardsContent->AddChildToVerticalBox(RewardMessage)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Text_Result, true, 26);
    Theme.StyleText(RewardInstruction, true, 20);
    Theme.StyleText(RewardBonusGold, true, 18);
    RewardBonusGold->SetColorAndOpacity(FLinearColor(0.95f, 0.76f, 0.34f));
    Theme.StyleText(RewardBalance, false, 18);
    Theme.StyleText(RewardMessage, false, 16);
    for (UTextBlock* Amount : RewardAmounts)
    {
        Theme.StyleText(Amount, true, 30);
        Amount->SetColorAndOpacity(FLinearColor(0.95f, 0.76f, 0.34f));
    }
    for (UTextBlock* Status : RewardStatuses) Theme.StyleText(Status, false, 16);
    for (UTextBlock* Details : RewardItemDetails) Theme.StyleText(Details, false, 14);
    Theme.StyleButton(Button_Continue, true);
    Button_Continue->OnClicked.AddUniqueDynamic(this, &UEncounterResultWidget::HandleContinueClicked);
}

void UEncounterResultWidget::ShowResult(ECombatResult Result, const FText& Message)
{
    DisplayedResult = Result;
    bRewardsComplete = true;
    bRewardSelectionAllowed = false;
    RewardsContainer->SetVisibility(ESlateVisibility::Collapsed);
    if (ResultContentSize)
    {
        ResultContentSize->SetMinDesiredWidth(540.f);
        ResultContentSize->SetMaxDesiredWidth(640.f);
    }
    const bool bVictory = Result == ECombatResult::Victory;
    Button_Continue->SetIsEnabled(bVictory && bContinueAllowed);
    Button_Continue->SetVisibility(ESlateVisibility::Collapsed);
    Text_Result->SetText(FText::FromString(TEXT("Defeat / 패배\nThe run has ended. / 진행이 종료되었습니다.")));

    if (bVictory)
    {
        Button_Continue->SetVisibility(ESlateVisibility::Visible);
        Text_Result->SetText(FText::FromString(TEXT("Victory / 승리\nEncounter complete. / 전투를 완료했습니다.")));
    }
    if (!Message.IsEmpty())
    {
        Text_Result->SetText(FText::FromString(Text_Result->GetText().ToString() + TEXT("\n") + Message.ToString()));
    }
}

void UEncounterResultWidget::RefreshResult(const FGameplayViewState& View)
{
    ShowResult(View.LastResult, View.FlowMessage);
    if (View.VictoryRestHP > 0.0f) Text_Result->SetText(FText::Format(NSLOCTEXT("CombatItemReward", "PartyRestComplete", "{0}\n휴식 완료 · 생존 파티 HP 최대 +{1}"), Text_Result->GetText(), FText::AsNumber(View.VictoryRestHP)));
    const FRunGoldRewardState& Rewards = View.GoldRewardState;
    const bool bItemRewards = Rewards.SchemaVersion == 2;
    const bool bHasRewards = View.LastResult == ECombatResult::Victory && (Rewards.SchemaVersion == 1 || bItemRewards);
    bRewardsComplete = View.GoldRewardState.SchemaVersion == 0 || View.bCanContinueAfterRewards;
    SetContinueEnabled(bContinueAllowed);
    RewardsContainer->SetVisibility(bHasRewards ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (!bHasRewards) return;
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    if (ResultContentSize && bItemRewards)
    {
        ResultContentSize->SetMinDesiredWidth(840.f);
        ResultContentSize->SetMaxDesiredWidth(900.f);
    }
    RewardInstruction->SetText(bItemRewards ? NSLOCTEXT("CombatItemReward", "ChooseOne", "전투 보상 · 아이템 3개 중 1개 선택") : NSLOCTEXT("CombatGoldReward", "ChooseOne", "전투 보상 · 3개 중 1개 선택"));
    RewardBonusGold->SetVisibility(bItemRewards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    RewardBonusGold->SetText(bItemRewards ? FText::Format(NSLOCTEXT("CombatItemReward", "BonusGold", "추가 골드 +{0}G · 아이템 선택 시 함께 지급"), FText::AsNumber(Rewards.BonusGold)) : FText::GetEmpty());
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    const FGuid CharacterId = Controller ? Controller->GetRewardCharacterId(View) : FGuid();
    const bool bPending = Controller && Controller->IsRewardSelectionPending();
    if (!bPending || RewardCharacterId != CharacterId || RewardNodeId != Rewards.NodeId) PendingChoiceIndex = INDEX_NONE;
    RewardCharacterId = CharacterId;
    RewardNodeId = Rewards.NodeId;
    const FRunPartyMember* Member = View.PartyMembers.FindByPredicate([this](const FRunPartyMember& Candidate) { return Candidate.CharacterId == RewardCharacterId; });
    const FRunGoldRewardClaim* Claim = Rewards.Claims.FindByPredicate([this](const FRunGoldRewardClaim& Candidate) { return Candidate.CharacterId == RewardCharacterId; });
    const int32 ChoiceCount = bItemRewards ? Rewards.ItemChoices.Num() : Rewards.GoldChoices.Num();
    bRewardSelectionAllowed = Controller && Member && RewardCharacterId.IsValid() && View.GoldRewardRecipientIds.Contains(RewardCharacterId) && !Claim && !bPending && !RewardNodeId.IsNone() && ChoiceCount == 3;
    for (int32 Index = 0; Index < RewardButtons.Num(); ++Index)
    {
        const bool bValid = bItemRewards ? Rewards.ItemChoices.IsValidIndex(Index) : Rewards.GoldChoices.IsValidIndex(Index);
        const bool bSelected = Claim && Claim->ChoiceIndex == Index;
        RewardButtons[Index]->SetIsEnabled(bRewardSelectionAllowed && bValid);
        RewardIconSizes[Index]->SetVisibility(bItemRewards && bValid ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        RewardItemDetails[Index]->SetVisibility(ESlateVisibility::Collapsed);
        Theme.StyleText(RewardAmounts[Index], true, bItemRewards ? 18 : 30);
        RewardAmounts[Index]->SetText(FText::GetEmpty());
        RewardButtons[Index]->SetToolTipText(FText::GetEmpty());
        if (bItemRewards && bValid)
        {
            // Present the frozen choice after claiming as well; the UI never regenerates items or skills.
            // 수령 후에도 고정 선택지를 표시하며 UI는 아이템이나 스킬을 재생성하지 않습니다.
            const FRunItemDefinition& Item = Rewards.ItemChoices[Index];
            RewardAmounts[Index]->SetText(RunItemPresentation::Name(Item, View.ItemRarities));
            if (const FRunWeaponRarityRule* Rarity = RunItemPresentation::FindRarity(Item, View.ItemRarities)) RewardAmounts[Index]->SetColorAndOpacity(Rarity->Color);
            Theme.SetItemIcon(RewardIcons[Index], Item.Tags);
            const FText Skills = RunItemPresentation::GrantedSkills(Item);
            RewardItemDetails[Index]->SetText(Skills);
            RewardItemDetails[Index]->SetVisibility(Skills.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
            RewardButtons[Index]->SetToolTipText(RunItemPresentation::Tooltip(Item, View.ItemRarities));
        }
        else if (bValid)
        {
            RewardAmounts[Index]->SetText(FText::Format(NSLOCTEXT("CombatGoldReward", "GoldAmount", "+{0}G"), FText::AsNumber(Rewards.GoldChoices[Index])));
            RewardAmounts[Index]->SetColorAndOpacity(FLinearColor(0.95f, 0.76f, 0.34f));
            RewardButtons[Index]->SetToolTipText(NSLOCTEXT("CombatGoldReward", "ClaimOnSelection", "선택하면 표시된 골드가 본인 캐릭터에게 지급됩니다. 한 번만 선택할 수 있습니다."));
        }
        RewardStatuses[Index]->SetText(bSelected ? NSLOCTEXT("CombatGoldReward", "Claimed", "획득 완료") : Claim ? NSLOCTEXT("CombatGoldReward", "NotSelected", "미선택") : NSLOCTEXT("CombatGoldReward", "Select", "선택"));
        Theme.StyleButton(RewardButtons[Index], bSelected);
    }
    RewardBalance->SetText(Member ? FText::Format(NSLOCTEXT("CombatGoldReward", "PersonalBalance", "{0} · 보유 골드 {1}G"), Member->CharacterName, FText::AsNumber(Member->Gold)) : FText::GetEmpty());
    FText Message;
    if (bPending)
    {
        const FText ItemName = Rewards.ItemChoices.IsValidIndex(PendingChoiceIndex) ? RunItemPresentation::Name(Rewards.ItemChoices[PendingChoiceIndex], View.ItemRarities) : NSLOCTEXT("CombatItemReward", "SelectedItem", "선택한 아이템");
        Message = bItemRewards ? FText::Format(NSLOCTEXT("CombatItemReward", "Pending", "{0}와 {1}G를 지급하는 중입니다."), ItemName, FText::AsNumber(Rewards.BonusGold)) : NSLOCTEXT("CombatGoldReward", "Pending", "보상을 지급하는 중입니다.");
    }
    else if (bItemRewards && Claim && Rewards.ItemChoices.IsValidIndex(Claim->ChoiceIndex))
    {
        Message = FText::Format(NSLOCTEXT("CombatItemReward", "ClaimComplete", "{0}와 {1}G를 획득했습니다."), RunItemPresentation::Name(Rewards.ItemChoices[Claim->ChoiceIndex], View.ItemRarities), FText::AsNumber(Rewards.BonusGold));
    }
    else if (Controller && !Controller->GetRewardSelectionMessage().IsEmpty())
    {
        Message = Controller->GetRewardSelectionMessage();
    }
    else if (!Member)
    {
        Message = NSLOCTEXT("CombatGoldReward", "NoRecipient", "보상을 받을 참가자가 선택을 마칠 때까지 기다려 주세요.");
    }
    else if (Claim && Rewards.GoldChoices.IsValidIndex(Claim->ChoiceIndex))
    {
        Message = FText::Format(NSLOCTEXT("CombatGoldReward", "ClaimComplete", "{0}G를 획득했습니다."), FText::AsNumber(Rewards.GoldChoices[Claim->ChoiceIndex]));
    }
    else
    {
        Message = bItemRewards ? NSLOCTEXT("CombatItemReward", "SelectionHint", "아이템을 선택하면 본인 인벤토리에 보관되며 추가 골드도 함께 지급됩니다.") : NSLOCTEXT("CombatGoldReward", "SelectionHint", "원하는 골드를 선택하면 본인 캐릭터에게 즉시 지급됩니다.");
    }
    if (Claim || !Member)
    {
        const FText ContinueHint = !bRewardsComplete ? NSLOCTEXT("CombatGoldReward", "WaitForOthers", "다른 참가자의 보상 선택을 기다리고 있습니다.") : !bContinueAllowed ? NSLOCTEXT("CombatGoldReward", "WaitForHost", "Host가 계속하기를 선택하면 다음으로 진행합니다.") : NSLOCTEXT("CombatGoldReward", "ContinueHint", "계속하기를 눌러 다음으로 진행하세요.");
        Message = FText::Format(NSLOCTEXT("CombatGoldReward", "MessageWithContinue", "{0}\n{1}"), Message, ContinueHint);
    }
    RewardMessage->SetText(Message);
}

void UEncounterResultWidget::SetContinueEnabled(bool bEnabled)
{
    bContinueAllowed = bEnabled;
    if (Button_Continue)
    {
        Button_Continue->SetIsEnabled(bContinueAllowed && bRewardsComplete && DisplayedResult == ECombatResult::Victory);
    }
}

void UEncounterResultWidget::HandleRewardSelection(FName ChoiceId)
{
    const int32 ChoiceIndex = RewardChoiceIds.IndexOfByKey(ChoiceId);
    if (!bRewardSelectionAllowed || ChoiceIndex == INDEX_NONE) return;
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>())
    {
        PendingChoiceIndex = ChoiceIndex;
        bRewardSelectionAllowed = false;
        for (UGameplayActionButton* Button : RewardButtons) Button->SetIsEnabled(false);
        Controller->RequestSelectGoldReward(RewardCharacterId, RewardNodeId, ChoiceIndex);
    }
}

void UEncounterResultWidget::HandleContinueClicked()
{
    if (!bContinueAllowed || !bRewardsComplete || DisplayedResult != ECombatResult::Victory)
    {
        return;
    }
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        Controller->RequestContinueRun();
    }
}
