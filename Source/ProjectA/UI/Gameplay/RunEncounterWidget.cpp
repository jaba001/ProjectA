#include "UI/Gameplay/RunEncounterWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
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
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Panel);
    ContentSlot->SetHorizontalAlignment(HAlign_Center);
    ContentSlot->SetVerticalAlignment(VAlign_Center);
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
    ShopBalance = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopBalance"));
    Actions->AddChildToVerticalBox(ShopBalance)->SetPadding(FMargin(0.0f, 5.0f));
    ShopHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopHint"));
    ShopHint->SetAutoWrapText(true);
    ShopHint->SetWrapTextAt(600.f);
    Actions->AddChildToVerticalBox(ShopHint)->SetPadding(FMargin(0.0f, 5.0f));
    ShopActions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopActions"));
    Actions->AddChildToVerticalBox(ShopActions);
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
    AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    BuyerCharacterId = Controller ? Controller->GetShopBuyerCharacterId(View) : FGuid();
    const FRunPartyMember* Buyer = BuyerCharacterId.IsValid() ? View.PartyMembers.FindByPredicate([this](const FRunPartyMember& Member) { return Member.CharacterId == BuyerCharacterId; }) : nullptr;
    const bool bInShop = View.Phase == ERunPhase::Shop;
    ShopBalance->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopHint->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopActions->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bInShop)
    {
        ShopBalance->SetText(Buyer ? FText::Format(NSLOCTEXT("RunSkillShop", "Balance", "{0} · 보유 골드 {1}G"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold)) : NSLOCTEXT("RunSkillShop", "NoBuyer", "구매 가능한 직접 조작 캐릭터가 없습니다."));
        ShopHint->SetText(View.SkillShopState.SchemaVersion == 0 ? NSLOCTEXT("RunSkillShop", "LegacyRun", "이전 저장에는 스킬 상점이 적용되지 않습니다. 새 Run에서 이용할 수 있습니다.") : NSLOCTEXT("RunSkillShop", "Rules", "구매한 스킬은 본인 캐릭터에만 적용되며 Run 동안 유지됩니다. 같은 스킬은 한 번만 구매할 수 있습니다."));
        while (ShopButtons.Num() < View.SkillShopState.Offers.Num())
        {
            UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
            Button->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
            ShopActions->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
            ShopButtons.Add(Button);
        }
        for (int32 Index = 0; Index < ShopButtons.Num(); ++Index)
        {
            UGameplayActionButton* Button = ShopButtons[Index];
            const bool bVisible = View.SkillShopState.Offers.IsValidIndex(Index);
            Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            if (!bVisible) continue;
            const FRunSkillShopOffer& Offer = View.SkillShopState.Offers[Index];
            const bool bOwned = Buyer && Buyer->Skills.Contains(Offer.Skill);
            const bool bAffordable = Buyer && Offer.Price > 0 && Buyer->Gold >= Offer.Price;
            const FText Status = bOwned ? NSLOCTEXT("RunSkillShop", "Owned", "보유 중") : bAffordable ? NSLOCTEXT("RunSkillShop", "Buy", "구매") : NSLOCTEXT("RunSkillShop", "CannotBuy", "구매 불가");
            Button->Configure(Offer.OfferId, FText::Format(NSLOCTEXT("RunSkillShop", "Product", "{0} · {1}G · {2}"), Offer.DisplayName, FText::AsNumber(Offer.Price), Status));
            Button->SetToolTipText(Offer.Description);
            Button->SetIsEnabled(!bOwned && bAffordable && Controller && !Controller->IsShopPurchasePending());
        }
    }
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
    FText DisplayMessage = View.FlowMessage;
    if (bInShop && Controller && !Controller->GetShopPurchaseMessage().IsEmpty()) DisplayMessage = DisplayMessage.IsEmpty() ? Controller->GetShopPurchaseMessage() : FText::Format(NSLOCTEXT("RunSkillShop", "FlowAndPurchaseMessage", "{0}\n{1}"), DisplayMessage, Controller->GetShopPurchaseMessage());
    if (bInShop && Controller && Controller->IsShopPurchasePending()) DisplayMessage = NSLOCTEXT("RunSkillShop", "Pending", "구매를 처리하는 중입니다.");
    if (DisplayMessage.IsEmpty() && !bAllowRunCommands) DisplayMessage = bInShop ? NSLOCTEXT("RunSkillShop", "HostLeaves", "본인 스킬을 구매할 수 있습니다. 상점 나가기는 Host가 결정합니다.") : NSLOCTEXT("RunEncounter", "HostOnly", "Host의 진행을 기다리는 중입니다.");
    Message->SetText(DisplayMessage);
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

void URunEncounterWidget::HandlePurchase(FName OfferId)
{
    if (!BuyerCharacterId.IsValid()) return;
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>()) Controller->RequestPurchaseShopSkill(BuyerCharacterId, OfferId);
}
