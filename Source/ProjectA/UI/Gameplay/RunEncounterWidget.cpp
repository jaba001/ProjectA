#include "UI/Gameplay/RunEncounterWidget.h"
#include "UI/ProjectALocalization.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/CharacterEquipmentPanel.h"
#include "UI/Gameplay/CharacterInventoryPanel.h"
#include "UI/Gameplay/RunItemPresentation.h"
#include "UI/Gameplay/ShopItemTooltipWidget.h"
#include "UI/Theme/DemonicUITheme.h"

namespace
{
    FText ShopActionStatus(bool bAvailable, bool bHasBuyer, bool bAffordable, bool bPending, const FText& Unavailable)
    {
        if (!bAvailable) return Unavailable;
        if (!bHasBuyer) return NSLOCTEXT("RunShop", "NoEligibleBuyer", "대상 없음");
        if (bPending) return NSLOCTEXT("RunShop", "ActionPending", "처리 중");
        if (!bAffordable) return NSLOCTEXT("RunShop", "InsufficientGold", "골드 부족");
        return NSLOCTEXT("RunShop", "Buy", "구매");
    }

    FText EncounterShopDescription(const FRunEncounterOffer& Offer)
    {
        const FGameplayTag Tag = Offer.GetResolvedTag();
        if (Tag.MatchesTag(FRunEncounterOffer::GetRarityItemShopTag())) return NSLOCTEXT("RunShop", "RarityStock", "선택한 등급의 상품을 최대 5개 진열합니다. 해당 등급의 가용 상품이 적으면 진열 수도 줄어듭니다.");
        if (Tag.MatchesTag(FRunEncounterOffer::GetTagItemShopTag())) return NSLOCTEXT("RunShop", "TagStock", "선택한 종류의 상품을 최대 5개 진열합니다. 해당 종류의 가용 상품이 적으면 진열 수도 줄어듭니다.");
        if (Tag.MatchesTag(FRunEncounterOffer::GetItemShopTag())) return NSLOCTEXT("RunShop", "BasicStock", "이 Run의 아이템 후보에서 서로 다른 상품 5개를 진열합니다.");
        if (Tag.MatchesTag(FRunEncounterOffer::GetRecoveryTag())) return NSLOCTEXT("RunShop", "RecoveryChoice", "생존한 본인 캐릭터의 HP를 회복합니다. 최대 HP이면 구매할 수 없습니다.");
        if (Tag.MatchesTag(FRunEncounterOffer::GetRevivalTag())) return NSLOCTEXT("RunShop", "RevivalChoice", "사망한 본인 직접 조작 캐릭터를 부활시킵니다. AI 동료는 대상이 아닙니다.");
        if (Tag.MatchesTag(FRunEncounterOffer::GetConsumableShopTag())) return NSLOCTEXT("RunShop", "ConsumableChoice", "전투에서 사용할 회복 소모품을 구매합니다. 본인 생존 캐릭터만 구매할 수 있습니다.");
        return NSLOCTEXT("RunShop", "SkillChoice", "이 Run에 저장된 스킬 상품과 HP 회복 서비스를 이용합니다.");
    }
}

TOptional<FUIInputConfig> URunEncounterWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void URunEncounterWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleBackdrop(Backdrop);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Backdrop);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    ContentFit = WidgetTree->ConstructWidget<UScaleBox>();
    ContentFit->SetStretch(EStretch::ScaleToFit);
    ContentFit->SetStretchDirection(EStretchDirection::DownOnly);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(ContentFit);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    ContentSlot->SetPadding(FMargin(32.0f, 64.0f, 32.0f, 32.0f));
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
    Columns->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    ContentFit->SetContent(Columns);
    if (UScaleBoxSlot* FitSlot = Cast<UScaleBoxSlot>(Columns->Slot))
    {
        FitSlot->SetHorizontalAlignment(HAlign_Center);
        FitSlot->SetVerticalAlignment(VAlign_Center);
    }
    EquipmentSize = WidgetTree->ConstructWidget<USizeBox>();
    EquipmentSize->SetWidthOverride(300.0f);
    EquipmentSize->SetHeightOverride(840.0f);
    EquipmentSize->SetClipping(EWidgetClipping::ClipToBounds);
    EquipmentPanel = CreateWidget<UCharacterEquipmentPanel>(GetOwningPlayer());
    EquipmentSize->SetContent(EquipmentPanel);
    Columns->AddChildToHorizontalBox(EquipmentSize)->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
    InventorySize = WidgetTree->ConstructWidget<USizeBox>();
    InventorySize->SetWidthOverride(430.0f);
    InventorySize->SetHeightOverride(840.0f);
    InventorySize->SetClipping(EWidgetClipping::ClipToBounds);
    InventoryPanel = CreateWidget<UCharacterInventoryPanel>(GetOwningPlayer());
    InventorySize->SetContent(InventoryPanel);
    Columns->AddChildToHorizontalBox(InventorySize);
    // Reserve actual layout space for the NPC instead of overlaying either inventory or merchandise on the face.
    // 가방이나 상품이 얼굴 위에 겹치지 않도록 NPC를 위한 실제 배치 공간을 확보합니다.
    NpcFocusGap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopNpcFocusGap"));
    NpcFocusGap->SetWidthOverride(480.0f);
    NpcFocusGap->SetHeightOverride(840.0f);
    NpcFocusGap->SetVisibility(ESlateVisibility::HitTestInvisible);
    NpcFocusGap->SetContent(WidgetTree->ConstructWidget<USpacer>());
    Columns->AddChildToHorizontalBox(NpcFocusGap);
    MerchantSize = WidgetTree->ConstructWidget<USizeBox>();
    MerchantSize->SetWidthOverride(620.0f);
    MerchantSize->SetHeightOverride(840.0f);
    MerchantSize->SetClipping(EWidgetClipping::ClipToBounds);
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
    Title->SetAutoWrapText(true);
    Title->SetWrapTextAt(532.0f);
    Title->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
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
    InventoryButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_ShopInventory"));
    InventoryButton->Configure(TEXT("Inventory"), NSLOCTEXT("RunEncounter", "Inventory", "인벤토리 · 장비 보기 (I)"));
    InventoryButton->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandleInventory);
    Content->AddChildToVerticalBox(InventoryButton)->SetPadding(FMargin(0.0f, 5.0f));
    Message = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_EncounterMessage"));
    Message->SetAutoWrapText(true);
    Message->SetWrapTextAt(540.0f);
    Content->AddChildToVerticalBox(Message)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    DungeonChoiceFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("DungeonChoiceBar"));
    DungeonChoiceFit->SetStretch(EStretch::ScaleToFit);
    DungeonChoiceFit->SetStretchDirection(EStretchDirection::DownOnly);
    DungeonChoiceFit->SetVisibility(ESlateVisibility::Collapsed);
    UOverlaySlot* DungeonSlot = Root->AddChildToOverlay(DungeonChoiceFit);
    DungeonSlot->SetHorizontalAlignment(HAlign_Fill);
    DungeonSlot->SetVerticalAlignment(VAlign_Bottom);
    DungeonSlot->SetPadding(FMargin(32.0f, 0.0f, 32.0f, 24.0f));
    USizeBox* DungeonSize = WidgetTree->ConstructWidget<USizeBox>();
    DungeonSize->SetWidthOverride(1120.0f);
    DungeonChoiceFit->SetContent(DungeonSize);
    UBorder* DungeonPanel = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(DungeonPanel);
    DungeonPanel->SetPadding(FMargin(16.0f, 12.0f));
    DungeonSize->SetContent(DungeonPanel);
    UVerticalBox* DungeonContent = WidgetTree->ConstructWidget<UVerticalBox>();
    DungeonPanel->SetContent(DungeonContent);
    DungeonChoiceTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_DungeonChoiceTitle"));
    DungeonChoiceTitle->SetJustification(ETextJustify::Center);
    DungeonContent->AddChildToVerticalBox(DungeonChoiceTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    UHorizontalBox* DungeonChoices = WidgetTree->ConstructWidget<UHorizontalBox>();
    DungeonContent->AddChildToVerticalBox(DungeonChoices);
    // Keep offer indices zero, one and two aligned with the left, straight and right corridors.
    // 후보 0·1·2의 순서를 각각 왼쪽·직진·오른쪽 통로에 고정합니다.
    for (int32 Index = 0; Index < 3; ++Index)
    {
        USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
        CardSize->SetMinDesiredHeight(104.0f);
        UHorizontalBoxSlot* CardSlot = DungeonChoices->AddChildToHorizontalBox(CardSize);
        CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        CardSlot->SetPadding(FMargin(6.0f, 0.0f));
        UGameplayActionButton* Button = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), FName(*FString::Printf(TEXT("Button_DungeonChoice_%d"), Index)));
        Button->OnActionRequested.AddUObject(this, &URunEncounterWidget::HandleSelection);
        CardSize->SetContent(Button);
        DungeonChoiceButtons.Add(Button);
    }
    DungeonChoiceMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_DungeonChoiceMessage"));
    DungeonChoiceMessage->SetJustification(ETextJustify::Center);
    DungeonChoiceMessage->SetAutoWrapText(true);
    DungeonChoiceMessage->SetWrapTextAt(1040.0f);
    DungeonContent->AddChildToVerticalBox(DungeonChoiceMessage)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Title, true, 28);
    Theme.StyleText(ShopBalance, true, 18);
    Theme.StyleText(ShopHint, false, 14);
    Theme.StyleText(Message, false, 14);
    Theme.StyleText(DungeonChoiceTitle, true, 20);
    Theme.StyleText(DungeonChoiceMessage, false, 16);
}

void URunEncounterWidget::RefreshEncounter(const FGameplayViewState& View, bool bAllowRunCommands, bool bWorldPresentation)
{
    if (!Actions) return;
    bRunCommandsAllowed = bAllowRunCommands;
    AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    BuyerCharacterId = Controller ? Controller->GetShopBuyerCharacterId(View) : FGuid();
    const FRunPartyMember* Buyer = BuyerCharacterId.IsValid() ? View.PartyMembers.FindByPredicate([this](const FRunPartyMember& Member) { return Member.CharacterId == BuyerCharacterId; }) : nullptr;
    const FRunShopBuyerView* BuyerView = Buyer ? View.ShopBuyerViews.FindByPredicate([this](const FRunShopBuyerView& Entry) { return Entry.CharacterId == BuyerCharacterId; }) : nullptr;
    const bool bInShop = View.Phase == ERunPhase::Shop;
    const bool bDungeonChoice = bWorldPresentation && View.Phase == ERunPhase::EncounterChoice;
    const bool bItemShop = bInShop && View.EncounterProgress.IsItemShop();
    TArray<int32> VisibleItemOfferIndices;
    if (bItemShop)
    {
        // Filter presentation without rewriting frozen stock or changing saved offer identities.
        // 고정된 진열과 저장된 상품 식별자는 변경하지 않고 표시 대상만 선별합니다.
        const URunEquipmentCatalog& Equipment = URunEquipmentCatalog::Get();
        for (int32 Index = 0; Index < View.ItemShopState.Offers.Num(); ++Index)
        {
            if (Equipment.ResolveProfile(View.ItemShopState.Offers[Index].Item)) VisibleItemOfferIndices.Add(Index);
        }
    }
    const FRunEncounterOffer* SelectedOffer = View.EncounterProgress.FindSelectedOffer();
    const bool bService = bInShop && SelectedOffer && SelectedOffer->IsService();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    Backdrop->SetVisibility(bWorldPresentation ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ContentFit->SetVisibility(bDungeonChoice ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    DungeonChoiceFit->SetVisibility(bDungeonChoice ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (UOverlaySlot* ContentSlot = Cast<UOverlaySlot>(ContentFit->Slot)) ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    MerchantSize->SetWidthOverride(620.0f);
    EquipmentSize->SetVisibility(bInShop ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    InventorySize->SetVisibility(bInShop ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    NpcFocusGap->SetWidthOverride(bWorldPresentation ? 480.0f : 16.0f);
    NpcFocusGap->SetVisibility(bInShop ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    InventoryButton->SetVisibility(ESlateVisibility::Collapsed);
    MerchantSize->SetHeightOverride(bInShop ? 840.0f : 440.0f);
    if (bInShop)
    {
        // Reading owned inventory remains available when the character cannot buy, including after death.
        // 사망 등으로 구매할 수 없어도 본인 캐릭터의 보유 현황은 계속 열람합니다.
        const FGuid InventoryCharacterId = Controller ? Controller->GetInventoryCharacterId(View) : FGuid();
        EquipmentPanel->RefreshEquipment(View, InventoryCharacterId);
        InventoryPanel->RefreshInventory(View, InventoryCharacterId);
    }
    ShopRevision = bInShop ? (bService ? View.RecoveryState.Revision : bItemShop ? View.ItemShopState.Revision : View.SkillShopState.Revision) : INDEX_NONE;
    ShopBalance->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopHint->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ShopActions->SetVisibility(bInShop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (!bInShop)
    {
        for (int32 Index = 0; Index < ShopCards.Num(); ++Index)
        {
            ShopCards[Index]->SetToolTipText(FText::GetEmpty());
            ShopCards[Index]->SetToolTip(nullptr);
            ShopButtons[Index]->SetToolTipText(FText::GetEmpty());
        }
    }
    RecoveryButton->SetVisibility(bService || (bInShop && !bItemShop && View.SkillShopState.SchemaVersion == 1) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RerollButton->SetVisibility(bInShop && !bService && (bItemShop ? View.ItemShopState.SchemaVersion == 1 : View.SkillShopState.SchemaVersion == 1) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bInShop)
    {
        ShopBalance->SetText(Buyer ? FText::Format(NSLOCTEXT("RunSkillShop", "Balance", "{0} · 보유 골드 {1}G"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold)) : NSLOCTEXT("RunSkillShop", "NoBuyer", "구매 가능한 직접 조작 캐릭터가 없습니다."));
        if (Buyer && BuyerView) ShopBalance->SetText(FText::Format(NSLOCTEXT("RunSkillShop", "BalanceAndHP", "{0} · 보유 골드 {1}G · HP {2}/{3}"), Buyer->CharacterName, FText::AsNumber(Buyer->Gold), FText::AsNumber(Buyer->CurrentHP), FText::AsNumber(BuyerView->MaxHP)));
        if (bItemShop)
        {
            FText Hint = SelectedOffer ? EncounterShopDescription(*SelectedOffer) : FText::GetEmpty();
            int32 Remaining = 0;
            for (const int32 Index : VisibleItemOfferIndices) if (!View.ItemShopState.Offers[Index].bSold) ++Remaining;
            Hint = FText::Format(NSLOCTEXT("RunItemShop", "StockSummaryWithDetails", "{0}\n진열 {1}개 · 미판매 {2}개 · 상품 위에 마우스를 올려 상세 확인\n구매한 아이템은 인벤토리에 보관되며 자동 장착되지 않습니다."), Hint, FText::AsNumber(VisibleItemOfferIndices.Num()), FText::AsNumber(Remaining));
            if (VisibleItemOfferIndices.IsEmpty()) Hint = FText::Format(NSLOCTEXT("RunItemShop", "EmptyStockHint", "{0}\n{1}"), Hint, View.bCanRerollItemShop ? NSLOCTEXT("RunItemShop", "EmptyStockReroll", "현재 진열에 장착 가능한 상품이 없습니다. 리롤하여 새 상품을 확인하거나 나갈 수 있습니다.") : NSLOCTEXT("RunItemShop", "EmptyStockLeave", "이 상점 조건에 맞는 장착 가능 상품이 부족하여 리롤할 수 없습니다. 나가기를 선택해 진행할 수 있습니다."));
            else if (Remaining == 0) Hint = FText::Format(NSLOCTEXT("RunItemShop", "SoldStockHint", "{0}\n{1}"), Hint, View.bCanRerollItemShop ? NSLOCTEXT("RunItemShop", "SoldStockReroll", "현재 상품은 모두 판매되었습니다. 리롤하여 새 상품을 확인하거나 나갈 수 있습니다.") : NSLOCTEXT("RunItemShop", "SoldStockLeave", "현재 상품은 모두 판매되었으며 리롤할 수 없습니다. 나가기를 선택해 진행할 수 있습니다."));
            ShopHint->SetText(View.ItemShopState.SchemaVersion == 0 ? NSLOCTEXT("RunItemShop", "LegacyRun", "이전 저장에는 아이템 상점이 적용되지 않습니다. 새 Run에서 이용할 수 있습니다.") : Hint);
        }
        else if (bService) ShopHint->SetText(FText::GetEmpty());
        else if (View.SkillShopState.SchemaVersion == 0) ShopHint->SetText(NSLOCTEXT("RunSkillShop", "ShopUnavailable", "이 저장에서는 스킬 상점을 이용할 수 없습니다."));
        else if (View.SkillShopState.Revision == 0) ShopHint->SetText(NSLOCTEXT("RunSkillShop", "LegacyStock", "이전 저장의 스킬 상품과 HP 회복은 그대로 구매할 수 있습니다. 리롤은 새 Run에서 이용할 수 있습니다."));
        else ShopHint->SetText(NSLOCTEXT("RunSkillShop", "RerollRules", "가용 스킬 최대 5개 진열 · 전체 리롤 · 비용 1 → 2 → 3G… · 구매한 스킬은 본인에게 Run 동안 적용되며 같은 스킬은 한 번만 구매할 수 있습니다."));
        const int32 OfferCount = bService ? 0 : bItemShop ? VisibleItemOfferIndices.Num() : View.SkillShopState.Offers.Num();
        while (ShopButtons.Num() < OfferCount)
        {
            UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
            Theme.StyleInset(Card);
            Card->SetPadding(FMargin(12.0f));
            ShopActions->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.0f, 4.0f));
            ShopCards.Add(Card);
            ShopTooltips.Add(CreateWidget<UShopItemTooltipWidget>(GetOwningPlayer()));
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
            if (!bVisible)
            {
                ShopCards[Index]->SetToolTipText(FText::GetEmpty());
                ShopCards[Index]->SetToolTip(nullptr);
                Button->SetToolTipText(FText::GetEmpty());
                Button->SetIsEnabled(false);
                continue;
            }
            Theme.StyleText(ShopNames[Index], true, 17);
            if (bItemShop)
            {
                const FRunItemShopOffer& Offer = View.ItemShopState.Offers[VisibleItemOfferIndices[Index]];
                const bool bAffordable = Buyer && Offer.Item.Price > 0 && Buyer->Gold >= Offer.Item.Price;
                const FText Status = ShopActionStatus(!Offer.bSold, Buyer != nullptr, bAffordable, Controller && Controller->IsShopPurchasePending(), NSLOCTEXT("RunItemShop", "Sold", "판매 완료"));
                Button->Configure(Offer.OfferId, Status);
                ShopNames[Index]->SetText(RunItemPresentation::Name(Offer.Item, View.ItemRarities));
                if (const FRunWeaponRarityRule* Rarity = RunItemPresentation::FindRarity(Offer.Item, View.ItemRarities)) ShopNames[Index]->SetColorAndOpacity(Rarity->Color);
                ShopPrices[Index]->SetText(FText::Format(NSLOCTEXT("RunItemShop", "Price", "{0}G"), FText::AsNumber(Offer.Item.Price)));
                const FText GrantedSkills = RunItemPresentation::GrantedSkills(Offer.Item, false);
                if (!GrantedSkills.IsEmpty()) ShopPrices[Index]->SetText(FText::Format(NSLOCTEXT("RunItemShop", "PriceAndSkills", "{0}\n{1}"), ShopPrices[Index]->GetText(), GrantedSkills));
                ShopPrices[Index]->SetAutoWrapText(true);
                ShopPrices[Index]->SetWrapTextAt(320.0f);
                Theme.SetItemIcon(ShopIcons[Index], Offer.Item.Tags);
                ShopIcons[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
                ShopCards[Index]->SetRenderOpacity(Offer.bSold ? 0.55f : 1.0f);
                ShopTooltips[Index]->ConfigureItem(Offer.Item, View.ItemRarities, Status);
                // Let the entire card expose the same details, including disabled purchase buttons.
                // 비활성 구매 버튼을 포함한 카드 전체에서 같은 상세 설명을 표시합니다.
                Button->SetToolTipText(FText::GetEmpty());
                if (ShopCards[Index]->GetToolTip() != ShopTooltips[Index])
                {
                    ShopCards[Index]->SetToolTipText(FText::GetEmpty());
                    ShopCards[Index]->SetToolTip(ShopTooltips[Index]);
                }
                Button->SetIsEnabled(View.ItemShopState.SchemaVersion == 1 && !Offer.bSold && bAffordable && Controller && !Controller->IsShopPurchasePending());
                continue;
            }
            const FRunSkillShopOffer& Offer = View.SkillShopState.Offers[Index];
            const bool bOwned = Buyer && Buyer->Skills.Contains(Offer.Skill);
            const bool bAffordable = Buyer && Offer.Price > 0 && Buyer->Gold >= Offer.Price;
            const FText Status = ShopActionStatus(!bOwned, Buyer != nullptr, bAffordable, Controller && Controller->IsShopPurchasePending(), NSLOCTEXT("RunSkillShop", "Owned", "보유 중"));
            Button->Configure(Offer.OfferId, Status);
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
            // Refresh renamed skill labels without changing the saved shop offer.
            // 저장된 상점 상품을 변경하지 않고 이름이 바뀐 스킬의 표시를 갱신합니다.
            ShopNames[Index]->SetText(Skill ? RunItemPresentation::SkillName(Skill) : Offer.DisplayName);
            ShopPrices[Index]->SetText(FText::Format(NSLOCTEXT("RunSkillShop", "Price", "{0}G"), FText::AsNumber(Offer.Price)));
            ShopIcons[Index]->SetVisibility(Skill && Skill->SkillIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
            if (Skill && Skill->SkillIcon) ShopIcons[Index]->SetBrushFromTexture(Skill->SkillIcon);
            ShopCards[Index]->SetRenderOpacity(bOwned ? 0.55f : 1.0f);
            ShopCards[Index]->SetToolTip(nullptr);
            const FText Description = Skill ? ProjectALocalization::SkillDescription(FName(*Skill->GetPrimaryAssetId().ToString()), Offer.Description) : Offer.Description;
            ShopCards[Index]->SetToolTipText(Description);
            Button->SetToolTipText(FText::Format(NSLOCTEXT("RunShop", "ProductActionTooltip", "{0}\n{1}"), Status, Description));
            Button->SetIsEnabled(!bOwned && bAffordable && Controller && !Controller->IsShopPurchasePending());
        }
        const bool bCanRecover = Buyer && BuyerView && Buyer->CurrentHP > 0.f && Buyer->CurrentHP < BuyerView->MaxHP;
        const bool bRecoveryAffordable = Buyer && View.SkillShopState.Recovery.Price > 0 && Buyer->Gold >= View.SkillShopState.Recovery.Price;
        const FText RecoveryStatus = ShopActionStatus(!Buyer || !BuyerView || bCanRecover, Buyer && BuyerView, bRecoveryAffordable, Controller && Controller->IsShopPurchasePending(), NSLOCTEXT("RunSkillShop", "FullHP", "HP 가득 참"));
        RecoveryButton->Configure(FRunSkillShopState::GetRecoveryOfferId(), FText::Format(NSLOCTEXT("RunSkillShop", "RecoveryProduct", "HP 전체 회복 · {0}G · {1}"), FText::AsNumber(View.SkillShopState.Recovery.Price), RecoveryStatus));
        RecoveryButton->SetToolTipText(NSLOCTEXT("RunSkillShop", "RecoveryDescription", "본인 생존 캐릭터의 HP를 즉시 최대치까지 회복합니다. HP가 가득 차 있으면 구매할 수 없습니다."));
        RecoveryButton->SetIsEnabled(View.SkillShopState.SchemaVersion == 1 && bCanRecover && bRecoveryAffordable && Controller && !Controller->IsShopPurchasePending());
        const int32 RerollPrice = bItemShop ? View.ItemShopState.RerollPrice : View.SkillShopState.RerollPrice;
        const bool bRerollAvailable = bItemShop ? View.bCanRerollItemShop : View.SkillShopState.SchemaVersion == 1 && View.SkillShopState.Revision > 0 && !View.SkillShopState.Offers.IsEmpty();
        const bool bRerollAffordable = Buyer && RerollPrice > 0 && Buyer->Gold >= RerollPrice;
        const FName RerollOfferId = bItemShop ? FRunItemShopState::GetRerollOfferId() : FRunSkillShopState::GetRerollOfferId();
        FText RerollLabel = FText::Format(NSLOCTEXT("RunShop", "Reroll", "리롤 · {0}G"), FText::AsNumber(RerollPrice));
        if (!bRerollAvailable || !Buyer || !bRerollAffordable || (Controller && Controller->IsShopPurchasePending())) RerollLabel = FText::Format(NSLOCTEXT("RunShop", "RerollStatus", "{0} · {1}"), RerollLabel, ShopActionStatus(bRerollAvailable, Buyer != nullptr, bRerollAffordable, Controller && Controller->IsShopPurchasePending(), NSLOCTEXT("RunShop", "Unavailable", "이용 불가")));
        RerollButton->Configure(RerollOfferId, RerollLabel);
        RerollButton->SetToolTipText(bItemShop ? bRerollAvailable ? NSLOCTEXT("RunItemShop", "RerollDescription", "본인 골드를 사용하여 같은 상점 조건으로 상품을 최대 5개 다시 추첨합니다. 이전 상품이 다시 나올 수 있습니다.") : NSLOCTEXT("RunItemShop", "UnavailableRerollDescription", "상점 조건에 맞는 장착 가능 상품이 부족하여 리롤할 수 없습니다. 남아 있는 상품을 구매하거나 나가기를 선택할 수 있습니다.") : !bRerollAvailable ? NSLOCTEXT("RunSkillShop", "LegacyRerollDescription", "현재 저장된 스킬 후보가 없어 리롤을 지원하지 않습니다. 남아 있는 상품과 회복 서비스를 이용할 수 있습니다.") : NSLOCTEXT("RunSkillShop", "RerollDescription", "본인 골드로 가용 스킬 최대 5개를 중복 없이 다시 추첨합니다. 이전 스킬이 다시 나올 수 있으며, 비용은 1G부터 사용마다 1G씩 증가합니다."));
        RerollButton->SetIsEnabled(bRerollAvailable && bRerollAffordable && Controller && !Controller->IsShopPurchasePending());
        if (bService)
        {
            const FGameplayTag Service = SelectedOffer->GetResolvedTag();
            const bool bConsumable = Service.MatchesTag(FRunEncounterOffer::GetConsumableShopTag());
            const bool bRevival = Service.MatchesTag(FRunEncounterOffer::GetRevivalTag());
            const FRunRecoveryState& Rules = View.RecoveryState;
            const int32 Price = bConsumable ? Rules.ConsumablePrice : bRevival ? Rules.RevivalPrice : Rules.RecoveryPrice;
            const FRunConsumableStack* Stack = Buyer ? Buyer->Consumables.FindByPredicate([&Rules](const FRunConsumableStack& Candidate) { return Candidate.ItemTag == Rules.ConsumableTag && Candidate.Skill == Rules.HealingSkill; }) : nullptr;
            const bool bEligible = Buyer && BuyerView && (bRevival ? Buyer->CurrentHP == 0.f : Buyer->CurrentHP > 0.f) && (bConsumable ? Stack && Stack->Quantity < RunRecoveryRules::MaximumQuantity : bRevival || Buyer->CurrentHP < BuyerView->MaxHP);
            const FText Product = bConsumable ? FText::Format(NSLOCTEXT("RunRecovery", "ConsumableQuantity", "회복 소모품 1개 · 보유 {0}개"), Stack ? Stack->Quantity : 0) : bRevival ? FText::Format(NSLOCTEXT("RunRecovery", "RevivalHP", "최대 HP {0}%로 부활"), FMath::RoundToInt(Rules.RevivalFraction * 100.f)) : FText::Format(NSLOCTEXT("RunRecovery", "RecoveryHP", "HP {0} 회복"), FMath::RoundToInt(Rules.RecoveryHP));
            const FText Ineligible = bConsumable ? NSLOCTEXT("RunRecovery", "QuantityLimit", "보유 한도") : bRevival ? NSLOCTEXT("RunRecovery", "LivingCharacter", "사망자만") : NSLOCTEXT("RunRecovery", "FullHP", "HP 가득 참");
            const bool bServiceAvailable = Rules.SchemaVersion == 1 && Rules.Revision > 0;
            const FText Status = ShopActionStatus(bServiceAvailable && (!Buyer || !BuyerView || bEligible), Buyer && BuyerView, Buyer && Buyer->Gold >= Price, Controller && Controller->IsShopPurchasePending(), bServiceAvailable ? Ineligible : NSLOCTEXT("RunShop", "Unavailable", "이용 불가"));
            RecoveryButton->Configure(Service.GetTagName(), FText::Format(NSLOCTEXT("RunRecovery", "ProductStatus", "{0} · {1}G · {2}"), Product, FText::AsNumber(Price), Status));
            const FText Hint = bConsumable ? NSLOCTEXT("RunRecovery", "ConsumableHint", "본인 생존 캐릭터가 구매합니다. 전투에서 본인에게 AP 1로 사용하며 실제 회복이 발동한 경우에만 1개를 소모합니다.") : bRevival ? NSLOCTEXT("RunRecovery", "RevivalHint", "본인이 직접 조작하는 사망 캐릭터를 부활시킵니다. AI 동료와 다른 참가자의 캐릭터는 구매할 수 없습니다.") : NSLOCTEXT("RunRecovery", "RecoveryHint", "본인 생존 캐릭터의 HP를 회복합니다. 이미 최대 HP이면 구매할 수 없습니다.");
            RecoveryButton->SetToolTipText(Hint);
            ShopHint->SetText(Hint);
            RecoveryButton->SetIsEnabled(Rules.SchemaVersion == 1 && Rules.Revision > 0 && bEligible && Buyer->Gold >= Price && Controller && !Controller->IsShopPurchasePending());
        }
    }
    for (int32 Index = 0; Index < ChoiceButtons.Num(); ++Index)
    {
        UGameplayActionButton* Button = ChoiceButtons[Index];
        const bool bVisible = !bDungeonChoice && View.Phase == ERunPhase::EncounterChoice && View.EncounterProgress.Offers.IsValidIndex(Index);
        Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetIsEnabled(bVisible && bAllowRunCommands);
        if (bVisible)
        {
            const FRunEncounterOffer& Offer = View.EncounterProgress.Offers[Index];
            Button->Configure(Offer.EncounterId, ProjectALocalization::Content(TEXT("Encounter.") + Offer.EncounterId.ToString() + TEXT(".Name"), Offer.GetDisplayName()));
            Button->SetToolTipText(EncounterShopDescription(Offer));
        }
    }
    for (int32 Index = 0; Index < DungeonChoiceButtons.Num(); ++Index)
    {
        UGameplayActionButton* Button = DungeonChoiceButtons[Index];
        const bool bVisible = bDungeonChoice && View.EncounterProgress.Offers.IsValidIndex(Index);
        // Hidden slots retain their direction instead of shifting the remaining corridor cards.
        // 후보가 없는 칸도 공간을 유지하여 남은 통로 카드의 방향이 바뀌지 않게 합니다.
        Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
        Button->SetIsEnabled(bVisible && bAllowRunCommands);
        if (!bVisible) continue;
        const FRunEncounterOffer& Offer = View.EncounterProgress.Offers[Index];
        const FText Direction = EncounterDungeonLayout::GetDirectionLabel(Index);
        const FText OfferName = ProjectALocalization::Content(TEXT("Encounter.") + Offer.EncounterId.ToString() + TEXT(".Name"), Offer.GetDisplayName());
        Button->Configure(Offer.EncounterId, FText::Format(NSLOCTEXT("RunEncounter", "DungeonDirectionOffer", "{0}\n{1}"), Direction, OfferName));
        Button->SetToolTipText(FText::Format(NSLOCTEXT("RunEncounter", "DungeonDirectionDescription", "{0} · {1}\n{2}"), Direction, OfferName, EncounterShopDescription(Offer)));
        if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
        {
            Label->SetWrapTextAt(300.0f);
            Theme.StyleText(Label, true, 20);
        }
    }
    LeaveButton->SetVisibility(View.Phase == ERunPhase::Shop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    LeaveButton->SetIsEnabled(View.Phase == ERunPhase::Shop && bAllowRunCommands);
    FText DisplayMessage = View.FlowMessage;
    if (bInShop && Controller && !Controller->GetShopPurchaseMessage().IsEmpty()) DisplayMessage = DisplayMessage.IsEmpty() ? Controller->GetShopPurchaseMessage() : FText::Format(NSLOCTEXT("RunSkillShop", "FlowAndPurchaseMessage", "{0}\n{1}"), DisplayMessage, Controller->GetShopPurchaseMessage());
    if (bInShop && Controller && Controller->IsShopPurchasePending()) DisplayMessage = NSLOCTEXT("RunShop", "Pending", "상점 요청을 처리하는 중입니다.");
    if (DisplayMessage.IsEmpty() && !bAllowRunCommands)
    {
        if (bService) DisplayMessage = NSLOCTEXT("RunRecovery", "HostLeaves", "본인 캐릭터의 회복 서비스를 구매할 수 있습니다. 인카운터 나가기는 Host가 결정합니다.");
        else if (bItemShop) DisplayMessage = NSLOCTEXT("RunItemShop", "HostLeaves", "본인 골드로 아이템 구매와 리롤을 할 수 있습니다. 상점 나가기는 Host가 결정합니다.");
        else if (bInShop && View.SkillShopState.Revision > 0) DisplayMessage = NSLOCTEXT("RunSkillShop", "HostShopActions", "본인 골드로 스킬 구매, HP 회복과 리롤을 할 수 있습니다. 상점 나가기는 Host가 결정합니다.");
        else DisplayMessage = bInShop ? NSLOCTEXT("RunSkillShop", "HostLeaves", "본인 캐릭터의 스킬과 HP 회복을 구매할 수 있습니다. 상점 나가기는 Host가 결정합니다.") : NSLOCTEXT("RunEncounter", "HostOnly", "Host의 진행을 기다리는 중입니다.");
    }
    Message->SetText(DisplayMessage);
    Message->SetVisibility(DisplayMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    DungeonChoiceMessage->SetText(DisplayMessage);
    DungeonChoiceMessage->SetVisibility(DisplayMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (View.Phase == ERunPhase::EncounterChoice)
    {
        Title->SetText(bDungeonChoice ? NSLOCTEXT("RunEncounter", "DungeonChoose", "갈림길 · 이동할 통로 선택") : NSLOCTEXT("RunEncounter", "Choose", "인카운터 선택"));
    }
    else if (View.Phase == ERunPhase::Shop)
    {
        const FRunEncounterOffer* Selected = View.EncounterProgress.FindSelectedOffer();
        Title->SetText(Selected ? ProjectALocalization::Content(TEXT("Encounter.") + Selected->EncounterId.ToString() + TEXT(".Name"), Selected->GetDisplayName()) : NSLOCTEXT("RunEncounter", "Shop", "상점"));
    }
    if (View.bTargetRun && (View.Phase == ERunPhase::EncounterChoice || bInShop)) Title->SetText(FText::Format(NSLOCTEXT("RunEncounter", "TargetProgressTitle", "{0} · {1}/80 완료"), Title->GetText(), FText::AsNumber(View.TargetCompletedSteps)));
    DungeonChoiceTitle->SetText(Title->GetText());
}

void URunEncounterWidget::FocusInventory()
{
    // Focus the visible bag without opening an overlay or issuing a gameplay request.
    // 오버레이를 열거나 게임 요청을 보내지 않고 표시 중인 가방으로 포커스만 옮깁니다.
    if (!IsActivated() || !GetIsEnabled() || !InventoryPanel || !InventorySize || !InventorySize->IsVisible()) return;
    InventoryPanel->FocusInventory();
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
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>()) Controller->RequestPurchaseShopOffer(BuyerCharacterId, OfferId, ShopRevision);
}

void URunEncounterWidget::HandleInventory(FName)
{
    if (AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>()) Controller->RequestToggleInventory();
}
