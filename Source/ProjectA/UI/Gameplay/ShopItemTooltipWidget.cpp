#include "UI/Gameplay/ShopItemTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UI/Gameplay/RunItemPresentation.h"
#include "UI/Theme/DemonicUITheme.h"

namespace
{
    constexpr float TooltipWidth = 440.f;
    constexpr float ContentWidth = TooltipWidth - 28.f;
    constexpr float HeaderWidth = ContentWidth - 68.f;
}

UTextBlock* UShopItemTooltipWidget::AddText(UVerticalBox* Parent, int32 FontSize, float WrapWidth, bool bHeading)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetAutoWrapText(true);
    Text->SetWrapTextAt(WrapWidth);
    Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    UDemonicUITheme::Get().StyleText(Text, bHeading, FontSize);
    Parent->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
    return Text;
}

void UShopItemTooltipWidget::UpdateMaximumHeight()
{
    if (!TooltipBounds) return;
    float MaximumHeight = 600.f;
    if (FSlateApplication::IsInitialized())
    {
        // Tooltip windows use desktop DPI rather than the game's viewport scaling curve.
        // 툴팁 창은 게임 뷰포트 배율 곡선 대신 데스크톱 DPI를 사용합니다.
        FSlateApplication& Slate = FSlateApplication::Get();
        const FVector2D PointerPosition = Slate.GetCursorPos();
        const FSlateRect WorkArea = Slate.GetWorkArea(FSlateRect(PointerPosition.X, PointerPosition.Y, PointerPosition.X + 1.f, PointerPosition.Y + 1.f));
        const float Scale = Slate.GetApplicationScale() * FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(PointerPosition.X, PointerPosition.Y);
        if (Scale > 0.f && WorkArea.Bottom > WorkArea.Top) MaximumHeight = FMath::Clamp((WorkArea.Bottom - WorkArea.Top) / Scale - 32.f, 1.f, MaximumHeight);
    }
    TooltipBounds->SetMaxDesiredHeight(MaximumHeight);
}

void UShopItemTooltipWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetIsFocusable(false);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    TooltipBounds = WidgetTree->ConstructWidget<USizeBox>();
    TooltipBounds->SetWidthOverride(TooltipWidth);
    WidgetTree->RootWidget = TooltipBounds;
    UpdateMaximumHeight();
    // Downscale unusually long saved skill lists without clipping; ordinary one-skill content keeps its natural size.
    // 유난히 긴 저장 스킬 목록은 자르지 않고 축소하며 일반적인 스킬 한 개 내용은 원래 크기를 유지합니다.
    UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    TooltipBounds->SetContent(Fit);
    USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(TooltipWidth);
    Fit->SetContent(Width);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(Panel);
    Panel->SetPadding(FMargin(14.f));
    Width->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);
    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
    IconSize->SetWidthOverride(56.f);
    IconSize->SetHeightOverride(56.f);
    UHorizontalBoxSlot* IconSlot = Header->AddChildToHorizontalBox(IconSize);
    IconSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    IconSlot->SetVerticalAlignment(VAlign_Top);
    ItemIcon = WidgetTree->ConstructWidget<UImage>();
    ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    IconSize->SetContent(ItemIcon);
    UVerticalBox* Identity = WidgetTree->ConstructWidget<UVerticalBox>();
    Header->AddChildToHorizontalBox(Identity)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ItemName = AddText(Identity, 18, HeaderWidth, true);
    PriceStatus = AddText(Identity, 16, HeaderWidth);
    EquipmentText = AddText(Content, 16, ContentWidth);
    Theme.AddDivider(WidgetTree, Content);
    AddText(Content, 18, ContentWidth, true)->SetText(NSLOCTEXT("ShopItemTooltip", "SkillsHeading", "부여 스킬"));
    SkillList = WidgetTree->ConstructWidget<UVerticalBox>();
    Content->AddChildToVerticalBox(SkillList);
    EmptySkills = AddText(SkillList, 16, ContentWidth);
    EmptySkills->SetText(NSLOCTEXT("ShopItemTooltip", "NoSkills", "부여된 스킬 없음"));
    Theme.AddDivider(WidgetTree, Content);
    AddText(Content, 16, ContentWidth)->SetText(NSLOCTEXT("ShopItemTooltip", "EquipHint", "구매한 아이템은 인벤토리에 보관됩니다. 인벤토리에서 캐릭터에게 장착하면 부여 스킬을 사용할 수 있습니다."));
}

void UShopItemTooltipWidget::AddSkillRow()
{
    UVerticalBox* Row = WidgetTree->ConstructWidget<UVerticalBox>();
    SkillList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
    SkillRows.Add(Row);
    SkillNames.Add(AddText(Row, 17, ContentWidth, true));
    SkillDescriptions.Add(AddText(Row, 16, ContentWidth));
    SkillStats.Add(AddText(Row, 16, ContentWidth));
}

void UShopItemTooltipWidget::ConfigureItem(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities, const FText& Status)
{
    if (!ItemName || !SkillList) return;
    UpdateMaximumHeight();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    Theme.SetItemIcon(ItemIcon, Item.Tags);
    Theme.StyleText(ItemName, true, 18);
    ItemName->SetText(RunItemPresentation::Name(Item, Rarities));
    if (const FRunWeaponRarityRule* Rarity = RunItemPresentation::FindRarity(Item, Rarities)) ItemName->SetColorAndOpacity(Rarity->Color);
    PriceStatus->SetText(Status.IsEmpty() ? FText::Format(NSLOCTEXT("ShopItemTooltip", "Price", "가격 {0}G"), FText::AsNumber(Item.Price)) : FText::Format(NSLOCTEXT("ShopItemTooltip", "PriceStatus", "가격 {0}G · {1}"), FText::AsNumber(Item.Price), Status));
    const FText Equipment = RunItemPresentation::EquipmentDescription(Item);
    EquipmentText->SetText(Equipment);
    EquipmentText->SetVisibility(Equipment.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    // Store resolved display values in widgets and reuse rows; no references to the changing Run view are retained.
    // 해석한 표시값만 위젯에 저장하고 행을 재사용하며 변경되는 Run 뷰의 참조는 보관하지 않습니다.
    const TArray<RunItemPresentation::FItemSkillDetails> Skills = RunItemPresentation::SkillDetails(Item);
    while (SkillRows.Num() < Skills.Num()) AddSkillRow();
    EmptySkills->SetVisibility(Skills.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    for (int32 Index = 0; Index < SkillRows.Num(); ++Index)
    {
        const bool bVisible = Skills.IsValidIndex(Index);
        SkillRows[Index]->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bVisible) continue;
        const RunItemPresentation::FItemSkillDetails& Skill = Skills[Index];
        SkillNames[Index]->SetText(Skill.Name);
        SkillNames[Index]->SetColorAndOpacity(Skill.Color);
        SkillDescriptions[Index]->SetText(Skill.Description);
        SkillDescriptions[Index]->SetVisibility(Skill.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        SkillStats[Index]->SetText(Skill.Stats);
        SkillStats[Index]->SetVisibility(Skill.Stats.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}
