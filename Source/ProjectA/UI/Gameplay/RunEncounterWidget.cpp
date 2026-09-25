#include "UI/Gameplay/RunEncounterWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/CharacterEquipmentPanel.h"
#include "UI/Gameplay/CharacterInventoryPanel.h"
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
    UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Fit);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    ContentSlot->SetPadding(FMargin(32.0f, 64.0f, 32.0f, 32.0f));
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
    Fit->SetContent(Columns);
    EquipmentSize = WidgetTree->ConstructWidget<USizeBox>();
    EquipmentSize->SetWidthOverride(300.0f);
    EquipmentSize->SetHeightOverride(840.0f);
    EquipmentPanel = CreateWidget<UCharacterEquipmentPanel>(GetOwningPlayer());
    EquipmentSize->SetContent(EquipmentPanel);
    Columns->AddChildToHorizontalBox(EquipmentSize)->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
    MerchantSize = WidgetTree->ConstructWidget<USizeBox>();
    MerchantSize->SetWidthOverride(620.0f);
    MerchantSize->SetHeightOverride(840.0f);
    Columns->AddChildToHorizontalBox(MerchantSize);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EncounterPanel"));
    Theme.StylePanel(Panel);
    Panel->SetPadding(FMargin(24.0f, 40.0f, 24.0f, 24.0f));
    MerchantSize->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);
    UBorder* TitleBar = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleSectionHeader(TitleBar);
    TitleBar->SetPadding(FMargin(20.0f, 8.0f));
    Content->AddChildToVerticalBox(TitleBar);
    Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_EncounterTitle"));
    Title->SetJustification(ETextJustify::Center);
    TitleBar->SetContent(Title);
    Theme.AddDivider(WidgetTree, Content);
    UScrollBox* MerchantScroll = WidgetTree->ConstructWidget<UScrollBox>();
    MerchantScroll->SetOrientation(Orient_Vertical);
    MerchantScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    Content->AddChildToVerticalBox(MerchantScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Actions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EncounterActions"));
    MerchantScroll->AddChild(Actions);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
        Button->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandleSelection);
        Actions->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
        ChoiceButtons.Add(Button);
    }
    ShopBalance = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopBalance"));
    Actions->AddChildToVerticalBox(ShopBalance)->SetPadding(FMargin(0.0f, 5.0f));
    ShopBalance->SetAutoWrapText(true);
    ShopBalance->SetWrapTextAt(540.0f);
    ShopHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ShopHint"));
    ShopHint->SetAutoWrapText(true);
    ShopHint->SetWrapTextAt(540.0f);
    Actions->AddChildToVerticalBox(ShopHint)->SetPadding(FMargin(0.0f, 5.0f));
    ShopActions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopActions"));
    Actions->AddChildToVerticalBox(ShopActions);
    RecoveryButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_ShopRecovery"));
    RecoveryButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
    Content->AddChildToVerticalBox(RecoveryButton)->SetPadding(FMargin(0.0f, 5.0f));
    RerollButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_ShopReroll"));
    RerollButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
    Content->AddChildToVerticalBox(RerollButton)->SetPadding(FMargin(0.0f, 5.0f));
    LeaveButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_LeaveShop"));
    LeaveButton->Configure(TEXT("Leave"), NSLOCTEXT("RunEncounter", "Leave", "나가기"));
    LeaveButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandleLeave);
    Content->AddChildToVerticalBox(LeaveButton)->SetPadding(FMargin(0.0f, 5.0f));
    Message = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_EncounterMessage"));
    Message->SetAutoWrapText(true);
    Message->SetWrapTextAt(540.0f);
    Content->AddChildToVerticalBox(Message)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    InventorySize = WidgetTree->ConstructWidget<USizeBox>();
    InventorySize->SetWidthOverride(430.0f);
    InventorySize->SetHeightOverride(840.0f);
    InventoryPanel = CreateWidget<UCharacterInventoryPanel>(GetOwningPlayer());
    InventorySize->SetContent(InventoryPanel);
    Columns->AddChildToHorizontalBox(InventorySize)->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Title, true, 28);
    Theme.StyleText(ShopBalance, true, 18);
    Theme.StyleText(ShopHint, false, 14);
    Theme.StyleText(Message, false, 14);
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
    const bool bItemShop = bInShop && View.EncounterProgress.IsItemShop();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    EquipmentSize->SetVisibility(bInShop ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    InventorySize->SetVisibility(bInShop ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    MerchantSize->SetHeightOverride(bInShop ? 840.0f : 440.0f);
    if (bInShop)
    {
        // Reading owned inventory remains available when the character cannot buy, including after death.
        // 사망 등으로 구매할 수 없어도 본인 캐릭터의 보유 현황은 계속 열람합니다.
        const FGuid InventoryCharacterId = Controller ? Controller->GetInventoryCharacterId(View) : FGuid();
        EquipmentPanel->RefreshEquipment(View, InventoryCharacterId);
        InventoryPanel->RefreshInventory(View, InventoryCharacterId);
    }
    ItemShopRevision = bItemShop ? View.ItemShopState.Revision : INDEX_NONE;
    ShopBalance->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopHint->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopActions->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RecoveryButton->SetVisibility(bInShop && !bItemShop && View.SkillShopState.SchemaVersion == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RerollButton->SetVisibility(bItemShop && View.ItemShopState.SchemaVersion == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bInShop)
    {
        ShopBalance->SetText(Buyer ? FText::Format(NSLOCTEXT("RunSkillShop", "Balance", "{0} · 보유 골드 {1}G"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold)) : NSLOCTEXT("RunSkillShop", "NoBuyer", "구매 가능한 직접 조작 캐릭터가 없습니다."));
        if (Buyer && BuyerView) ShopBalance->SetText(FText::Format(NSLOCTEXT("RunSkillShop", "BalanceAndHP", "{0} · 보유 골드 {1}G · HP {2}/{3}"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold), FText::AsNumber(Buyer->CurrentHP), FText::AsNumber(BuyerView->MaxHP)));
        if (bItemShop)
        {
            ShopHint->SetText(View.ItemShopState.SchemaVersion == 0 ? NSLOCTEXT("RunItemShop", "LegacyRun", "이전 저장에는 아이템 상점이 적용되지 않습니다. 새 Run에서 이용할 수 있습니다.") : NSLOCTEXT("RunItemShop", "InventoryRules", "중복 없이 5개 추첨 · 구매한 아이템은 오른쪽 인벤토리에 보관됩니다."));
        }
        else ShopHint->SetText(View.SkillShopState.SchemaVersion == 0 ? NSLOCTEXT("RunSkillShop", "LegacyRun", "이전 저장에는 스킬 상점이 적용되지 않습니다. 새 Run에서 이용할 수 있습니다.") : NSLOCTEXT("RunSkillShop", "Rules", "구매한 스킬은 본인 캐릭터에만 적용되며 Run 동안 유지됩니다. 같은 스킬은 한 번만 구매할 수 있습니다."));
        const int32 OfferCount = bItemShop ? View.ItemShopState.Offers.Num() : View.SkillShopState.Offers.Num();
        while (ShopButtons.Num() < OfferCount)
        {
            UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
            Theme.StyleInset(Card);
            Card->SetPadding(FMargin(12.0f));
            ShopActions->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.0f, 4.0f));
            ShopCards.Add(Card);
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
            Card->SetContent(Row);
            USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
            IconSize->SetWidthOverride(44.0f);
            IconSize->SetHeightOverride(44.0f);
            UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize);
            IconSlot->SetVerticalAlignment(VAlign_Center);
            IconSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
            UImage* Icon = WidgetTree->ConstructWidget<UImage>();
            IconSize->SetContent(Icon);
            ShopIcons.Add(Icon);
            UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
            UHorizontalBoxSlot* InfoSlot = Row->AddChildToHorizontalBox(Info);
            InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            InfoSlot->SetVerticalAlignment(VAlign_Center);
            InfoSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
            UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>();
            Name->SetAutoWrapText(true);
            Name->SetWrapTextAt(320.0f);
            Name->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
            Theme.StyleText(Name, true, 17);
            Info->AddChildToVerticalBox(Name);
            ShopNames.Add(Name);
            UTextBlock* Price = WidgetTree->ConstructWidget<UTextBlock>();
            Theme.StyleText(Price, false, 14);
            Info->AddChildToVerticalBox(Price)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
            ShopPrices.Add(Price);
            USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
            ButtonSize->SetWidthOverride(124.0f);
            Row->AddChildToHorizontalBox(ButtonSize)->SetVerticalAlignment(VAlign_Center);
            UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>();
            Button->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandlePurchase);
            ButtonSize->SetContent(Button);
            ShopButtons.Add(Button);
        }
        for (int32 Index = 0; Index < ShopButtons.Num(); ++Index)
        {
            UGameplayActionButton* Button = ShopButtons[Index];
            const bool bVisible = Index < OfferCount;
            ShopCards[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            if (!bVisible) continue;
            if (bItemShop)
            {
                const FRunItemShopOffer& Offer = View.ItemShopState.Offers[Index];
                const bool bAffordable = Buyer && Offer.Item.Price > 0 && Buyer->Gold >= Offer.Item.Price;
                const FText Status = Offer.bSold ? NSLOCTEXT("RunItemShop", "Sold", "판매 완료") : bAffordable ? NSLOCTEXT("RunItemShop", "Buy", "구매") : NSLOCTEXT("RunItemShop", "CannotBuy", "구매 불가");
                Button->Configure(Offer.OfferId, Status);
                ShopNames[Index]->SetText(Offer.Item.DisplayName);
                ShopPrices[Index]->SetText(FText::Format(NSLOCTEXT("RunItemShop", "Price", "{0}G"), FText::AsNumber(Offer.Item.Price)));
                Theme.SetItemIcon(ShopIcons[Index], Offer.Item.Tags);
                ShopIcons[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
                ShopCards[Index]->SetRenderOpacity(Offer.bSold ? 0.55f : 1.0f);
                ShopCards[Index]->SetToolTipText(Offer.Item.DisplayName);
                Button->SetToolTipText(Offer.Item.DisplayName);
                Button->SetIsEnabled(View.ItemShopState.SchemaVersion == 1 && !Offer.bSold && bAffordable && Controller && !Controller->IsShopPurchasePending());
                continue;
            }
            const FRunSkillShopOffer& Offer = View.SkillShopState.Offers[Index];
            const bool bOwned = Buyer && Buyer->Skills.Contains(Offer.Skill);
            const bool bAffordable = Buyer && Offer.Price > 0 && Buyer->Gold >= Offer.Price;
            const FText Status = bOwned ? NSLOCTEXT("RunSkillShop", "Owned", "보유 중") : bAffordable ? NSLOCTEXT("RunSkillShop", "Buy", "구매") : NSLOCTEXT("RunSkillShop", "CannotBuy", "구매 불가");
            Button->Configure(Offer.OfferId, Status);
            ShopNames[Index]->SetText(Offer.DisplayName);
            ShopPrices[Index]->SetText(FText::Format(NSLOCTEXT("RunSkillShop", "Price", "{0}G"), FText::AsNumber(Offer.Price)));
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
            ShopIcons[Index]->SetVisibility(Skill && Skill->SkillIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
            if (Skill && Skill->SkillIcon) ShopIcons[Index]->SetBrushFromTexture(Skill->SkillIcon);
            ShopCards[Index]->SetRenderOpacity(bOwned ? 0.55f : 1.0f);
            ShopCards[Index]->SetToolTipText(Offer.Description);
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
            Button->Configure(Offer.EncounterId, Offer.GetDisplayName());
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
    Message->SetVisibility(DisplayMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (View.Phase == ERunPhase::EncounterChoice)
    {
        Title->SetText(NSLOCTEXT("RunEncounter", "Choose", "인카운터 선택"));
    }
    else if (View.Phase == ERunPhase::Shop)
    {
        const FRunEncounterOffer* Selected = View.EncounterProgress.FindSelectedOffer();
        Title->SetText(Selected ? Selected->GetDisplayName() : NSLOCTEXT("RunEncounter", "Shop", "상점"));
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
