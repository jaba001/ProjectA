#include "UI/Gameplay/CharacterInventoryPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "UI/Theme/DemonicUITheme.h"

UTextBlock* UCharacterInventoryPanel::AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 22, FontSize);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    return Label;
}

void UCharacterInventoryPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
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
    Theme.AddDivider(WidgetTree, Content);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    Content->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* InventoryContent = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(InventoryContent);
    ItemCountText = AddText(InventoryContent, FText::GetEmpty(), 20, 10.0f);
    EmptyItemsText = AddText(InventoryContent, NSLOCTEXT("Inventory", "EmptyItems", "보유한 아이템이 없습니다."), 16, 12.0f);
    ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryItems"));
    ItemGrid->SetSlotPadding(FMargin(4.0f));
    InventoryContent->AddChildToVerticalBox(ItemGrid);
    Theme.AddDivider(WidgetTree, InventoryContent);
    AddText(InventoryContent, NSLOCTEXT("Inventory", "Skills", "보유 · 장착 스킬"), 22, 10.0f);
    SkillStatusText = AddText(InventoryContent, FText::GetEmpty(), 15, 8.0f);
    SkillList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventorySkills"));
    InventoryContent->AddChildToVerticalBox(SkillList);
    Theme.ApplyControls(WidgetTree);
}

void UCharacterInventoryPanel::AddItem(const FRunItemDefinition& Item, int32 Count, int32 Index)
{
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(Card);
    Card->SetPadding(FMargin(8.0f, 10.0f));
    UUniformGridSlot* GridSlot = ItemGrid->AddChildToUniformGrid(Card, Index / 2, Index % 2);
    GridSlot->SetHorizontalAlignment(HAlign_Fill);
    GridSlot->SetVerticalAlignment(VAlign_Fill);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Content);
    USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
    IconSize->SetWidthOverride(54.0f);
    IconSize->SetHeightOverride(54.0f);
    Content->AddChildToVerticalBox(IconSize)->SetHorizontalAlignment(HAlign_Center);
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleSlot(Frame, true);
    Frame->SetPadding(FMargin(9.0f));
    IconSize->SetContent(Frame);
    UImage* Icon = WidgetTree->ConstructWidget<UImage>();
    Theme.SetItemIcon(Icon, Item.Tags);
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Frame->SetContent(Icon);
    const FText ItemName = Item.DisplayName.IsEmpty() ? FText::FromString(Item.Asset.GetAssetName()) : Item.DisplayName;
    UTextBlock* Name = AddText(Content, ItemName, 14, 6.0f);
    Name->SetWrapTextAt(154.0f);
    Name->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    Name->SetJustification(ETextJustify::Center);
    UTextBlock* Quantity = AddText(Content, FText::Format(NSLOCTEXT("Inventory", "Quantity", "{0}개"), FText::AsNumber(Count)), 14, 0.0f);
    Quantity->SetJustification(ETextJustify::Center);
    Card->SetToolTipText(FText::Format(NSLOCTEXT("Inventory", "ItemTooltip", "{0}\n수량 {1}개\n{2}"), ItemName, FText::AsNumber(Count), FText::FromString(Item.Asset.ToString())));
}

void UCharacterInventoryPanel::AddSkill(const USkillDefinitionDataAsset* Skill)
{
    if (!Skill)
    {
        AddText(SkillList, NSLOCTEXT("Inventory", "MissingSkill", "스킬 정보를 불러올 수 없습니다."), 16, 12.0f);
        return;
    }

    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(Card);
    Card->SetPadding(FMargin(10.0f));
    SkillList->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Card->SetContent(Row);
    if (Skill->SkillIcon)
    {
        USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
        IconSize->SetWidthOverride(42.0f);
        IconSize->SetHeightOverride(42.0f);
        UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize);
        IconSlot->SetVerticalAlignment(VAlign_Top);
        IconSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
        UImage* Icon = WidgetTree->ConstructWidget<UImage>();
        Icon->SetBrushFromTexture(Skill->SkillIcon);
        Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
        IconSize->SetContent(Icon);
    }
    UVerticalBox* Description = WidgetTree->ConstructWidget<UVerticalBox>();
    Row->AddChildToHorizontalBox(Description)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const FText Name = Skill->SkillName.IsEmpty() ? FText::FromName(Skill->SkillId) : Skill->SkillName;
    AddText(Description, Name, 18, 4.0f);
    AddText(Description, Skill->GetActionPointCostText(), 14, 4.0f);
    if (!Skill->SkillDescription.IsEmpty()) AddText(Description, Skill->SkillDescription, 14, 0.0f);
}

void UCharacterInventoryPanel::RefreshInventory(const FGameplayViewState& View, FGuid CharacterId)
{
    if (!ItemGrid || !SkillList) return;
    ItemGrid->ClearChildren();
    SkillList->ClearChildren();
    const FRunPartyMember* Member = CharacterId.IsValid() ? View.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId && Candidate.bCreated; }) : nullptr;
    GoldText->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ItemCountText->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    EmptyItemsText->SetVisibility(Member && Member->Items.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StatusText->SetVisibility(ESlateVisibility::Visible);
    SkillStatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (!Member)
    {
        GoldText->SetText(FText::GetEmpty());
        StatusText->SetText(NSLOCTEXT("Inventory", "NoInventory", "직접 조작 캐릭터가 없어 표시할 인벤토리가 없습니다."));
        return;
    }

    GoldText->SetText(FText::Format(NSLOCTEXT("Inventory", "Gold", "보유 골드 {0}G"), FText::AsNumber(Member->Gold)));
    StatusText->SetText(Member->CurrentHP == 0.0f ? NSLOCTEXT("Inventory", "DeadInventory", "사망 · 보유 아이템과 스킬 보기") : NSLOCTEXT("Inventory", "ItemPrototype", "구매한 아이템은 보관되며 장착 효과는 아직 없습니다."));
    ItemCountText->SetText(FText::Format(NSLOCTEXT("Inventory", "Items", "보유 아이템 · {0}개"), FText::AsNumber(Member->Items.Num())));

    // Group repeated purchases by their full asset path, preserving distinct assets with the same name.
    // 전체 에셋 경로로 반복 구매를 묶어 이름이 같은 서로 다른 에셋을 구분합니다.
    TMap<FSoftObjectPath, int32> ItemCounts;
    for (const FRunItemDefinition& Item : Member->Items) ++ItemCounts.FindOrAdd(Item.Asset);
    int32 ItemIndex = 0;
    for (const FRunItemDefinition& Item : Member->Items)
    {
        const int32* Count = ItemCounts.Find(Item.Asset);
        if (!Count) continue;
        AddItem(Item, *Count, ItemIndex++);
        ItemCounts.Remove(Item.Asset);
    }
    if (Member->bHasSkillLoadout)
    {
        if (Member->Skills.IsEmpty()) AddText(SkillList, NSLOCTEXT("Inventory", "EmptySkills", "보유한 스킬이 없습니다."), 16);
        for (const FSoftObjectPath& Path : Member->Skills) AddSkill(Cast<USkillDefinitionDataAsset>(Path.TryLoad()));
        return;
    }

    // Only the authoritative Run owns the legacy catalog; clients must not substitute local defaults.
    // 권위 Run만 이전 저장의 목록을 보유하므로 클라이언트에서 로컬 기본값으로 대체하지 않습니다.
    const APlayerController* Controller = GetOwningPlayer();
    const UGameInstance* GameInstance = GetGameInstance();
    const URunStateSubsystem* Run = Controller && Controller->HasAuthority() && GameInstance ? GameInstance->GetSubsystem<URunStateSubsystem>() : nullptr;
    const UPartyDefinitionDataAsset* Catalog = Run ? Run->PartyDefinition.Get() : nullptr;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
    FText Error;
    SkillStatusText->SetVisibility(ESlateVisibility::Visible);
    if (!Catalog || !Catalog->ResolveMemberSkills(*Member, Skills, Error))
    {
        SkillStatusText->SetText(NSLOCTEXT("Inventory", "LegacyUnavailable", "이전 저장의 장착 스킬 정보를 불러올 수 없습니다."));
        return;
    }
    SkillStatusText->SetText(NSLOCTEXT("Inventory", "LegacySkills", "이전 저장의 직업 기본 장착 스킬입니다."));
    for (const USkillDefinitionDataAsset* Skill : Skills) AddSkill(Skill);
    if (Skills.IsEmpty()) AddText(SkillList, NSLOCTEXT("Inventory", "EmptySkills", "보유한 스킬이 없습니다."), 16);
}
