#include "UI/Debug/CombatDebugWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "UI/Theme/DemonicUITheme.h"
#include "Unit/UnitBase.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UCombatDebugActionButton::Initialize(ECombatDebugAction InAction, const FSoftObjectPath& InAsset, int32 InIndex, FGameplayTag InSlot)
{
    Action = InAction;
    Asset = InAsset;
    Index = InIndex;
    EquipmentSlot = InSlot;
    OnClicked.AddUniqueDynamic(this, &UCombatDebugActionButton::HandleClicked);
}

void UCombatDebugActionButton::HandleClicked()
{
    OnAction.Broadcast(this);
}

UTextBlock* UCombatDebugWidget::AddText(UVerticalBox* Box, const FString& Text, int32 Size)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, Size >= 18, Size);
    Box->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 3.f));
    return Label;
}

UButton* UCombatDebugWidget::AddButton(UVerticalBox* Box, const FString& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    UDemonicUITheme::Get().StyleText(Label, false, 14);
    UDemonicUITheme::Get().StyleButton(Button);
    Button->SetContent(Label);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 2.f));
    return Button;
}

void UCombatDebugWidget::AddAction(UVerticalBox* Box, const FString& Label, ECombatDebugAction Action, const FSoftObjectPath& Asset, int32 Index, FGameplayTag EquipmentSlot)
{
    UCombatDebugActionButton* Button = WidgetTree->ConstructWidget<UCombatDebugActionButton>();
    Button->Initialize(Action, Asset, Index, EquipmentSlot);
    Button->OnAction.AddUObject(this, &UCombatDebugWidget::HandleAction);
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Text, false, 13);
    UDemonicUITheme::Get().StyleButton(Button);
    Button->SetContent(Text);
    if (Asset.IsValid()) Button->SetToolTipText(FText::FromString(Asset.ToString()));
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 2.f));
}

void UCombatDebugWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    UCommonActivatableWidgetStack* CombatLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
    UOverlaySlot* CombatSlot = Root->AddChildToOverlay(CombatLayer);
    CombatSlot->SetHorizontalAlignment(HAlign_Fill);
    CombatSlot->SetVerticalAlignment(VAlign_Fill);
    CombatLayer->AddWidget<UCombatRoundPlanningWidget>(UCombatRoundPlanningWidget::StaticClass());

    UVerticalBox* Tools = WidgetTree->ConstructWidget<UVerticalBox>();
    UOverlaySlot* ToolsSlot = Root->AddChildToOverlay(Tools);
    ToolsSlot->SetHorizontalAlignment(HAlign_Left);
    ToolsSlot->SetVerticalAlignment(VAlign_Top);
    ToolsSlot->SetPadding(FMargin(24.f, 16.f));
    USizeBox* ToolWidth = WidgetTree->ConstructWidget<USizeBox>();
    ToolWidth->SetWidthOverride(400.f);
    UVerticalBox* Header = WidgetTree->ConstructWidget<UVerticalBox>();
    ToolWidth->SetContent(Header);
    Tools->AddChildToVerticalBox(ToolWidth);
    AddButton(Header, TEXT("디버그 도구 열기 / 접기"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::TogglePanel);
    PanelSize = WidgetTree->ConstructWidget<USizeBox>();
    PanelSize->SetWidthOverride(400.f);
    PanelSize->SetMaxDesiredHeight(470.f);
    Tools->AddChildToVerticalBox(PanelSize);
    Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.016f, 0.022f, 0.97f), 8.f));
    Panel->SetPadding(FMargin(10.f));
    PanelSize->SetContent(Panel);
    UScrollBox* ToolScroll = WidgetTree->ConstructWidget<UScrollBox>();
    Panel->SetContent(ToolScroll);
    UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
    ToolScroll->AddChild(Body);
    AddText(Body, TEXT("전투 디버그 · 저장하지 않음"), 18);
    AddText(Body, TEXT("계획 단계에서만 변경 · 스킬 최대 5개\n장비는 외형 장착용이며 스킬을 자동 지급하지 않습니다."));
    UnitChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    Body->AddChildToVerticalBox(UnitChoice);
    UnitChoice->OnSelectionChanged.AddDynamic(this, &UCombatDebugWidget::HandleUnit);
    AddButton(Body, TEXT("전투 초기화 · 처음 장착으로 복원"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::RestartCombat);
    Status = AddText(Body, TEXT("전투 준비 중"));
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    Body->AddChildToVerticalBox(Tabs);
    UVerticalBox* SkillTab = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* EquipmentTab = WidgetTree->ConstructWidget<UVerticalBox>();
    Tabs->AddChildToHorizontalBox(SkillTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Tabs->AddChildToHorizontalBox(EquipmentTab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AddButton(SkillTab, TEXT("스킬"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::ShowSkills);
    AddButton(EquipmentTab, TEXT("장비"))->OnClicked.AddDynamic(this, &UCombatDebugWidget::ShowEquipment);
    OwnedList = WidgetTree->ConstructWidget<UVerticalBox>();
    Body->AddChildToVerticalBox(OwnedList);
    Search = WidgetTree->ConstructWidget<UEditableTextBox>();
    Search->SetHintText(FText::FromString(TEXT("이름 / 에셋 이름 검색")));
    Body->AddChildToVerticalBox(Search);
    Search->OnTextChanged.AddDynamic(this, &UCombatDebugWidget::HandleSearch);
    CatalogTitle = AddText(Body, TEXT("전체 목록"));
    CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>();
    USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
    ListSize->SetHeightOverride(240.f);
    ListSize->SetContent(CatalogScroll);
    Body->AddChildToVerticalBox(ListSize);
    CatalogList = WidgetTree->ConstructWidget<UVerticalBox>();
    CatalogScroll->AddChild(CatalogList);
    UDemonicUITheme::Get().ApplyControls(WidgetTree);
}

void UCombatDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshElapsed += InDeltaTime;
    if (RefreshElapsed < 0.15f) return;
    RefreshElapsed = 0.f;
    PanelSize->SetMaxDesiredHeight(FMath::Clamp(MyGeometry.GetLocalSize().Y - 380.f, 190.f, 540.f));
    RefreshState();
}

void UCombatDebugWidget::RefreshState()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Round)
    {
        ObservedCombatId.Invalidate();
        ObservedRevision = INDEX_NONE;
        SelectedUnitId = INDEX_NONE;
        bRefreshingUnits = true;
        UnitOptions.Reset();
        UnitChoice->ClearOptions();
        bRefreshingUnits = false;
        OwnedList->ClearChildren();
        CatalogList->ClearChildren();
        OwnedList->SetIsEnabled(false);
        CatalogList->SetIsEnabled(false);
        const ACombatDebugGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatDebugGameMode>();
        Status->SetText(Mode ? Mode->GetStatusMessage() : FText::FromString(TEXT("디버그 전투가 없습니다.")));
        return;
    }
    Controller->GetDebugLoadout();
    const FCombatRoundView& View = Round->GetView();
    if (ObservedCombatId != View.CombatId)
    {
        ObservedCombatId = View.CombatId;
        ObservedRevision = INDEX_NONE;
        bRefreshingUnits = true;
        UnitOptions.Reset();
        UnitChoice->ClearOptions();
        for (const FCombatRoundUnitView& Unit : View.Units)
        {
            if (Unit.bEnemy || Unit.OwnerSlot != Controller->GetRoundParticipantSlot() || !IsValid(Unit.Unit)) continue;
            const FString Label = FString::Printf(TEXT("%d · %s"), Unit.UnitId, *Unit.Unit->RuntimeCharacterName.ToString());
            UnitOptions.Add(Label, Unit.UnitId);
            UnitChoice->AddOption(Label);
        }
        UnitChoice->SetSelectedIndex(0);
        SelectedUnitId = UnitOptions.FindRef(UnitChoice->GetSelectedOption());
        bRefreshingUnits = false;
        ActionMessage = FText::GetEmpty();
    }
    if (ObservedRevision != View.PlanRevision)
    {
        ObservedRevision = View.PlanRevision;
        RebuildLists();
    }
    FText Error;
    const bool bCanEdit = Round->CanEditDebugUnit(Controller, SelectedUnitId, Error);
    OwnedList->SetIsEnabled(bCanEdit);
    CatalogList->SetIsEnabled(bCanEdit);
    Status->SetText(bCanEdit ? (ActionMessage.IsEmpty() ? FText::FromString(TEXT("추가·제거 즉시 반영 / 해당 유닛의 계획 해제")) : ActionMessage) : Error);
}

void UCombatDebugWidget::RebuildLists()
{
    if (!OwnedList || !CatalogList) return;
    OwnedList->ClearChildren();
    CatalogList->ClearChildren();
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    UCombatDebugLoadout* Loadout = Controller ? Controller->GetDebugLoadout() : nullptr;
    if (!Round || !Loadout) return;
    const FCombatRoundUnitView* Selected = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
    if (!Selected || !IsValid(Selected->Unit)) return;
    const FString Query = Search->GetText().ToString().TrimStartAndEnd();
    int32 Count = 0;
    if (!bEquipment)
    {
        const auto& Skills = Selected->Unit->GetEquippedSkillDataAssets();
        AddText(OwnedList, FString::Printf(TEXT("현재 스킬 %d / 5"), Skills.Num()), 18);
        for (int32 Index = 0; Index < Skills.Num(); ++Index)
        {
            if (Skills[Index]) AddAction(OwnedList, TEXT("제거 · ") + Loadout->GetSkillLabel(FSoftObjectPath(Skills[Index])).ToString(), ECombatDebugAction::RemoveSkill, FSoftObjectPath(Skills[Index]), Index);
        }
        for (const FSoftObjectPath& Asset : Loadout->GetSkillAssets())
        {
            const FString DisplayName = Loadout->GetSkillLabel(Asset).ToString();
            if (!Query.IsEmpty() && !Asset.GetAssetName().Contains(Query) && !DisplayName.Contains(Query)) continue;
            const bool bOwned = Skills.ContainsByPredicate([&Asset](const USkillDefinitionDataAsset* Skill) { return Skill && FSoftObjectPath(Skill) == Asset; });
            if (bOwned) continue;
            AddAction(CatalogList, TEXT("추가 · ") + DisplayName + TEXT("\n") + Asset.GetAssetName(), ECombatDebugAction::AddSkill, Asset);
            ++Count;
        }
    }
    else
    {
        AddText(OwnedList, TEXT("현재 보유 장비 · 제거 시 장착도 해제"), 18);
        const FRunPartyMember* Member = Loadout->GetEquipmentMember(SelectedUnitId);
        if (Member)
        {
            for (int32 Index = 0; Index < Member->Items.Num(); ++Index)
            {
                FString Slots;
                for (const FRunEquipmentSlot& EquippedSlot : Member->Equipment.Slots)
                {
                    if (EquippedSlot.ItemIndex == Index) Slots += EquippedSlot.SlotTag == URunEquipmentCatalog::GetWeaponSlot(0) ? TEXT(" [무기 1]") : TEXT(" [무기 2]");
                }
                AddAction(OwnedList, TEXT("제거 · ") + Member->Items[Index].DisplayName.ToString() + Slots, ECombatDebugAction::RemoveEquipment, Member->Items[Index].Asset, Index);
            }
        }
        for (const FRunItemDefinition& Item : Loadout->GetEquipmentItems())
        {
            if (!Query.IsEmpty() && !Item.DisplayName.ToString().Contains(Query) && !Item.Asset.GetAssetName().Contains(Query)) continue;
            const FRunEquipmentProfile* Profile = URunEquipmentCatalog::Get().ResolveProfile(Item);
            if (!Profile) continue;
            AddText(CatalogList, Item.DisplayName.ToString() + TEXT(" · ") + Item.Asset.GetAssetName());
            for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
            {
                const FGameplayTag WeaponSlot = URunEquipmentCatalog::GetWeaponSlot(SlotIndex);
                if (URunEquipmentCatalog::ResolveSlot(*Profile, WeaponSlot).IsValid()) AddAction(CatalogList, FString::Printf(TEXT("획득 + 무기 %d 장착"), SlotIndex + 1), ECombatDebugAction::GrantEquipment, Item.Asset, INDEX_NONE, WeaponSlot);
            }
            ++Count;
        }
    }
    CatalogTitle->SetText(FText::FromString(FString::Printf(TEXT("%s 목록 · %d개"), bEquipment ? TEXT("장비") : TEXT("미보유 스킬"), Count)));
}

void UCombatDebugWidget::HandleAction(UCombatDebugActionButton* Button)
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    UCombatDebugLoadout* Loadout = Controller ? Controller->GetDebugLoadout() : nullptr;
    FText Error;
    if (!Button || !Round || !Loadout || !Round->CanEditDebugUnit(Controller, SelectedUnitId, Error))
    {
        ActionMessage = Error;
        return;
    }
    bool bSucceeded = false;
    if (Button->Action == ECombatDebugAction::AddSkill || Button->Action == ECombatDebugAction::RemoveSkill)
    {
        const FCombatRoundUnitView* Selected = Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
        if (!Selected || !IsValid(Selected->Unit)) return;
        TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills = Selected->Unit->GetEquippedSkillDataAssets();
        if (Button->Action == ECombatDebugAction::AddSkill && Loadout->GetSkillAssets().Contains(Button->Asset))
        {
            if (USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Button->Asset.TryLoad())) Skills.AddUnique(Skill);
            else Error = FText::FromString(TEXT("스킬 에셋을 불러오지 못했습니다."));
        }
        else if (Skills.IsValidIndex(Button->Index) && FSoftObjectPath(Skills[Button->Index]) == Button->Asset) Skills.RemoveAt(Button->Index);
        else Error = FText::FromString(TEXT("목록이 바뀌었습니다. 다시 선택하세요."));
        if (Error.IsEmpty()) bSucceeded = Round->SetDebugUnitSkills(Controller, SelectedUnitId, Skills, Error);
    }
    else if (Button->Action == ECombatDebugAction::GrantEquipment) bSucceeded = Loadout->GrantEquipment(Controller, SelectedUnitId, Button->Asset, Button->EquipmentSlot, Error);
    else bSucceeded = Loadout->RemoveEquipment(Controller, SelectedUnitId, Button->Index, Error);
    ActionMessage = bSucceeded ? FText::FromString(TEXT("반영했습니다. 전투 화면에서 바로 확인할 수 있습니다.")) : Error;
    RebuildLists();
    RefreshState();
}

void UCombatDebugWidget::TogglePanel()
{
    PanelSize->SetVisibility(PanelSize->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UCombatDebugWidget::ShowSkills()
{
    bEquipment = false;
    CatalogScroll->ScrollToStart();
    RebuildLists();
}

void UCombatDebugWidget::ShowEquipment()
{
    bEquipment = true;
    CatalogScroll->ScrollToStart();
    RebuildLists();
}

void UCombatDebugWidget::RestartCombat()
{
    if (ACombatDebugGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatDebugGameMode>())
    {
        Mode->RestartCombat();
        ActionMessage = Mode->GetStatusMessage();
        RefreshState();
    }
}

void UCombatDebugWidget::HandleSearch(const FText& Text)
{
    CatalogScroll->ScrollToStart();
    RebuildLists();
}

void UCombatDebugWidget::HandleUnit(FString Value, ESelectInfo::Type SelectionType)
{
    if (bRefreshingUnits) return;
    SelectedUnitId = UnitOptions.FindRef(Value);
    ActionMessage = FText::GetEmpty();
    RebuildLists();
}
