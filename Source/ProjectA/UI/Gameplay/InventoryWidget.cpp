#include "UI/Gameplay/InventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/EngineBaseTypes.h"
#include "UI/Gameplay/CharacterEquipmentPanel.h"
#include "UI/Gameplay/CharacterInventoryPanel.h"
#include "UI/Theme/DemonicUITheme.h"

UInventoryWidget::UInventoryWidget()
{
    bIsBackHandler = true;
    bIsModal = true;
}

TOptional<FUIInputConfig> UInventoryWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    Root->SetVisibility(ESlateVisibility::Visible);
    WidgetTree->RootWidget = Root;

    // Keep the battlefield visible while blocking clicks through the inventory.
    // 전장을 보이게 유지하면서 인벤토리 아래로 클릭이 전달되지 않도록 합니다.
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryInputBlocker"));
    Background->SetBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f)));
    Background->SetVisibility(ESlateVisibility::Visible);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Scale);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    ContentSlot->SetPadding(FMargin(24.0f));
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(746.0f);
    Size->SetHeightOverride(840.0f);
    Scale->SetContent(Size);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Size->SetContent(Content);
    UHorizontalBox* Panels = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Panels)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    USizeBox* EquipmentSize = WidgetTree->ConstructWidget<USizeBox>();
    EquipmentSize->SetWidthOverride(300.0f);
    Panels->AddChildToHorizontalBox(EquipmentSize)->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
    EquipmentPanel = WidgetTree->ConstructWidget<UCharacterEquipmentPanel>();
    EquipmentSize->SetContent(EquipmentPanel);
    USizeBox* InventorySize = WidgetTree->ConstructWidget<USizeBox>();
    InventorySize->SetWidthOverride(430.0f);
    Panels->AddChildToHorizontalBox(InventorySize);
    InventoryPanel = WidgetTree->ConstructWidget<UCharacterInventoryPanel>();
    InventorySize->SetContent(InventoryPanel);

    UBorder* Footer = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleInset(Footer);
    Footer->SetPadding(FMargin(12.0f, 8.0f));
    Content->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    UHorizontalBox* FooterContent = WidgetTree->ConstructWidget<UHorizontalBox>();
    Footer->SetContent(FooterContent);
    UTextBlock* Shortcuts = WidgetTree->ConstructWidget<UTextBlock>();
    Shortcuts->SetText(NSLOCTEXT("Inventory", "Shortcuts", "I · 인벤토리 닫기    Esc · 설정 열기"));
    Shortcuts->SetAutoWrapText(true);
    Theme.StyleText(Shortcuts, false, 15);
    UHorizontalBoxSlot* ShortcutsSlot = FooterContent->AddChildToHorizontalBox(Shortcuts);
    ShortcutsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ShortcutsSlot->SetVerticalAlignment(VAlign_Center);
    ShortcutsSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
    CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_CloseInventory"));
    UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CloseLabel->SetText(NSLOCTEXT("Inventory", "Close", "닫기"));
    CloseLabel->SetJustification(ETextJustify::Center);
    CastChecked<UButtonSlot>(CloseButton->AddChild(CloseLabel))->SetPadding(FMargin(20.0f, 10.0f));
    FooterContent->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);
    CloseButton->OnClicked.AddUniqueDynamic(this, &UInventoryWidget::HandleClose);
    Theme.ApplyControls(WidgetTree);
    Theme.StyleButton(CloseButton, true);
}

void UInventoryWidget::RefreshInventory(const FGameplayViewState& View, FGuid CharacterId)
{
    if (EquipmentPanel) EquipmentPanel->RefreshEquipment(View, CharacterId);
    if (InventoryPanel) InventoryPanel->RefreshInventory(View, CharacterId);
}

void UInventoryWidget::HandleClose()
{
    DeactivateWidget();
}

UWidget* UInventoryWidget::NativeGetDesiredFocusTarget() const
{
    return CloseButton;
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    return FReply::Handled();
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    return FReply::Handled();
}
