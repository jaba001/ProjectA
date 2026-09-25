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
#include "Controller/GameplayPlayerController.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Profession/ProfessionBase.h"
#include "UI/Gameplay/EquipmentDragDropOperation.h"
#include "UI/Gameplay/EquipmentItemSlotWidget.h"
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

    const TArray<FGameplayTag> Tags = URunEquipmentCatalog::GetSlotTags();
    AddEquipmentSlot(Tags[0], TEXT("Weapon"), NSLOCTEXT("Equipment", "MainHand", "주 무기"), 0, 0);
    AddEquipmentSlot(Tags[2], TEXT("Head"), NSLOCTEXT("Equipment", "Head", "투구"), 0, 1);
    AddEquipmentSlot(Tags[1], TEXT("Weapon"), NSLOCTEXT("Equipment", "OffHand", "보조 무기"), 0, 2);
    AddEquipmentSlot(Tags[3], TEXT("Hands"), NSLOCTEXT("Equipment", "Hands", "장갑"), 1, 0);
    AddEquipmentSlot(Tags[5], TEXT("Body"), NSLOCTEXT("Equipment", "Body", "갑옷"), 1, 1);
    AddEquipmentSlot(Tags[6], TEXT("Neck"), NSLOCTEXT("Equipment", "Neck", "목걸이"), 1, 2);
    AddEquipmentSlot(Tags[7], TEXT("Ring"), NSLOCTEXT("Equipment", "RingOne", "반지 1"), 2, 0);
    AddEquipmentSlot(Tags[4], TEXT("Feet"), NSLOCTEXT("Equipment", "Feet", "신발"), 2, 1);
    AddEquipmentSlot(Tags[8], TEXT("Ring"), NSLOCTEXT("Equipment", "RingTwo", "반지 2"), 2, 2);
    HintText = AddText(EquipmentContent, FText::GetEmpty(), 15, 8.0f);
    Theme.ApplyControls(WidgetTree);
}

void UCharacterEquipmentPanel::AddEquipmentSlot(FGameplayTag SlotTag, FName SlotId, const FText& Label, int32 Row, int32 Column)
{
    UEquipmentItemSlotWidget* EquipmentWidget = CreateWidget<UEquipmentItemSlotWidget>(GetOwningPlayer());
    EquipmentWidget->CanAcceptDrop.BindUObject(this, &UCharacterEquipmentPanel::CanAcceptDrop);
    EquipmentWidget->ReceiveDrop.BindUObject(this, &UCharacterEquipmentPanel::HandleDrop);
    EquipmentWidget->RefreshSlot(FGuid(), 0, INDEX_NONE, nullptr, SlotTag, SlotId, Label, false);
    UUniformGridSlot* GridSlot = EquipmentSlots->AddChildToUniformGrid(EquipmentWidget, Row, Column);
    GridSlot->SetHorizontalAlignment(HAlign_Fill);
    GridSlot->SetVerticalAlignment(VAlign_Top);
    SlotWidgets.Add(EquipmentWidget);
    SlotTags.Add(SlotTag);
    SlotIcons.Add(SlotId);
    SlotLabels.Add(Label);
}

void UCharacterEquipmentPanel::RefreshEquipment(const FGameplayViewState& View, FGuid CharacterId)
{
    if (!CharacterText || !StatusText || !EquipmentSlots) return;
    const FRunPartyMember* Member = CharacterId.IsValid() ? View.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId && Candidate.bCreated; }) : nullptr;
    const AGameplayPlayerController* Controller = GetOwningPlayer<AGameplayPlayerController>();
    DisplayedMember = Member ? *Member : FRunPartyMember();
    bCanChangeEquipment = Member && Controller && Controller->CanChangeEquipment(View, CharacterId);
    for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
    {
        const int32 ItemIndex = Member ? RunEquipmentRules::FindItemIndexAtSlot(*Member, SlotTags[Index]) : INDEX_NONE;
        const FRunItemDefinition* Item = Member && Member->Items.IsValidIndex(ItemIndex) ? &Member->Items[ItemIndex] : nullptr;
        SlotWidgets[Index]->RefreshSlot(CharacterId, DisplayedMember.Equipment.Revision, ItemIndex, Item, SlotTags[Index], SlotIcons[Index], SlotLabels[Index], bCanChangeEquipment);
    }
    HintText->SetText(bCanChangeEquipment ? NSLOCTEXT("Equipment", "DragHint", "아이템을 슬롯으로 끌어 장착·교체하세요.\n가방으로 끌면 해제됩니다.\n양손 장비는 두 무기 슬롯을 사용합니다.") : NSLOCTEXT("Equipment", "ReadOnlyHint", "장비 변경은 상점에서만 가능합니다."));
    if (Controller && Controller->IsEquipmentChangePending()) HintText->SetText(NSLOCTEXT("Equipment", "Pending", "장비 변경을 저장하고 있습니다."));
    else if (Controller && !Controller->GetEquipmentMessage().IsEmpty()) HintText->SetText(Controller->GetEquipmentMessage());
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
    TSet<int32> EquippedItems;
    for (const FRunEquipmentSlot& EquippedSlot : Member->Equipment.Slots) if (Member->Items.IsValidIndex(EquippedSlot.ItemIndex)) EquippedItems.Add(EquippedSlot.ItemIndex);
    StatusText->SetText(!Member->Equipment.bHasLoadout ? NSLOCTEXT("Equipment", "LegacyEquipment", "이전 저장 · 장비 정보 없음") : Member->CurrentHP == 0.0f ? NSLOCTEXT("Equipment", "Dead", "사망 · 보관 정보 보기") : FText::Format(NSLOCTEXT("Equipment", "EquippedCount", "장착 아이템 {0}개"), FText::AsNumber(EquippedItems.Num())));
}

bool UCharacterEquipmentPanel::CanAcceptDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot) const
{
    FText Error;
    return FEquipmentDropRequest::CanDrop(GetOwningPlayer<AGameplayPlayerController>(), DisplayedMember, bCanChangeEquipment, Operation, TargetSlot, Error);
}

bool UCharacterEquipmentPanel::HandleDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot)
{
    FText Error;
    if (!FEquipmentDropRequest::Submit(GetOwningPlayer<AGameplayPlayerController>(), DisplayedMember, bCanChangeEquipment, Operation, TargetSlot, Error)) HintText->SetText(Error);
    return Operation != nullptr;
}
