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
    ShopInventory = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopInventory"));
    Actions->AddChildToVerticalBox(ShopInventory)->SetPadding(FMargin(0.0f, 5.0f));
    ShopHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopHint"));
    ShopHint->SetAutoWrapText(true);
    ShopHint->SetWrapTextAt(600.f);
    Actions->AddChildToVerticalBox(ShopHint)->SetPadding(FMargin(0.0f, 5.0f));
    ShopActions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopActions"));
    Actions->AddChildToVerticalBox(ShopActions);
    RecoveryButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_ShopRecovery"));
    RecoveryButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
    Actions->AddChildToVerticalBox(RecoveryButton)->SetPadding(FMargin(0.0f, 5.0f));
    RerollButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_ShopReroll"));
    RerollButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
    Actions->AddChildToVerticalBox(RerollButton)->SetPadding(FMargin(0.0f, 5.0f));
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
    const FRunShopBuyerView* BuyerView = Buyer ? View.ShopBuyerViews.FindByPredicate([this](const FRunShopBuyerView& Entry) { return Entry.CharacterId == BuyerCharacterId; }) : nullptr;
    const bool bInShop = View.Phase == ERunPhase::Shop;
    const bool bItemShop = bInShop && View.EncounterProgress.SelectedEncounterId == FRunItemShopState::GetEncounterId();
    ItemShopRevision = bItemShop ? View.ItemShopState.Revision : INDEX_NONE;
    ShopBalance->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopHint->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopInventory->SetVisibility(bItemShop && Buyer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopActions->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RecoveryButton->SetVisibility(bInShop && !bItemShop && View.SkillShopState.SchemaVersion == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RerollButton->SetVisibility(bItemShop && View.ItemShopState.SchemaVersion == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bInShop)
    {
        ShopBalance->SetText(Buyer ? FText::Format(NSLOCTEXT("RunSkillShop", "Balance", "{0} · 보유 골드 {1}G"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold)) : NSLOCTEXT("RunSkillShop", "NoBuyer", "구매 가능한 직접 조작 캐릭터가 없습니다."));
        if (Buyer && BuyerView) ShopBalance->SetText(FText::Format(NSLOCTEXT("RunSkillShop", "BalanceAndHP", "{0} · 보유 골드 {1}G · HP {2}/{3}"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold), FText::AsNumber(Buyer->CurrentHP), FText::AsNumber(BuyerView->MaxHP)));
        if (bItemShop)
        {
            ShopHint->SetText(View.ItemShopState.SchemaVersion == 0 ? NSLOCTEXT("RunItemShop", "LegacyRun", "이전 저장에는 아이템 상점이 적용되지 않습니다. 새 Run에서 이용할 수 있습니다.") : NSLOCTEXT("RunItemShop", "Rules", "중복 없이 5개 추첨 · 리롤로 전체 상품 갱신\n구매한 아이템은 본인 캐릭터에 보관되며 장착 효과는 아직 없습니다."));
            if (Buyer)
            {
                ShopInventory->SetText(FText::Format(NSLOCTEXT("RunItemShop", "Inventory", "보유 아이템 {0}개 · 이름은 마우스를 올려 확인"), FText::AsNumber(Buyer->Items.Num())));
                TArray<FString> ItemNames;
                for (const FRunItemDefinition& Item : Buyer->Items) ItemNames.Add(Item.DisplayName.ToString());
                ShopInventory->SetToolTipText(ItemNames.IsEmpty() ? NSLOCTEXT("RunItemShop", "EmptyInventory", "보유한 아이템이 없습니다.") : FText::FromString(FString::Join(ItemNames, TEXT("\n"))));
            }
        }
        else ShopHint->SetText(View.SkillShopState.SchemaVersion == 0 ? NSLOCTEXT("RunSkillShop", "LegacyRun", "이전 저장에는 스킬 상점이 적용되지 않습니다. 새 Run에서 이용할 수 있습니다.") : NSLOCTEXT("RunSkillShop", "Rules", "구매한 스킬은 본인 캐릭터에만 적용되며 Run 동안 유지됩니다. 같은 스킬은 한 번만 구매할 수 있습니다."));
        const int32 OfferCount = bItemShop ? View.ItemShopState.Offers.Num() : View.SkillShopState.Offers.Num();
        while (ShopButtons.Num() < OfferCount)
        {
            UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
            Button->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
            ShopActions->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
            ShopButtons.Add(Button);
        }
        for (int32 Index = 0; Index < ShopButtons.Num(); ++Index)
        {
            UGameplayActionButton* Button = ShopButtons[Index];
            const bool bVisible = Index < OfferCount;
            Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            if (!bVisible) continue;
            if (bItemShop)
            {
                const FRunItemShopOffer& Offer = View.ItemShopState.Offers[Index];
                const bool bAffordable = Buyer && Offer.Item.Price > 0 && Buyer->Gold >= Offer.Item.Price;
                const FText Status = Offer.bSold ? NSLOCTEXT("RunItemShop", "Sold", "판매 완료") : bAffordable ? NSLOCTEXT("RunItemShop", "Buy", "구매") : NSLOCTEXT("RunItemShop", "CannotBuy", "구매 불가");
                Button->Configure(Offer.OfferId, FText::Format(NSLOCTEXT("RunItemShop", "Product", "{0} · {1}G · {2}"), Offer.Item.DisplayName, FText::AsNumber(Offer.Item.Price), Status));
                if (UTextBlock* ProductText = Cast<UTextBlock>(Button->GetContent()))
                {
                    ProductText->SetAutoWrapText(true);
                    ProductText->SetWrapTextAt(560.f);
                    ProductText->SetJustification(ETextJustify::Center);
                }
                Button->SetToolTipText(Offer.Item.DisplayName);
                Button->SetIsEnabled(View.ItemShopState.SchemaVersion == 1 && !Offer.bSold && bAffordable && Controller && !Controller->IsShopPurchasePending());
                continue;
            }
            const FRunSkillShopOffer& Offer = View.SkillShopState.Offers[Index];
            const bool bOwned = Buyer && Buyer->Skills.Contains(Offer.Skill);
            const bool bAffordable = Buyer && Offer.Price > 0 && Buyer->Gold >= Offer.Price;
            const FText Status = bOwned ? NSLOCTEXT("RunSkillShop", "Owned", "보유 중") : bAffordable ? NSLOCTEXT("RunSkillShop", "Buy", "구매") : NSLOCTEXT("RunSkillShop", "CannotBuy", "구매 불가");
            Button->Configure(Offer.OfferId, FText::Format(NSLOCTEXT("RunSkillShop", "Product", "{0} · {1}G · {2}"), Offer.DisplayName, FText::AsNumber(Offer.Price), Status));
            Button->SetToolTipText(Offer.Description);
            Button->SetIsEnabled(!bOwned && bAffordable && Controller && !Controller->IsShopPurchasePending());
        }
        const bool bCanRecover = Buyer && BuyerView && Buyer->CurrentHP > 0.f && Buyer->CurrentHP < BuyerView->MaxHP;
        const bool bRecoveryAffordable = Buyer && View.SkillShopState.Recovery.Price > 0 && Buyer->Gold >= View.SkillShopState.Recovery.Price;
        const FText RecoveryStatus = Buyer && BuyerView && Buyer->CurrentHP >= BuyerView->MaxHP ? NSLOCTEXT("RunSkillShop", "FullHP", "HP 가득 참") : bCanRecover && bRecoveryAffordable ? NSLOCTEXT("RunSkillShop", "Buy", "구매") : NSLOCTEXT("RunSkillShop", "CannotBuy", "구매 불가");
        RecoveryButton->Configure(FRunSkillShopState::GetRecoveryOfferId(), FText::Format(NSLOCTEXT("RunSkillShop", "RecoveryProduct", "HP 전체 회복 · {0}G · {1}"), FText::AsNumber(View.SkillShopState.Recovery.Price), RecoveryStatus));
        RecoveryButton->SetToolTipText(NSLOCTEXT("RunSkillShop", "RecoveryDescription", "본인 생존 캐릭터의 HP를 즉시 최대치까지 회복합니다. HP가 가득 차 있으면 구매할 수 없습니다."));
        RecoveryButton->SetIsEnabled(View.SkillShopState.SchemaVersion == 1 && bCanRecover && bRecoveryAffordable && Controller && !Controller->IsShopPurchasePending());
        const bool bRerollAffordable = Buyer && View.ItemShopState.RerollPrice > 0 && Buyer->Gold >= View.ItemShopState.RerollPrice;
        RerollButton->Configure(FRunItemShopState::GetRerollOfferId(), FText::Format(NSLOCTEXT("RunItemShop", "Reroll", "리롤 · {0}G"), FText::AsNumber(View.ItemShopState.RerollPrice)));
        RerollButton->SetToolTipText(NSLOCTEXT("RunItemShop", "RerollDescription", "본인 골드를 사용하여 모두에게 표시되는 5개 상품을 다시 추첨합니다. 이전 상품이 다시 나올 수 있습니다."));
        RerollButton->SetIsEnabled(bItemShop && View.ItemShopState.SchemaVersion == 1 && bRerollAffordable && Controller && !Controller->IsShopPurchasePending());
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
    if (bInShop && Controller && Controller->IsShopPurchasePending()) DisplayMessage = bItemShop ? NSLOCTEXT("RunItemShop", "Pending", "상점 요청을 처리하는 중입니다.") : NSLOCTEXT("RunSkillShop", "Pending", "구매를 처리하는 중입니다.");
    if (DisplayMessage.IsEmpty() && !bAllowRunCommands)
    {
        if (bItemShop) DisplayMessage = NSLOCTEXT("RunItemShop", "HostLeaves", "본인 골드로 아이템 구매와 리롤을 할 수 있습니다. 상점 나가기는 Host가 결정합니다.");
        else DisplayMessage = bInShop ? NSLOCTEXT("RunSkillShop", "HostLeaves", "본인 캐릭터의 스킬과 HP 회복을 구매할 수 있습니다. 상점 나가기는 Host가 결정합니다.") : NSLOCTEXT("RunEncounter", "HostOnly", "Host의 진행을 기다리는 중입니다.");
    }
    Message->SetText(DisplayMessage);
    if (View.Phase == ERunPhase::EncounterChoice)
    {
        Title->SetText(NSLOCTEXT("RunEncounter", "Choose", "인카운터 선택"));
    }
    else if (View.Phase == ERunPhase::Shop)
    {
        const FRunEncounterOffer* Selected = View.EncounterProgress.Offers.FindByPredicate([&View](const FRunEncounterOffer& Offer) { return Offer.EncounterId == View.EncounterProgress.SelectedEncounterId; });
        Title->SetText(bItemShop ? NSLOCTEXT("RunItemShop", "Title", "상점2 · 아이템 상점") : Selected ? Selected->DisplayName : NSLOCTEXT("RunEncounter", "Shop", "상점"));
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
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>()) Controller->RequestPurchaseShopOffer(BuyerCharacterId, OfferId, ItemShopRevision);
}
