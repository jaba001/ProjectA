#include "UI/Gameplay/EquipmentItemSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "InputCoreTypes.h"
#include "UI/Gameplay/EquipmentDragDropOperation.h"
#include "UI/Gameplay/RunItemPresentation.h"
#include "UI/Gameplay/ShopItemTooltipWidget.h"
#include "UI/Theme/DemonicUITheme.h"

void UEquipmentItemSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::Visible);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    Card = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(Card);
    Card->SetPadding(FMargin(4.0f, 8.0f));
    WidgetTree->RootWidget = Card;
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Content);
    SlotText = WidgetTree->ConstructWidget<UTextBlock>();
    SlotText->SetJustification(ETextJustify::Center);
    Theme.StyleText(SlotText, false, 13);
    Content->AddChildToVerticalBox(SlotText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
    IconSize = WidgetTree->ConstructWidget<USizeBox>();
    Content->AddChildToVerticalBox(IconSize)->SetHorizontalAlignment(HAlign_Center);
    Frame = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleSlot(Frame);
    Frame->SetPadding(FMargin(9.0f));
    IconSize->SetContent(Frame);
    Icon = WidgetTree->ConstructWidget<UImage>();
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Frame->SetContent(Icon);
    ItemText = WidgetTree->ConstructWidget<UTextBlock>();
    ItemText->SetAutoWrapText(true);
    ItemText->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    ItemText->SetJustification(ETextJustify::Center);
    Theme.StyleText(ItemText, false, 13);
    Content->AddChildToVerticalBox(ItemText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));
    StateText = WidgetTree->ConstructWidget<UTextBlock>();
    StateText->SetAutoWrapText(true);
    StateText->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    StateText->SetJustification(ETextJustify::Center);
    Theme.StyleText(StateText, false, 12);
    Content->AddChildToVerticalBox(StateText);
}

void UEquipmentItemSlotWidget::UseListPresentation()
{
    if (bListPresentation || !Card) return;
    bListPresentation = true;
    SetIsFocusable(true);
    // Reuse the existing icon and name so selecting a row never rebuilds its drag source.
    // 행 선택 시 드래그 원본을 다시 만들지 않도록 기존 아이콘과 이름을 재사용합니다.
    Icon->RemoveFromParent();
    IconSize->RemoveFromParent();
    ItemText->RemoveFromParent();
    StateText->RemoveFromParent();
    IconSize->SetContent(Icon);
    IconSize->SetWidthOverride(24.0f);
    IconSize->SetHeightOverride(24.0f);
    SlotText->SetVisibility(ESlateVisibility::Collapsed);
    StateText->SetVisibility(ESlateVisibility::Collapsed);
    ItemText->SetAutoWrapText(false);
    ItemText->SetWrapTextAt(0.0f);
    ItemText->SetJustification(ETextJustify::Left);
    ItemText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    ItemText->SetClipping(EWidgetClipping::ClipToBounds);
    ItemText->SetVisibility(ESlateVisibility::HitTestInvisible);
    UDemonicUITheme::Get().StyleText(ItemText, false, 16);
    Card->SetBrush(FSlateColorBrush(FLinearColor::White));
    Card->SetPadding(FMargin(8.0f, 4.0f));
    USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>();
    RowSize->SetMinDesiredHeight(24.0f);
    Card->SetContent(RowSize);
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    RowSize->SetContent(Row);
    UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize);
    IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
    IconSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(ItemText);
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    NameSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* StateSlot = Row->AddChildToHorizontalBox(StateText);
    StateSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
    StateSlot->SetVerticalAlignment(VAlign_Center);
    StateText->SetAutoWrapText(false);
    StateText->SetWrapTextAt(0.0f);
    UpdateListHighlight();
}

void UEquipmentItemSlotWidget::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    UpdateListHighlight();
}

void UEquipmentItemSlotWidget::RefreshSlot(FGuid InCharacterId, int32 InRevision, int32 InItemIndex, const FRunItemDefinition* Item, FGameplayTag InTargetSlot, FName EmptyIcon, const FText& SlotLabel, bool bAllowDrag, const TArray<FRunWeaponRarityRule>& Rarities)
{
    CharacterId = InCharacterId;
    Revision = InRevision;
    ItemIndex = Item ? InItemIndex : INDEX_NONE;
    TargetSlot = InTargetSlot;
    DisplayedItem = Item ? *Item : FRunItemDefinition();
    DisplayedRarities = Rarities;
    const bool bEquipmentSlot = TargetSlot.IsValid();
    const bool bSupported = Item && URunEquipmentCatalog::Get().ResolveProfile(*Item);
    bCanDrag = bAllowDrag && Item && (bEquipmentSlot || bSupported);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    SlotText->SetText(SlotLabel);
    SlotText->SetVisibility(bListPresentation || SlotLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    IconSize->SetWidthOverride(bListPresentation ? 24.0f : bEquipmentSlot ? 60.0f : 54.0f);
    IconSize->SetHeightOverride(bListPresentation ? 24.0f : bEquipmentSlot ? 60.0f : 54.0f);
    ItemText->SetWrapTextAt(bListPresentation ? 0.0f : bEquipmentSlot ? 68.0f : 148.0f);
    StateText->SetWrapTextAt(bListPresentation ? 0.0f : bEquipmentSlot ? 68.0f : 148.0f);
    Theme.StyleSlot(Frame, Item != nullptr);
    if (Item) Theme.SetItemIcon(Icon, Item->Tags);
    else Theme.SetEquipmentIcon(Icon, EmptyIcon);
    Icon->SetColorAndOpacity(Item ? FLinearColor::White : FLinearColor(0.62f, 0.56f, 0.46f, 0.72f));
    Card->SetRenderOpacity(bListPresentation || !Item || bSupported ? 1.0f : 0.6f);
    const FText ItemName = Item ? RunItemPresentation::Name(*Item, DisplayedRarities) : NSLOCTEXT("Equipment", "Empty", "미장착");
    Theme.StyleText(ItemText, false, bListPresentation ? 16 : 13);
    if (const FRunWeaponRarityRule* Rarity = RunItemPresentation::FindRarity(DisplayedItem, DisplayedRarities)) ItemText->SetColorAndOpacity(Rarity->Color);
    ItemText->SetText(bListPresentation && Item ? FText::Format(NSLOCTEXT("Equipment", "ListItem", "{0} (1)"), ItemName) : ItemName);
    StateText->SetText(!Item ? FText::GetEmpty() : bEquipmentSlot ? NSLOCTEXT("Equipment", "Equipped", "장착 중") : bSupported ? NSLOCTEXT("Equipment", "Stored", "보관 중") : NSLOCTEXT("Equipment", "Unsupported", "장착 미지원"));
    StateText->SetVisibility(Item && !bListPresentation ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Item)
    {
        // Cache the shared item details on refresh; hovering never changes equipment or loads a new random copy.
        // 갱신 시 공통 아이템 상세를 보관하며 hover로 장비를 바꾸거나 새 사본을 추첨하지 않습니다.
        if (!ItemTooltip) ItemTooltip = CreateWidget<UShopItemTooltipWidget>(GetOwningPlayer());
        const FText Hint = bCanDrag ? bEquipmentSlot ? NSLOCTEXT("Equipment", "TooltipUnequipHint", "가방으로 끌어 해제할 수 있습니다.") : NSLOCTEXT("Equipment", "TooltipEquipHint", "장비 슬롯으로 끌어 장착할 수 있습니다.") : FText::GetEmpty();
        const FText Status = Hint.IsEmpty() ? StateText->GetText() : FText::Format(NSLOCTEXT("Equipment", "TooltipItemState", "{0}\n{1}"), StateText->GetText(), Hint);
        ItemTooltip->ConfigureInventory(*Item, DisplayedRarities, Status);
        if (GetToolTip() != ItemTooltip)
        {
            SetToolTipText(FText::GetEmpty());
            SetToolTip(ItemTooltip);
        }
    }
    else
    {
        SetToolTip(nullptr);
        SetToolTipText(FText::Format(NSLOCTEXT("Equipment", "EmptySlotTooltip", "{0} · 미장착\n지원하는 아이템을 상점에서 이 슬롯으로 끌어 장착하세요."), SlotLabel));
    }
    ResetDropHighlight();
}

FReply UEquipmentItemSlotWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (bListPresentation && ItemIndex != INDEX_NONE && (KeyEvent.GetKey() == EKeys::Enter || KeyEvent.GetKey() == EKeys::SpaceBar))
    {
        ItemSelected.ExecuteIfBound(ItemIndex);
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

void UEquipmentItemSlotWidget::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    Super::NativeOnMouseEnter(Geometry, MouseEvent);
    bListHovered = true;
    UpdateListHighlight();
}

void UEquipmentItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
    Super::NativeOnMouseLeave(MouseEvent);
    bListHovered = false;
    UpdateListHighlight();
}

FReply UEquipmentItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    const bool bSelectItem = bListPresentation && ItemIndex != INDEX_NONE && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
    if (bSelectItem)
    {
        ItemSelected.ExecuteIfBound(ItemIndex);
        SetKeyboardFocus();
    }
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (bCanDrag && Controller && !Controller->IsShopPurchasePending()) return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    if (bSelectItem) return FReply::Handled();
    return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

void UEquipmentItemSlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent, UDragDropOperation*& OutOperation)
{
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (!bCanDrag || !Controller || Controller->IsShopPurchasePending() || ItemIndex == INDEX_NONE) return;
    UEquipmentDragDropOperation* Drag = NewObject<UEquipmentDragDropOperation>(this);
    Drag->CharacterId = CharacterId;
    Drag->ItemIndex = ItemIndex;
    Drag->ExpectedRevision = Revision;
    UEquipmentItemSlotWidget* Preview = CreateWidget<UEquipmentItemSlotWidget>(GetOwningPlayer());
    Preview->RefreshSlot(CharacterId, Revision, ItemIndex, &DisplayedItem, FGameplayTag(), NAME_None, FText::GetEmpty(), false, DisplayedRarities);
    Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
    Preview->SetRenderOpacity(0.9f);
    Drag->DefaultDragVisual = Preview;
    Drag->Pivot = EDragPivot::CenterCenter;
    OutOperation = Drag;
}

bool UEquipmentItemSlotWidget::NativeOnDragOver(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
    const UEquipmentDragDropOperation* Drag = Cast<UEquipmentDragDropOperation>(Operation);
    if (!Drag || !CanAcceptDrop.IsBound()) return false;
    const bool bAccepted = CanAcceptDrop.Execute(Drag, TargetSlot);
    bDropHighlighted = true;
    bDropAccepted = bAccepted;
    if (bListPresentation) UpdateListHighlight();
    else Frame->SetBrushColor(bAccepted ? FLinearColor(0.62f, 1.0f, 0.65f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.3f, 1.0f));
    return true;
}

void UEquipmentItemSlotWidget::NativeOnDragLeave(const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
    Super::NativeOnDragLeave(DragDropEvent, Operation);
    ResetDropHighlight();
}

bool UEquipmentItemSlotWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
    ResetDropHighlight();
    const UEquipmentDragDropOperation* Drag = Cast<UEquipmentDragDropOperation>(Operation);
    if (!Drag || !ReceiveDrop.IsBound()) return false;
    return ReceiveDrop.Execute(Drag, TargetSlot);
}

void UEquipmentItemSlotWidget::ResetDropHighlight()
{
    bDropHighlighted = false;
    if (bListPresentation) UpdateListHighlight();
    else if (Frame) UDemonicUITheme::Get().StyleSlot(Frame, ItemIndex != INDEX_NONE);
}

void UEquipmentItemSlotWidget::UpdateListHighlight()
{
    if (!bListPresentation || !Card) return;
    const bool bUnsupported = ItemIndex != INDEX_NONE && !URunEquipmentCatalog::Get().ResolveProfile(DisplayedItem);
    StateText->SetText(bSelected ? NSLOCTEXT("Equipment", "SelectedItem", "선택") : bUnsupported ? NSLOCTEXT("Equipment", "Unsupported", "장착 미지원") : FText::GetEmpty());
    StateText->SetVisibility(bSelected || bUnsupported ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    FLinearColor Background(0.028f, 0.024f, 0.02f, 0.68f);
    if (bDropHighlighted) Background = bDropAccepted ? FLinearColor(0.1f, 0.24f, 0.1f, 0.92f) : FLinearColor(0.3f, 0.09f, 0.06f, 0.92f);
    else if (bSelected) Background = FLinearColor(0.34f, 0.28f, 0.2f, 0.94f);
    else if (bListHovered) Background = FLinearColor(0.18f, 0.15f, 0.11f, 0.88f);
    Card->SetBrushColor(Background);
    const FRunWeaponRarityRule* Rarity = RunItemPresentation::FindRarity(DisplayedItem, DisplayedRarities);
    ItemText->SetColorAndOpacity(Rarity ? Rarity->Color : bSelected || bListHovered ? FLinearColor(1.0f, 0.96f, 0.84f) : FLinearColor(0.91f, 0.87f, 0.81f));
}
