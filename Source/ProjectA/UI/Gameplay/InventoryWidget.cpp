#include "UI/Gameplay/InventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Profession/ProfessionBase.h"
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

UTextBlock* UInventoryWidget::AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 22, FontSize);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    return Label;
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

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(680.0f);
    Size->SetHeightOverride(620.0f);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Size);
    ContentSlot->SetHorizontalAlignment(HAlign_Center);
    ContentSlot->SetVerticalAlignment(VAlign_Center);
    ContentSlot->SetPadding(FMargin(24.0f));
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryPanel"));
    Theme.StylePanel(Panel);
    Panel->SetPadding(FMargin(32.0f));
    Size->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);
    AddText(Content, NSLOCTEXT("Inventory", "Title", "인벤토리"), 30, 12.0f);
    CharacterText = AddText(Content, FText::GetEmpty(), 22);
    GoldText = AddText(Content, FText::GetEmpty(), 20);
    Theme.AddDivider(WidgetTree, Content);
    AddText(Content, NSLOCTEXT("Inventory", "Skills", "보유 · 장착 스킬"), 22);
    StatusText = AddText(Content, FText::GetEmpty(), 16);

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
    Content->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SkillList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventorySkills"));
    Scroll->AddChild(SkillList);
    AddText(Content, NSLOCTEXT("Inventory", "Shortcuts", "I · 인벤토리 닫기    Esc · 설정 열기"), 16, 12.0f);

    CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_CloseInventory"));
    UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CloseLabel->SetText(NSLOCTEXT("Inventory", "Close", "닫기"));
    CloseLabel->SetJustification(ETextJustify::Center);
    CastChecked<UButtonSlot>(CloseButton->AddChild(CloseLabel))->SetPadding(FMargin(20.0f, 10.0f));
    Content->AddChildToVerticalBox(CloseButton)->SetHorizontalAlignment(HAlign_Right);
    CloseButton->OnClicked.AddUniqueDynamic(this, &UInventoryWidget::HandleClose);
    Theme.ApplyControls(WidgetTree);
    Theme.StyleButton(CloseButton, true);
}

void UInventoryWidget::AddSkill(const USkillDefinitionDataAsset* Skill)
{
    if (!Skill)
    {
        AddText(SkillList, NSLOCTEXT("Inventory", "MissingSkill", "스킬 정보를 불러올 수 없습니다."), 18, 16.0f);
        return;
    }

    const FText Name = Skill->SkillName.IsEmpty() ? FText::FromName(Skill->SkillId) : Skill->SkillName;
    AddText(SkillList, FText::Format(NSLOCTEXT("Inventory", "Skill", "{0} · {1}"), Name, Skill->GetActionPointCostText()), 20, 4.0f);
    if (!Skill->SkillDescription.IsEmpty()) AddText(SkillList, Skill->SkillDescription, 16, 12.0f);
    UDemonicUITheme::Get().AddDivider(WidgetTree, SkillList);
}

void UInventoryWidget::RefreshInventory(const FGameplayViewState& View, FGuid CharacterId)
{
    if (!SkillList) return;
    SkillList->ClearChildren();
    const FRunPartyMember* Member = CharacterId.IsValid() ? View.PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId && Candidate.bCreated; }) : nullptr;
    GoldText->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StatusText->SetVisibility(ESlateVisibility::Visible);
    if (!Member)
    {
        CharacterText->SetText(NSLOCTEXT("Inventory", "NoCharacter", "직접 조작 캐릭터가 없습니다."));
        GoldText->SetText(FText::GetEmpty());
        StatusText->SetText(NSLOCTEXT("Inventory", "NoInventory", "표시할 인벤토리가 없습니다."));
        return;
    }

    const UProfessionBase* Profession = UProfessionBase::FindProfession(Member->ClassId);
    const FText ClassName = Profession ? Profession->DisplayName : FText::FromName(Member->ClassId);
    const FText CharacterName = Member->CharacterName.IsEmpty() ? ClassName : Member->CharacterName;
    CharacterText->SetText(FText::Format(NSLOCTEXT("Inventory", "Character", "{0} · {1}"), CharacterName, ClassName));
    GoldText->SetText(FText::Format(NSLOCTEXT("Inventory", "Gold", "보유 골드 {0}G"), FText::AsNumber(Member->Gold)));
    StatusText->SetText(FText::GetEmpty());
    StatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (Member->bHasSkillLoadout)
    {
        if (Member->Skills.IsEmpty()) AddText(SkillList, NSLOCTEXT("Inventory", "EmptySkills", "보유한 스킬이 없습니다."), 18);
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
    StatusText->SetVisibility(ESlateVisibility::Visible);
    if (!Catalog || !Catalog->ResolveMemberSkills(*Member, Skills, Error))
    {
        StatusText->SetText(NSLOCTEXT("Inventory", "LegacyUnavailable", "이전 저장의 장착 스킬 정보를 불러올 수 없습니다."));
        return;
    }
    StatusText->SetText(NSLOCTEXT("Inventory", "LegacySkills", "이전 저장의 직업 기본 장착 스킬입니다."));
    for (const USkillDefinitionDataAsset* Skill : Skills) AddSkill(Skill);
    if (Skills.IsEmpty()) AddText(SkillList, NSLOCTEXT("Inventory", "EmptySkills", "보유한 스킬이 없습니다."), 18);
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
