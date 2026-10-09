#include "UI/Gameplay/CharacterInventoryPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemSaleRules.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "UI/Gameplay/EquipmentDragDropOperation.h"
#include "UI/Gameplay/EquipmentItemSlotWidget.h"
#include "UI/Gameplay/GameplayActionButton.h"
#include "UI/Gameplay/RunItemPresentation.h"
#include "UI/Theme/DemonicUITheme.h"

namespace
{
    FInventoryDisplayCategory MakeCategory(const FText& Label, const TCHAR* IconTag, const FGameplayTagQuery& Query = FGameplayTagQuery(), bool bSkills = false)
    {
        FInventoryDisplayCategory Category;
        Category.Label = Label;
        if (IconTag) Category.IconTags.AddTag(FGameplayTag::RequestGameplayTag(FName(IconTag)));
        Category.Query = Query;
        Category.bSkills = bSkills;
        return Category;
    }

    FText SlotName(FGameplayTag Slot)
    {
        const TArray<FGameplayTag> Slots = URunEquipmentCatalog::GetSlotTags();
        const TArray<FText> Labels = { NSLOCTEXT("Equipment", "MainHand", "주 무기"), NSLOCTEXT("Equipment", "OffHand", "보조 무기"), NSLOCTEXT("Equipment", "Head", "투구"), NSLOCTEXT("Equipment", "Hands", "장갑"), NSLOCTEXT("Equipment", "Feet", "신발"), NSLOCTEXT("Equipment", "Body", "갑옷"), NSLOCTEXT("Equipment", "Neck", "목걸이"), NSLOCTEXT("Equipment", "RingOne", "반지 1"), NSLOCTEXT("Equipment", "RingTwo", "반지 2") };
        const int32 Index = Slots.IndexOfByKey(Slot);
        return Labels.IsValidIndex(Index) ? Labels[Index] : FText::GetEmpty();
    }
}

void UInventoryCategoryButton::InitializeCategory(int32 InCategoryIndex)
{
    CategoryIndex = InCategoryIndex;
    OnClicked.AddUniqueDynamic(this, &UInventoryCategoryButton::HandleClicked);
}

void UInventoryCategoryButton::HandleClicked()
{
    CategorySelected.ExecuteIfBound(CategoryIndex);
}

UTextBlock* UCharacterInventoryPanel::AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 22, FontSize);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    return Label;
}

void UCharacterInventoryPanel::InitializeCategories()
{
    const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon"));
    const FGameplayTag ShieldTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield"));
    const FGameplayTag ArrowTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.ArrowBolt"));
    const FGameplayTag BulletTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Bullet"));
    const FGameplayTag OtherTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Other"));
    FGameplayTagQueryExpression Weapon;
    Weapon.AllExprMatch().AddExpr(FGameplayTagQueryExpression().AllTagsMatch().AddTag(WeaponTag)).AddExpr(FGameplayTagQueryExpression().NoTagsMatch().AddTag(ShieldTag).AddTag(ArrowTag).AddTag(BulletTag).AddTag(OtherTag));
    FGameplayTagQueryExpression Shield;
    Shield.AnyTagsMatch().AddTag(ShieldTag);
    FGameplayTagQueryExpression Ammo;
    Ammo.AllExprMatch().AddExpr(FGameplayTagQueryExpression().AnyTagsMatch().AddTag(ArrowTag).AddTag(BulletTag)).AddExpr(FGameplayTagQueryExpression().NoTagsMatch().AddTag(ShieldTag));
    FGameplayTagQueryExpression Other;
    Other.NoExprMatch().AddExpr(Weapon).AddExpr(Shield).AddExpr(Ammo);
    // The fallback query also keeps unknown future item tags visible instead of dropping their copies.
    // 기본 분류 쿼리는 향후 알 수 없는 아이템 태그의 사본도 누락하지 않고 표시합니다.
    Categories = { MakeCategory(NSLOCTEXT("Inventory", "AllCategory", "전체"), nullptr), MakeCategory(NSLOCTEXT("Inventory", "WeaponsCategory", "무기"), TEXT("Item.Weapon.Sword"), FGameplayTagQuery::BuildQuery(Weapon)), MakeCategory(NSLOCTEXT("Inventory", "ShieldsCategory", "방패"), TEXT("Item.Weapon.Shield"), FGameplayTagQuery::BuildQuery(Shield)), MakeCategory(NSLOCTEXT("Inventory", "AmmoCategory", "탄약"), TEXT("Item.Weapon.ArrowBolt"), FGameplayTagQuery::BuildQuery(Ammo)), MakeCategory(NSLOCTEXT("Inventory", "OtherCategory", "기타"), TEXT("Item.Weapon.Other"), FGameplayTagQuery::BuildQuery(Other)), MakeCategory(NSLOCTEXT("Inventory", "SkillsCategory", "스킬"), TEXT("Item.Weapon.Spellbook"), FGameplayTagQuery(), true) };
}

void UCharacterInventoryPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::Visible);
    InitializeCategories();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CharacterInventoryPanel"));
    Theme.StylePanel(Panel);
    Panel->SetPadding(FMargin(18.0f, 24.0f));
    WidgetTree->RootWidget = Panel;
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);
    UBorder* Header = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleSectionHeader(Header);
    Header->SetPadding(FMargin(8.0f));
    Content->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("Inventory", "Title", "인벤토리"));
    Title->SetJustification(ETextJustify::Center);
    Theme.StyleText(Title, true, 26);
    Header->SetContent(Title);
    GoldText = AddText(Content, FText::GetEmpty(), 20, 6.0f);
    StatusText = AddText(Content, FText::GetEmpty(), 15, 4.0f);
    ItemCountText = AddText(Content, FText::GetEmpty(), 14, 8.0f);
    Theme.AddDivider(WidgetTree, Content);

    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryCategoryTabs"));
    Content->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
    for (int32 Index = 0; Index < Categories.Num(); ++Index)
    {
        UInventoryCategoryButton* Button = WidgetTree->ConstructWidget<UInventoryCategoryButton>();
        Button->InitializeCategory(Index);
        Button->CategorySelected.BindUObject(this, &UCharacterInventoryPanel::SelectCategory);
        Button->SetToolTipText(Categories[Index].Label);
        UHorizontalBoxSlot* TabSlot = Tabs->AddChildToHorizontalBox(Button);
        TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TabSlot->SetPadding(FMargin(1.0f, 0.0f));
        UVerticalBox* TabContent = WidgetTree->ConstructWidget<UVerticalBox>();
        CastChecked<UButtonSlot>(Button->AddChild(TabContent))->SetPadding(FMargin(3.0f, 5.0f));
        UHorizontalBox* CountRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        TabContent->AddChildToVerticalBox(CountRow)->SetHorizontalAlignment(HAlign_Center);
        USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
        IconSize->SetWidthOverride(20.0f);
        IconSize->SetHeightOverride(20.0f);
        CountRow->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 0.0f));
        UImage* Icon = WidgetTree->ConstructWidget<UImage>();
        Theme.SetItemIcon(Icon, Categories[Index].IconTags);
        IconSize->SetContent(Icon);
        UTextBlock* Count = WidgetTree->ConstructWidget<UTextBlock>();
        Theme.StyleText(Count, false, 13);
        CountRow->AddChildToHorizontalBox(Count)->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = AddText(TabContent, Categories[Index].Label, 12, 0.0f);
        Label->SetJustification(ETextJustify::Center);
        Label->SetAutoWrapText(false);
        CategoryButtons.Add(Button);
        CategoryCounts.Add(Count);
    }

    ListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryListScroll"));
    ListScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    Content->AddChildToVerticalBox(ListScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* ListContent = WidgetTree->ConstructWidget<UVerticalBox>();
    ListScroll->AddChild(ListContent);
    EmptyItemsText = AddText(ListContent, FText::GetEmpty(), 16, 12.0f);
    ItemList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryItems"));
    ListContent->AddChildToVerticalBox(ItemList);
    SkillStatusText = AddText(ListContent, FText::GetEmpty(), 15, 8.0f);
    SkillList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventorySkills"));
    ListContent->AddChildToVerticalBox(SkillList);

    DetailsPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SelectedInventoryItem"));
    Theme.StyleInset(DetailsPanel);
    DetailsPanel->SetPadding(FMargin(10.0f));
    Content->AddChildToVerticalBox(DetailsPanel)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
    USizeBox* DetailsSize = WidgetTree->ConstructWidget<USizeBox>();
    DetailsSize->SetHeightOverride(220.0f);
    DetailsPanel->SetContent(DetailsSize);
    UVerticalBox* DetailsLayout = WidgetTree->ConstructWidget<UVerticalBox>();
    DetailsSize->SetContent(DetailsLayout);
    DetailsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryDetailsScroll"));
    DetailsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    DetailsLayout->AddChildToVerticalBox(DetailsScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* DetailsContent = WidgetTree->ConstructWidget<UVerticalBox>();
    DetailsScroll->AddChild(DetailsContent);
    UHorizontalBox* DetailRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    DetailsContent->AddChildToVerticalBox(DetailRow);
    USizeBox* DetailsIconSize = WidgetTree->ConstructWidget<USizeBox>();
    DetailsIconSize->SetWidthOverride(48.0f);
    DetailsIconSize->SetHeightOverride(48.0f);
    DetailRow->AddChildToHorizontalBox(DetailsIconSize)->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
    DetailsIcon = WidgetTree->ConstructWidget<UImage>();
    DetailsIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    DetailsIconSize->SetContent(DetailsIcon);
    UVerticalBox* Description = WidgetTree->ConstructWidget<UVerticalBox>();
    DetailRow->AddChildToHorizontalBox(Description)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    DetailsName = AddText(Description, FText::GetEmpty(), 20, 6.0f);
    DetailsSummary = AddText(Description, FText::GetEmpty(), 14, 6.0f);
    DetailsHint = AddText(DetailsContent, FText::GetEmpty(), 14, 0.0f);
    DetailsSkills = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryItemSkillDetails"));
    DetailsContent->AddChildToVerticalBox(DetailsSkills)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
    // Keep confirmation controls visible while long skill descriptions scroll above them.
    // 긴 스킬 설명만 스크롤하고 판매 확인 버튼은 아래에 고정합니다.
    SaleControls = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventorySaleControls"));
    DetailsLayout->AddChildToVerticalBox(SaleControls)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    SaleSummary = AddText(SaleControls, FText::GetEmpty(), 15, 3.0f);
    SaleSummary->SetAutoWrapText(false);
    SaleSummary->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    SaleHint = AddText(SaleControls, FText::GetEmpty(), 13, 5.0f);
    UHorizontalBox* SaleActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    SaleControls->AddChildToVerticalBox(SaleActions);
    SellButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_SellInventoryItem"));
    SellButton->OnActionRequested.AddUObject(this, &UCharacterInventoryPanel::HandleSaleAction);
    SaleActions->AddChildToHorizontalBox(SellButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CancelSaleButton = WidgetTree->ConstructWidget<UGameplayActionButton>(UGameplayActionButton::StaticClass(), TEXT("Button_CancelInventorySale"));
    CancelSaleButton->OnActionRequested.AddUObject(this, &UCharacterInventoryPanel::HandleSaleAction);
    UHorizontalBoxSlot* CancelSlot = SaleActions->AddChildToHorizontalBox(CancelSaleButton);
    CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CancelSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
    RefreshSaleControls();
    Theme.ApplyControls(WidgetTree);
}

void UCharacterInventoryPanel::AddItem(const FRunItemDefinition& Item, int32 ItemIndex)
{
    UEquipmentItemSlotWidget* Row = CreateWidget<UEquipmentItemSlotWidget>(GetOwningPlayer());
    Row->UseListPresentation();
    Row->ItemSelected.BindUObject(this, &UCharacterInventoryPanel::SelectItem);
    Row->CanAcceptDrop.BindUObject(this, &UCharacterInventoryPanel::CanAcceptDrop);
    Row->ReceiveDrop.BindUObject(this, &UCharacterInventoryPanel::HandleDrop);
    Row->RefreshSlot(DisplayedMember.CharacterId, DisplayedMember.Equipment.Revision, ItemIndex, &Item, FGameplayTag(), NAME_None, FText::GetEmpty(), bCanChangeEquipment, DisplayedRarities);
    ItemList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
    ItemRows.Add(Row);
    VisibleItemIndices.Add(ItemIndex);
}

void UCharacterInventoryPanel::AddSkill(const USkillDefinitionDataAsset* Skill)
{
    if (!Skill)
    {
        AddText(SkillList, NSLOCTEXT("Inventory", "MissingSkill", "스킬 정보를 불러올 수 없습니다."), 16, 8.0f);
        return;
    }
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(Card);
    Card->SetPadding(FMargin(8.0f, 6.0f));
    SkillList->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Content);
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Row);
    USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
    IconSize->SetWidthOverride(24.0f);
    IconSize->SetHeightOverride(24.0f);
    Row->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
    UImage* Icon = WidgetTree->ConstructWidget<UImage>();
    if (Skill->SkillIcon) Icon->SetBrushFromTexture(Skill->SkillIcon);
    else Theme.SetItemIcon(Icon, Categories.Last().IconTags);
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    IconSize->SetContent(Icon);
    const FText Name = RunItemPresentation::SkillName(Skill);
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Name);
    Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Theme.StyleText(Label, false, 16);
    UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Label);
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    NameSlot->SetVerticalAlignment(VAlign_Center);
    TArray<FText> Sources;
    TSet<int32> SourceItems;
    FCombatRoundSkill DisplayedSkill;
    FText Error;
    const bool bResolvedSkill = Skill->ResolveRoundSkill(DisplayedSkill, Error);
    RunItemPresentation::FItemSkillDetails Detail;
    Detail.Name = Name;
    Detail.Description = RunItemPresentation::SkillDescription(Skill);
    Detail.Stats = bResolvedSkill ? FText::Format(NSLOCTEXT("Inventory", "LegacySkillStats", "위력 {0} · AP {1} / SAP {2} · 선딜 {3}초"), FText::AsNumber(DisplayedSkill.Power), FText::AsNumber(DisplayedSkill.ActionPointCost), FText::AsNumber(DisplayedSkill.SubActionPointCost), FText::AsNumber(DisplayedSkill.WindupSeconds)) : NSLOCTEXT("Inventory", "MissingSkillStats", "스킬 수치 정보를 확인할 수 없습니다.");
    bool bHasSourceDetails = false;
    for (const FRunEquipmentSlot& EquipmentSlot : DisplayedMember.Equipment.Slots)
    {
        if (!DisplayedMember.Items.IsValidIndex(EquipmentSlot.ItemIndex) || SourceItems.Contains(EquipmentSlot.ItemIndex)) continue;
        const FRunItemDefinition& Item = DisplayedMember.Items[EquipmentSlot.ItemIndex];
        if (Item.GenerationVersion == 0) continue;
        const int32 GrantedSkillIndex = Item.GrantedSkills.IndexOfByPredicate([Skill, bResolvedSkill, &DisplayedSkill](const FSoftObjectPath& Path)
        {
            const USkillDefinitionDataAsset* GrantedSkill = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
            if (GrantedSkill == Skill) return true;
            FCombatRoundSkill Granted;
            FText GrantedError;
            return bResolvedSkill && GrantedSkill && GrantedSkill->ResolveRoundSkill(Granted, GrantedError) && Granted.SkillId == DisplayedSkill.SkillId;
        });
        if (GrantedSkillIndex == INDEX_NONE) continue;
        if (!bHasSourceDetails)
        {
            // Equipped copies carry the same frozen tuning used by combat; never replace it with asset description numbers.
            // 장착 사본은 전투와 같은 고정 수치를 보유하므로 원본 설명의 숫자로 대체하지 않습니다.
            const TArray<RunItemPresentation::FItemSkillDetails> ItemDetails = RunItemPresentation::SkillDetails(Item);
            if (ItemDetails.IsValidIndex(GrantedSkillIndex)) Detail = ItemDetails[GrantedSkillIndex];
            else
            {
                Detail.Description = NSLOCTEXT("Inventory", "MissingSavedSkillDetails", "저장된 스킬 설명을 확인할 수 없습니다.");
                Detail.Stats = NSLOCTEXT("Inventory", "MissingSavedSkillStats", "저장된 스킬 등급·수치 정보를 확인할 수 없습니다.");
            }
            bHasSourceDetails = true;
        }
        SourceItems.Add(EquipmentSlot.ItemIndex);
        Sources.Add(FText::Format(NSLOCTEXT("Inventory", "EquippedSkillSource", "{0} ({1})"), RunItemPresentation::Name(Item, DisplayedRarities), SlotName(EquipmentSlot.SlotTag)));
    }
    Label->SetText(Detail.Name);
    Label->SetColorAndOpacity(Detail.Color);
    AddText(Content, Detail.Stats, 14, 4.0f)->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (!Detail.Description.IsEmpty()) AddText(Content, Detail.Description, 14, 4.0f)->SetVisibility(ESlateVisibility::HitTestInvisible);
    const FText SourceText = Sources.IsEmpty() ? FText::GetEmpty() : FText::Format(NSLOCTEXT("Inventory", "EquippedSkillSources", "장착 무기: {0}"), FText::Join(FText::FromString(TEXT(" · ")), Sources));
    if (!SourceText.IsEmpty()) AddText(Content, SourceText, 13, 0.0f)->SetVisibility(ESlateVisibility::HitTestInvisible);
    const FText Tooltip = FText::Format(NSLOCTEXT("Inventory", "ResolvedSkillTooltip", "{0}\n{1}\n{2}"), Detail.Name, Detail.Stats, Detail.Description);
    Card->SetToolTipText(SourceText.IsEmpty() ? Tooltip : FText::Format(NSLOCTEXT("Inventory", "SkillSourceTooltip", "{0}\n{1}"), Tooltip, SourceText));
}

void UCharacterInventoryPanel::ResolveDisplayedSkills()
{
    DisplayedSkills.Reset();
    bSkillsUnavailable = false;
    SkillStatusText->SetText(FText::GetEmpty());
    if (!DisplayedMember.bCreated) return;
    if (DisplayedMember.bHasSkillLoadout)
    {
        for (const FSoftObjectPath& Path : DisplayedMember.Skills) DisplayedSkills.Add(Cast<USkillDefinitionDataAsset>(Path.TryLoad()));
        return;
    }
    // Only the authoritative Run owns the legacy catalog; clients cannot substitute local defaults.
    // 권위 Run만 이전 저장의 목록을 보유하며 클라이언트는 로컬 기본값으로 대체할 수 없습니다.
    const APlayerController* Controller = GetOwningPlayer();
    const UGameInstance* GameInstance = GetGameInstance();
    const URunStateSubsystem* Run = Controller && Controller->HasAuthority() && GameInstance ? GameInstance->GetSubsystem<URunStateSubsystem>() : nullptr;
    const UPartyDefinitionDataAsset* Catalog = Run ? Run->PartyDefinition.Get() : nullptr;
    FText Error;
    if (!Catalog || !Catalog->ResolveMemberSkills(DisplayedMember, DisplayedSkills, Error))
    {
        DisplayedSkills.Reset();
        bSkillsUnavailable = true;
        SkillStatusText->SetText(NSLOCTEXT("Inventory", "LegacyUnavailable", "이전 저장의 장착 스킬 정보를 불러올 수 없습니다."));
        return;
    }
    SkillStatusText->SetText(NSLOCTEXT("Inventory", "LegacySkills", "이전 저장의 직업 기본 장착 스킬입니다."));
}

void UCharacterInventoryPanel::RefreshInventory(const FGameplayViewState& View, FGuid CharacterId)
{
    if (!ItemList || !SkillList) return;
    // A refreshed view invalidates local confirmation, even when its item index happens to stay the same.
    // 표시 상태가 갱신되면 아이템 인덱스가 같아도 로컬 판매 확인을 취소합니다.
    bConfirmingSale = false;
    SaleCommand = FRunItemSaleCommand();
    const FRunPartyMember* Member = CharacterId.IsValid() ? View.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId && Candidate.bCreated; }) : nullptr;
    // Preserve a selection only when the same character still owns the same indexed copy and definition.
    // 같은 캐릭터가 동일 인덱스의 사본과 정의를 계속 보유할 때만 선택을 유지합니다.
    const FRunItemDefinition* Candidate = Member && Member->Items.IsValidIndex(SelectedItemIndex) ? &Member->Items[SelectedItemIndex] : nullptr;
    if (!Candidate || Member->CharacterId != DisplayedMember.CharacterId || Candidate->Asset != SelectedItem.Asset || !Candidate->DisplayName.EqualTo(SelectedItem.DisplayName) || Candidate->Tags != SelectedItem.Tags || Candidate->Price != SelectedItem.Price || Candidate->ItemInstanceId != SelectedItem.ItemInstanceId)
    {
        if (SelectedItemIndex != INDEX_NONE) bSuppressAutomaticSelection = true;
        SelectedItemIndex = INDEX_NONE;
        DetailsScroll->ScrollToStart();
    }
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    DisplayedMember = Member ? *Member : FRunPartyMember();
    DisplayedRarities = View.ItemRarities;
    DisplayedEncounterId = View.EncounterProgress.SelectedEncounterId;
    DisplayedShopRevision = View.ItemShopState.Revision;
    bCanChangeEquipment = Member && Controller && Controller->CanChangeEquipment(View, CharacterId);
    bCanSellItems = Member && Controller && Controller->CanSellInventoryItems(View, CharacterId);
    GoldText->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ItemCountText->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StatusText->SetVisibility(ESlateVisibility::Visible);
    GoldText->SetText(Member ? FText::Format(NSLOCTEXT("Inventory", "Gold", "보유 골드 {0}G"), FText::AsNumber(Member->Gold)) : FText::GetEmpty());
    StatusText->SetText(!Member ? NSLOCTEXT("Inventory", "NoInventory", "직접 조작 캐릭터가 없어 표시할 인벤토리가 없습니다.") : Member->CurrentHP == 0.0f ? NSLOCTEXT("Inventory", "DeadInventory", "사망 · 보유 아이템과 스킬 보기") : bCanChangeEquipment ? NSLOCTEXT("Inventory", "ListDragHint", "선택하여 상세 보기 · 슬롯으로 끌어 장착") : NSLOCTEXT("Inventory", "ListReadOnlyHint", "선택하여 상세 보기 · 장비 변경은 상점에서"));
    if (Member && Controller && Controller->IsItemSalePending()) StatusText->SetText(NSLOCTEXT("InventorySale", "Pending", "아이템 판매를 저장하고 있습니다."));
    else if (Member && Controller && Controller->IsEquipmentChangePending()) StatusText->SetText(NSLOCTEXT("Equipment", "Pending", "장비 변경을 저장하고 있습니다."));
    else if (Member && Controller && !Controller->GetItemSaleMessage().IsEmpty()) StatusText->SetText(Controller->GetItemSaleMessage());
    else if (Member && Controller && !Controller->GetEquipmentMessage().IsEmpty()) StatusText->SetText(Controller->GetEquipmentMessage());
    ResolveDisplayedSkills();
    RebuildList();
}

void UCharacterInventoryPanel::FocusInventory()
{
    // Move keyboard focus without selecting a different copy or confirming a sale.
    // 다른 사본을 선택하거나 판매를 확정하지 않고 키보드 포커스만 이동합니다.
    const int32 SelectedRow = VisibleItemIndices.IndexOfByKey(SelectedItemIndex);
    const int32 FocusRow = ItemRows.IsValidIndex(SelectedRow) ? SelectedRow : 0;
    if (ItemRows.IsValidIndex(FocusRow)) ItemRows[FocusRow]->SetFocus();
    else if (CategoryButtons.IsValidIndex(SelectedCategoryIndex)) CategoryButtons[SelectedCategoryIndex]->SetFocus();
}

void UCharacterInventoryPanel::SelectCategory(int32 CategoryIndex)
{
    if (!Categories.IsValidIndex(CategoryIndex) || SelectedCategoryIndex == CategoryIndex) return;
    bConfirmingSale = false;
    SaleCommand = FRunItemSaleCommand();
    SelectedCategoryIndex = CategoryIndex;
    ListScroll->ScrollToStart();
    DetailsScroll->ScrollToStart();
    RebuildList();
}

void UCharacterInventoryPanel::RebuildList()
{
    ItemList->ClearChildren();
    SkillList->ClearChildren();
    ItemRows.Reset();
    VisibleItemIndices.Reset();
    const bool bSkills = Categories[SelectedCategoryIndex].bSkills;
    ItemList->SetVisibility(bSkills ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    SkillList->SetVisibility(bSkills ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    SkillStatusText->SetVisibility(bSkills && !SkillStatusText->GetText().IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    TArray<int32> Counts;
    Counts.Init(0, Categories.Num());
    int32 BagCount = 0;
    for (int32 ItemIndex = 0; ItemIndex < DisplayedMember.Items.Num(); ++ItemIndex)
    {
        if (RunEquipmentRules::IsItemEquipped(DisplayedMember, ItemIndex)) continue;
        const FRunItemDefinition& Item = DisplayedMember.Items[ItemIndex];
        ++BagCount;
        for (int32 CategoryIndex = 0; CategoryIndex < Categories.Num(); ++CategoryIndex)
        {
            const FInventoryDisplayCategory& Category = Categories[CategoryIndex];
            if (Category.bSkills || (!Category.Query.IsEmpty() && !Category.Query.Matches(Item.Tags))) continue;
            ++Counts[CategoryIndex];
            if (CategoryIndex == SelectedCategoryIndex) AddItem(Item, ItemIndex);
        }
    }
    for (int32 Index = 0; Index < Categories.Num(); ++Index)
    {
        CategoryCounts[Index]->SetText(Categories[Index].bSkills ? bSkillsUnavailable ? NSLOCTEXT("Inventory", "UnknownSkillCount", "?") : FText::AsNumber(DisplayedSkills.Num()) : FText::AsNumber(Counts[Index]));
        CategoryButtons[Index]->SetIsEnabled(DisplayedMember.bCreated);
        UDemonicUITheme::Get().StyleButton(CategoryButtons[Index], Index == SelectedCategoryIndex);
    }
    ItemCountText->SetText(FText::Format(NSLOCTEXT("Inventory", "EquipmentItemCounts", "보관 {0}개 · 장착 {1}개 · 전체 {2}개"), FText::AsNumber(BagCount), FText::AsNumber(DisplayedMember.Items.Num() - BagCount), FText::AsNumber(DisplayedMember.Items.Num())));
    if (bSkills && DisplayedMember.bCreated)
    {
        for (const USkillDefinitionDataAsset* Skill : DisplayedSkills) AddSkill(Skill);
        if (DisplayedSkills.IsEmpty() && !bSkillsUnavailable) AddText(SkillList, NSLOCTEXT("Inventory", "EmptySkills", "보유한 스킬이 없습니다."), 16);
    }
    const bool bEmptyItems = !bSkills && VisibleItemIndices.IsEmpty() && DisplayedMember.bCreated;
    const FText EmptyBag = bCanChangeEquipment ? NSLOCTEXT("Inventory", "EmptyBag", "가방이 비어 있습니다.\n장착 아이템을 이곳으로 끌어 해제할 수 있습니다.") : NSLOCTEXT("Inventory", "EmptyReadOnlyBag", "가방이 비어 있습니다.\n장착 중인 아이템은 현재 장비에서 확인하세요.");
    EmptyItemsText->SetText(BagCount == 0 ? EmptyBag : FText::Format(NSLOCTEXT("Inventory", "EmptyCategory", "{0} 분류에 보관된 아이템이 없습니다.\n전체 탭에서 다른 아이템을 확인하세요."), Categories[SelectedCategoryIndex].Label));
    EmptyItemsText->SetVisibility(bEmptyItems ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (!VisibleItemIndices.Contains(SelectedItemIndex)) SelectedItemIndex = bSuppressAutomaticSelection || VisibleItemIndices.IsEmpty() ? INDEX_NONE : VisibleItemIndices[0];
    RefreshSelectedItem();
}

void UCharacterInventoryPanel::SelectItem(int32 ItemIndex)
{
    if (!VisibleItemIndices.Contains(ItemIndex) || !DisplayedMember.Items.IsValidIndex(ItemIndex)) return;
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (Controller && Controller->IsShopPurchasePending()) return;
    bConfirmingSale = false;
    SaleCommand = FRunItemSaleCommand();
    bSuppressAutomaticSelection = false;
    SelectedItemIndex = ItemIndex;
    DetailsScroll->ScrollToStart();
    // Change only highlights and details so mouse selection does not replace a pending drag source.
    // 마우스 선택이 대기 중인 드래그 원본을 교체하지 않도록 강조와 상세만 변경합니다.
    RefreshSelectedItem();
}

void UCharacterInventoryPanel::RefreshSelectedItem()
{
    for (int32 Index = 0; Index < ItemRows.Num(); ++Index) ItemRows[Index]->SetSelected(VisibleItemIndices[Index] == SelectedItemIndex);
    const bool bHasItem = DisplayedMember.Items.IsValidIndex(SelectedItemIndex) && VisibleItemIndices.Contains(SelectedItemIndex);
    DetailsPanel->SetVisibility(bHasItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    SelectedItem = bHasItem ? DisplayedMember.Items[SelectedItemIndex] : FRunItemDefinition();
    DetailsSkills->ClearChildren();
    RefreshSaleControls();
    if (!bHasItem) return;
    UDemonicUITheme::Get().SetItemIcon(DetailsIcon, SelectedItem.Tags);
    DetailsName->SetText(RunItemPresentation::Name(SelectedItem, DisplayedRarities));
    UDemonicUITheme::Get().StyleText(DetailsName, false, 20);
    if (const FRunWeaponRarityRule* Rarity = RunItemPresentation::FindRarity(SelectedItem, DisplayedRarities)) DetailsName->SetColorAndOpacity(Rarity->Color);
    FText CategoryLabel = Categories[0].Label;
    for (const FInventoryDisplayCategory& Category : Categories)
    {
        if (!Category.bSkills && !Category.Query.IsEmpty() && Category.Query.Matches(SelectedItem.Tags))
        {
            CategoryLabel = Category.Label;
            break;
        }
    }
    DetailsSummary->SetText(FText::Format(NSLOCTEXT("Inventory", "ItemDetails", "{0} · 보관 1개\n카탈로그 기준 가격 {1}G"), CategoryLabel, FText::AsNumber(SelectedItem.Price)));
    const TArray<RunItemPresentation::FItemSkillDetails> SkillDetails = RunItemPresentation::SkillDetails(SelectedItem);
    if (!SkillDetails.IsEmpty())
    {
        AddText(DetailsSkills, NSLOCTEXT("Inventory", "GrantedSkillDetailsTitle", "장착 시 사용할 스킬"), 16, 6.0f);
        for (const RunItemPresentation::FItemSkillDetails& Detail : SkillDetails)
        {
            AddText(DetailsSkills, Detail.Name, 16, 4.0f)->SetColorAndOpacity(Detail.Color);
            AddText(DetailsSkills, Detail.Stats, 14, 4.0f);
            AddText(DetailsSkills, Detail.Description, 14, 10.0f);
        }
    }
    else AddText(DetailsSkills, NSLOCTEXT("Inventory", "NoGrantedSkills", "이 아이템에 저장된 부여 스킬이 없습니다."), 14, 0.0f);
    const FRunEquipmentProfile* Profile = URunEquipmentCatalog::Get().ResolveProfile(SelectedItem);
    if (!Profile)
    {
        DetailsHint->SetText(RunItemPresentation::EquipmentDescription(SelectedItem));
        return;
    }
    const FText Hint = DisplayedMember.CurrentHP == 0.0f ? NSLOCTEXT("Equipment", "DeadChangeHint", "사망한 캐릭터는 장비를 변경할 수 없습니다.") : bCanChangeEquipment ? NSLOCTEXT("Inventory", "DetailsDragHint", "목록에서 장비 슬롯으로 끌어 장착하세요.") : NSLOCTEXT("Inventory", "DetailsReadOnlyHint", "상점에서 장비를 변경할 수 있습니다.");
    DetailsHint->SetText(FText::Format(NSLOCTEXT("Inventory", "ResolvedEquipmentDetails", "{0}\n{1}"), RunItemPresentation::EquipmentDescription(SelectedItem), Hint));
}

void UCharacterInventoryPanel::RefreshSaleControls()
{
    if (!SaleControls) return;
    const bool bHasItem = DisplayedMember.Items.IsValidIndex(SelectedItemIndex) && VisibleItemIndices.Contains(SelectedItemIndex);
    SaleControls->SetVisibility(bHasItem ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    const bool bPending = Controller && Controller->IsShopPurchasePending();
    const int32 Price = bHasItem ? RunItemSaleRules::GetPrice(SelectedItem) : 0;
    const bool bCanSell = bHasItem && bCanSellItems && !bPending && Price > 0 && !RunEquipmentRules::IsItemEquipped(DisplayedMember, SelectedItemIndex);
    const FText Name = bHasItem ? RunItemPresentation::Name(SelectedItem, DisplayedRarities) : FText::GetEmpty();
    SaleSummary->SetText(bConfirmingSale ? FText::Format(NSLOCTEXT("InventorySale", "ConfirmSummary", "{0} · 판매가 {1}G"), Name, FText::AsNumber(Price)) : FText::Format(NSLOCTEXT("InventorySale", "Price", "판매가격 {0}G"), FText::AsNumber(Price)));
    SaleSummary->SetToolTipText(SaleSummary->GetText());
    const FText Hint = bPending ? NSLOCTEXT("InventorySale", "TransactionPending", "처리 중인 거래가 끝날 때까지 기다려 주세요.") : bConfirmingSale ? NSLOCTEXT("InventorySale", "ConfirmWarning", "판매하면 되돌릴 수 없습니다.") : Price <= 0 ? NSLOCTEXT("InventorySale", "NoPrice", "가격이 없는 아이템은 판매할 수 없습니다.") : bCanSell ? NSLOCTEXT("InventorySale", "UnequipFirst", "장착한 아이템은 먼저 해제하세요.") : NSLOCTEXT("InventorySale", "Unavailable", "본인 생존 캐릭터만 아이템 상점에서 판매할 수 있습니다.");
    SaleHint->SetText(Hint);
    SellButton->Configure(TEXT("Sell"), bConfirmingSale ? NSLOCTEXT("InventorySale", "Confirm", "판매 확정") : NSLOCTEXT("InventorySale", "Sell", "판매"));
    SellButton->SetIsEnabled(bCanSell);
    CancelSaleButton->Configure(TEXT("Cancel"), NSLOCTEXT("InventorySale", "Cancel", "취소"));
    CancelSaleButton->SetVisibility(bConfirmingSale ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CancelSaleButton->SetIsEnabled(!bPending);
}

void UCharacterInventoryPanel::HandleSaleAction(FName ActionId)
{
    if (ActionId == TEXT("Cancel"))
    {
        bConfirmingSale = false;
        SaleCommand = FRunItemSaleCommand();
        RefreshSaleControls();
        return;
    }
    AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (ActionId != TEXT("Sell") || !Controller || !bCanSellItems || Controller->IsShopPurchasePending() || !VisibleItemIndices.Contains(SelectedItemIndex) || !DisplayedMember.Items.IsValidIndex(SelectedItemIndex) || RunItemSaleRules::GetPrice(SelectedItem) <= 0 || RunEquipmentRules::IsItemEquipped(DisplayedMember, SelectedItemIndex)) return;
    if (!bConfirmingSale)
    {
        SaleCommand.CharacterId = DisplayedMember.CharacterId;
        SaleCommand.ItemIndex = SelectedItemIndex;
        SaleCommand.Asset = SelectedItem.Asset;
        SaleCommand.ItemInstanceId = SelectedItem.ItemInstanceId;
        SaleCommand.ExpectedEquipmentRevision = DisplayedMember.Equipment.Revision;
        SaleCommand.ExpectedShopRevision = DisplayedShopRevision;
        SaleCommand.EncounterId = DisplayedEncounterId;
        bConfirmingSale = true;
        RefreshSaleControls();
        return;
    }
    const FRunItemSaleCommand Command = SaleCommand;
    // Clear the selected copy before a synchronous result or replicated list can reuse its former index.
    // 동기 결과나 복제 목록이 이전 인덱스를 재사용하기 전에 선택한 사본을 비웁니다.
    bConfirmingSale = false;
    SaleCommand = FRunItemSaleCommand();
    bSuppressAutomaticSelection = true;
    SelectedItemIndex = INDEX_NONE;
    RefreshSelectedItem();
    Controller->RequestSellInventoryItem(Command);
}

bool UCharacterInventoryPanel::CanAcceptDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot) const
{
    FText Error;
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    return FEquipmentDropRequest::CanDrop(Controller, DisplayedMember, bCanChangeEquipment && Controller && !Controller->IsShopPurchasePending(), Operation, FGameplayTag(), Error);
}

bool UCharacterInventoryPanel::HandleDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot)
{
    FText Error;
    AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (!FEquipmentDropRequest::Submit(Controller, DisplayedMember, bCanChangeEquipment && Controller && !Controller->IsShopPurchasePending(), Operation, FGameplayTag(), Error)) StatusText->SetText(Error);
    return Operation != nullptr;
}

bool UCharacterInventoryPanel::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
    const UEquipmentDragDropOperation* Drag = Cast<UEquipmentDragDropOperation>(Operation);
    return Drag && HandleDrop(Drag, FGameplayTag());
}
