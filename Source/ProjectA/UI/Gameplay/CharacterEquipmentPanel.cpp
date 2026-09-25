#include "UI/Gameplay/CharacterEquipmentPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Profession/ProfessionBase.h"
#include "UI/Theme/DemonicUITheme.h"

UTextBlock* UCharacterEquipmentPanel::AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 22, FontSize);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    return Label;
}

void UCharacterEquipmentPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EquipmentPanel"));
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
    Title->SetText(NSLOCTEXT("Equipment", "Title", "현재 장비"));
    Title->SetJustification(ETextJustify::Center);
    Theme.StyleText(Title, true, 26);
    Header->SetContent(Title);
    CharacterText = AddText(Content, FText::GetEmpty(), 20, 6.0f);
    CharacterText->SetWrapTextAt(248.0f);
    CharacterText->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    StatusText = AddText(Content, FText::GetEmpty(), 15, 4.0f);
    Theme.AddDivider(WidgetTree, Content);

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    Content->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* EquipmentContent = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(EquipmentContent);
    UOverlay* EquipmentLayout = WidgetTree->ConstructWidget<UOverlay>();
    EquipmentContent->AddChildToVerticalBox(EquipmentLayout)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    USizeBox* BackdropSize = WidgetTree->ConstructWidget<USizeBox>();
    BackdropSize->SetWidthOverride(240.0f);
    BackdropSize->SetHeightOverride(240.0f);
    UOverlaySlot* BackdropSlot = EquipmentLayout->AddChildToOverlay(BackdropSize);
    BackdropSlot->SetHorizontalAlignment(HAlign_Center);
    BackdropSlot->SetVerticalAlignment(VAlign_Center);
    UScaleBox* BackdropScale = WidgetTree->ConstructWidget<UScaleBox>();
    BackdropScale->SetStretch(EStretch::ScaleToFit);
    BackdropSize->SetContent(BackdropScale);
    UImage* Backdrop = WidgetTree->ConstructWidget<UImage>();
    Theme.SetEquipmentBackdrop(Backdrop);
    Backdrop->SetRenderOpacity(0.22f);
    BackdropScale->SetContent(Backdrop);
    EquipmentSlots = WidgetTree->ConstructWidget<UUniformGridPanel>();
    EquipmentSlots->SetSlotPadding(FMargin(3.0f, 5.0f));
    UOverlaySlot* EquipmentSlot = EquipmentLayout->AddChildToOverlay(EquipmentSlots);
    EquipmentSlot->SetHorizontalAlignment(HAlign_Fill);
    EquipmentSlot->SetVerticalAlignment(VAlign_Fill);

    // Show empty presentation slots until the Run owns actual equipment data.
    // Run에 실제 장비 데이터가 생기기 전에는 표시용 빈 슬롯만 보여 줍니다.
    AddEquipmentSlot(TEXT("Weapon"), NSLOCTEXT("Equipment", "WeaponOne", "무기 1"), 0, 0);
    AddEquipmentSlot(TEXT("Head"), NSLOCTEXT("Equipment", "Head", "투구"), 0, 1);
    AddEquipmentSlot(TEXT("Weapon"), NSLOCTEXT("Equipment", "WeaponTwo", "무기 2"), 0, 2);
    AddEquipmentSlot(TEXT("Hands"), NSLOCTEXT("Equipment", "Hands", "장갑"), 1, 0);
    AddEquipmentSlot(TEXT("Body"), NSLOCTEXT("Equipment", "Body", "갑옷"), 1, 1);
    AddEquipmentSlot(TEXT("Neck"), NSLOCTEXT("Equipment", "Neck", "목걸이"), 1, 2);
    AddEquipmentSlot(TEXT("Ring"), NSLOCTEXT("Equipment", "RingOne", "반지 1"), 2, 0);
    AddEquipmentSlot(TEXT("Feet"), NSLOCTEXT("Equipment", "Feet", "신발"), 2, 1);
    AddEquipmentSlot(TEXT("Ring"), NSLOCTEXT("Equipment", "RingTwo", "반지 2"), 2, 2);
    AddText(EquipmentContent, NSLOCTEXT("Equipment", "Prototype", "장착 기능 준비 중\n구매한 아이템은 인벤토리에 보관됩니다."), 15, 8.0f);
    Theme.ApplyControls(WidgetTree);
}

void UCharacterEquipmentPanel::AddEquipmentSlot(FName SlotId, const FText& Label, int32 Row, int32 Column)
{
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UVerticalBox* SlotContent = WidgetTree->ConstructWidget<UVerticalBox>();
    UUniformGridSlot* GridSlot = EquipmentSlots->AddChildToUniformGrid(SlotContent, Row, Column);
    GridSlot->SetHorizontalAlignment(HAlign_Fill);
    GridSlot->SetVerticalAlignment(VAlign_Top);
    UTextBlock* SlotLabel = AddText(SlotContent, Label, 14, 5.0f);
    SlotLabel->SetJustification(ETextJustify::Center);
    USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
    IconSize->SetWidthOverride(66.0f);
    IconSize->SetHeightOverride(66.0f);
    SlotContent->AddChildToVerticalBox(IconSize)->SetHorizontalAlignment(HAlign_Center);
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleSlot(Frame);
    Frame->SetPadding(FMargin(12.0f));
    IconSize->SetContent(Frame);
    UImage* Icon = WidgetTree->ConstructWidget<UImage>();
    Theme.SetEquipmentIcon(Icon, SlotId);
    Icon->SetColorAndOpacity(FLinearColor(0.62f, 0.56f, 0.46f, 0.72f));
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Frame->SetContent(Icon);
    UTextBlock* EmptyLabel = AddText(SlotContent, NSLOCTEXT("Equipment", "Empty", "미장착"), 12, 8.0f);
    EmptyLabel->SetJustification(ETextJustify::Center);
}

void UCharacterEquipmentPanel::RefreshEquipment(const FGameplayViewState& View, FGuid CharacterId)
{
    if (!CharacterText || !StatusText || !EquipmentSlots) return;
    const FRunPartyMember* Member = CharacterId.IsValid() ? View.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId && Candidate.bCreated; }) : nullptr;
    EquipmentSlots->SetRenderOpacity(Member ? 1.0f : 0.4f);
    if (!Member)
    {
        CharacterText->SetText(NSLOCTEXT("Equipment", "NoCharacter", "직접 조작 캐릭터가 없습니다."));
        StatusText->SetText(NSLOCTEXT("Equipment", "NoEquipment", "표시할 캐릭터 장비가 없습니다."));
        return;
    }

    const UProfessionBase* Profession = UProfessionBase::FindProfession(Member->ClassId);
    const FText ClassName = Profession ? Profession->DisplayName : FText::FromName(Member->ClassId);
    const FText CharacterName = Member->CharacterName.IsEmpty() ? ClassName : Member->CharacterName;
    CharacterText->SetText(FText::Format(NSLOCTEXT("Equipment", "Character", "{0}\n{1}"), CharacterName, ClassName));
    StatusText->SetText(Member->CurrentHP == 0.0f ? NSLOCTEXT("Equipment", "Dead", "사망 · 보관 정보 보기") : NSLOCTEXT("Equipment", "NoEquippedItems", "장착된 아이템 없음"));
}
