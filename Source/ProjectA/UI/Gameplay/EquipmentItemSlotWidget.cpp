#include "UI/Gameplay/EquipmentItemSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "InputCoreTypes.h"
#include "UI/Gameplay/EquipmentDragDropOperation.h"
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

void UEquipmentItemSlotWidget::RefreshSlot(FGuid InCharacterId, int32 InRevision, int32 InItemIndex, const FRunItemDefinition* Item, FGameplayTag InTargetSlot, FName EmptyIcon, const FText& SlotLabel, bool bAllowDrag)
{
    CharacterId = InCharacterId;
    Revision = InRevision;
    ItemIndex = Item ? InItemIndex : INDEX_NONE;
    TargetSlot = InTargetSlot;
    DisplayedItem = Item ? *Item : FRunItemDefinition();
    const bool bEquipmentSlot = TargetSlot.IsValid();
    const bool bSupported = Item && URunEquipmentCatalog::Get().ResolveProfile(*Item);
    bCanDrag = bAllowDrag && Item && (bEquipmentSlot || bSupported);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    SlotText->SetText(SlotLabel);
    SlotText->SetVisibility(SlotLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    IconSize->SetWidthOverride(bEquipmentSlot ? 60.0f : 54.0f);
    IconSize->SetHeightOverride(bEquipmentSlot ? 60.0f : 54.0f);
    ItemText->SetWrapTextAt(bEquipmentSlot ? 68.0f : 148.0f);
    StateText->SetWrapTextAt(bEquipmentSlot ? 68.0f : 148.0f);
    Theme.StyleSlot(Frame, Item != nullptr);
    if (Item) Theme.SetItemIcon(Icon, Item->Tags);
    else Theme.SetEquipmentIcon(Icon, EmptyIcon);
    Icon->SetColorAndOpacity(Item ? FLinearColor::White : FLinearColor(0.62f, 0.56f, 0.46f, 0.72f));
    Card->SetRenderOpacity(!Item || bSupported ? 1.0f : 0.6f);
    const FText ItemName = Item ? (Item->DisplayName.IsEmpty() ? FText::FromString(Item->Asset.GetAssetName()) : Item->DisplayName) : NSLOCTEXT("Equipment", "Empty", "미장착");
    ItemText->SetText(ItemName);
    StateText->SetText(!Item ? FText::GetEmpty() : bEquipmentSlot ? NSLOCTEXT("Equipment", "Equipped", "장착 중") : bSupported ? NSLOCTEXT("Equipment", "Stored", "보관 중") : NSLOCTEXT("Equipment", "Unsupported", "장착 미지원"));
    StateText->SetVisibility(Item ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    SetToolTipText(Item ? FText::Format(NSLOCTEXT("Equipment", "ItemTooltip", "{0}\n{1}\n{2}"), ItemName, StateText->GetText(), FText::FromString(Item->Asset.ToString())) : SlotLabel);
    ResetDropHighlight();
}

FReply UEquipmentItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (bCanDrag && Controller && !Controller->IsEquipmentChangePending()) return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

void UEquipmentItemSlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent, UDragDropOperation*& OutOperation)
{
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    if (!bCanDrag || !Controller || Controller->IsEquipmentChangePending() || ItemIndex == INDEX_NONE) return;
    UEquipmentDragDropOperation* Drag = NewObject<UEquipmentDragDropOperation>(this);
    Drag->CharacterId = CharacterId;
    Drag->ItemIndex = ItemIndex;
    Drag->ExpectedRevision = Revision;
    UEquipmentItemSlotWidget* Preview = CreateWidget<UEquipmentItemSlotWidget>(GetOwningPlayer());
    Preview->RefreshSlot(CharacterId, Revision, ItemIndex, &DisplayedItem, FGameplayTag(), NAME_None, FText::GetEmpty(), false);
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
    Frame->SetBrushColor(bAccepted ? FLinearColor(0.62f, 1.0f, 0.65f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.3f, 1.0f));
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
    if (Frame) UDemonicUITheme::Get().StyleSlot(Frame, ItemIndex != INDEX_NONE);
}
