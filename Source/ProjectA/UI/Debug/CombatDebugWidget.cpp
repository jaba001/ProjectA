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
#include "GAS/Attribute/AS_Unit.h"
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
    CombatLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
    UOverlaySlot* CombatSlot = Root->AddChildToOverlay(CombatLayer);
    CombatSlot->SetHorizontalAlignment(HAlign_Fill);
    CombatSlot->SetVerticalAlignment(VAlign_Fill);

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
    AddText(Body, TEXT("스킬·장비는 계획 단계에서 변경 · 스킬 최대 5개\n장비는 외형 장착용이며 스킬을 자동 지급하지 않습니다."));
    UnitChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    Body->AddChildToVerticalBox(UnitChoice);
    UnitChoice->OnSelectionChanged.AddDynamic(this, &UCombatDebugWidget::HandleUnit);
    SpawnToggleButton = AddButton(Body, TEXT("캐릭터 추가 창 열기"));
    SpawnToggleButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ToggleSpawnPanel);
    SpawnPanel = WidgetTree->ConstructWidget<UBorder>();
    SpawnPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f), 6.f));
    SpawnPanel->SetPadding(FMargin(8.f));
    SpawnPanel->SetVisibility(ESlateVisibility::Collapsed);
    Body->AddChildToVerticalBox(SpawnPanel)->SetPadding(FMargin(0.f, 3.f));
    UVerticalBox* SpawnBody = WidgetTree->ConstructWidget<UVerticalBox>();
    SpawnPanel->SetContent(SpawnBody);
    SpawnCount = AddText(SpawnBody, TEXT("전투 준비 중"));
    AddText(SpawnBody, TEXT("계획 단계 또는 전투 종료 후 추가할 수 있습니다. 기존 캐릭터의 스킬·장비는 유지합니다."), 13);
    AddText(SpawnBody, TEXT("추가할 아군"));
    AllySpawnChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    SpawnBody->AddChildToVerticalBox(AllySpawnChoice);
    AllySpawnButton = AddButton(SpawnBody, TEXT("선택 캐릭터를 아군으로 추가"));
    AllySpawnButton->SetIsEnabled(false);
    AllySpawnButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::SpawnAlly);
    AllySpawnStatus = AddText(SpawnBody, TEXT("전투 준비 중"), 13);
    AddText(SpawnBody, TEXT("추가할 적군"));
    EnemySpawnChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    SpawnBody->AddChildToVerticalBox(EnemySpawnChoice);
    EnemySpawnButton = AddButton(SpawnBody, TEXT("선택 캐릭터를 적군으로 추가"));
    EnemySpawnButton->SetIsEnabled(false);
    EnemySpawnButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::SpawnEnemy);
    EnemySpawnStatus = AddText(SpawnBody, TEXT("전투 준비 중"), 13);
    ReviveToggleButton = AddButton(Body, TEXT("아군 부활 창 열기"));
    ReviveToggleButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ToggleRevivePanel);
    // Keep the revival controls inside the outer tool scroll so a small viewport can still reach every action.
    // 작은 화면에서도 모든 조작에 접근하도록 부활 패널을 도구의 바깥 스크롤 안에 배치합니다.
    RevivePanel = WidgetTree->ConstructWidget<UBorder>();
    RevivePanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f), 6.f));
    RevivePanel->SetPadding(FMargin(8.f));
    RevivePanel->SetVisibility(ESlateVisibility::Collapsed);
    Body->AddChildToVerticalBox(RevivePanel)->SetPadding(FMargin(0.f, 3.f));
    UVerticalBox* ReviveBody = WidgetTree->ConstructWidget<UVerticalBox>();
    RevivePanel->SetContent(ReviveBody);
    ReviveTarget = AddText(ReviveBody, TEXT("위 목록에서 부활할 아군을 선택하세요."));
    ReviveButton = AddButton(ReviveBody, TEXT("선택 아군 부활 · HP 전부 회복"));
    ReviveButton->SetIsEnabled(false);
    ReviveButton->OnClicked.AddDynamic(this, &UCombatDebugWidget::ReviveSelectedAlly);
    ReviveStatus = AddText(ReviveBody, TEXT("전투 준비 중"), 13);
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

void UCombatDebugWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Add content after the Slate switcher exists, and restore it after Slate resources are rebuilt.
    // Slate 스위처가 생성된 뒤 콘텐츠를 추가하고 Slate 리소스 재생성 후에도 복원합니다.
    if (CombatLayer && CombatLayer->GetNumWidgets() == 0) CombatLayer->AddWidget<UCombatRoundPlanningWidget>(UCombatRoundPlanningWidget::StaticClass());
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
        ReviveMessage = FText::GetEmpty();
        AllySpawnMessage = FText::GetEmpty();
        EnemySpawnMessage = FText::GetEmpty();
        RefreshReviveState();
        RefreshSpawnState();
        return;
    }
    Controller->GetDebugLoadout();
    const FCombatRoundView& View = Round->GetView();
    if (ObservedCombatId != View.CombatId)
    {
        ObservedCombatId = View.CombatId;
        ObservedRevision = INDEX_NONE;
        SelectedUnitId = INDEX_NONE;
        ActionMessage = FText::GetEmpty();
        ReviveMessage = FText::GetEmpty();
        AllySpawnMessage = FText::GetEmpty();
        EnemySpawnMessage = FText::GetEmpty();
        RebuildSpawnOptions();
    }
    const bool bUnitsChanged = RefreshUnitOptions();
    if (bUnitsChanged || ObservedRevision != View.PlanRevision)
    {
        ObservedRevision = View.PlanRevision;
        RebuildLists();
    }
    FText Error;
    const bool bCanEdit = Round->CanEditDebugUnit(Controller, SelectedUnitId, Error);
    OwnedList->SetIsEnabled(bCanEdit);
    CatalogList->SetIsEnabled(bCanEdit);
    Status->SetText(bCanEdit ? (ActionMessage.IsEmpty() ? FText::FromString(TEXT("추가·제거 즉시 반영 / 해당 유닛의 계획 해제")) : ActionMessage) : Error);
    RefreshReviveState();
    RefreshSpawnState();
}

bool UCombatDebugWidget::RefreshUnitOptions()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Round) return false;
    TMap<FString, int32> AvailableOptions;
    TArray<FString> Labels;
    FString SelectedLabel;
    bool bChanged = false;
    for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
    {
        if (Unit.bEnemy || Unit.OwnerSlot != Controller->GetRoundParticipantSlot() || !IsValid(Unit.Unit)) continue;
        const FString Label = FString::Printf(TEXT("%d · %s"), Unit.UnitId, *Unit.Unit->RuntimeCharacterName.ToString());
        AvailableOptions.Add(Label, Unit.UnitId);
        Labels.Add(Label);
        const int32* PreviousId = UnitOptions.Find(Label);
        bChanged |= !PreviousId || *PreviousId != Unit.UnitId;
        if (Unit.UnitId == SelectedUnitId) SelectedLabel = Label;
    }
    bChanged |= AvailableOptions.Num() != UnitOptions.Num();
    if (!bChanged && !SelectedLabel.IsEmpty() && UnitChoice->GetSelectedOption() == SelectedLabel) return false;
    // Spawning changes the roster without creating another combat; retain the selected ally when it still exists.
    // 캐릭터 추가는 새 전투를 만들지 않으므로 같은 전투의 명단도 갱신하고 기존 아군 선택을 유지합니다.
    bRefreshingUnits = true;
    UnitOptions = MoveTemp(AvailableOptions);
    UnitChoice->ClearOptions();
    for (const FString& Label : Labels) UnitChoice->AddOption(Label);
    if (SelectedLabel.IsEmpty() && !Labels.IsEmpty()) SelectedLabel = Labels[0];
    if (!SelectedLabel.IsEmpty()) UnitChoice->SetSelectedOption(SelectedLabel);
    const int32* NewSelectedId = UnitOptions.Find(SelectedLabel);
    SelectedUnitId = NewSelectedId ? *NewSelectedId : INDEX_NONE;
    bRefreshingUnits = false;
    return true;
}

void UCombatDebugWidget::RefreshReviveState()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundUnitView* Selected = Round ? Round->GetView().Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; }) : nullptr;
    if (!Selected || !IsValid(Selected->Unit))
    {
        ReviveTarget->SetText(FText::FromString(TEXT("위 목록에서 부활할 아군을 선택하세요.")));
        ReviveButton->SetIsEnabled(false);
        ReviveStatus->SetText(FText::FromString(Round ? TEXT("선택한 아군이 없습니다.") : TEXT("디버그 전투 준비 후 사용할 수 있습니다.")));
        ReviveButton->SetToolTipText(ReviveStatus->GetText());
        return;
    }
    const UAS_Unit* Attributes = Selected->Unit->GetAttributeSet();
    const FString Health = Attributes ? FString::Printf(TEXT("HP %.0f / %.0f"), Attributes->GetHP(), Attributes->GetMaxHP()) : TEXT("HP 정보 없음");
    ReviveTarget->SetText(FText::FromString(FString::Printf(TEXT("부활 대상: %d · %s\n%s · %s"), Selected->UnitId, *Selected->Unit->RuntimeCharacterName.ToString(), *Health, Selected->Unit->IsUnitAlive() ? TEXT("생존") : TEXT("사망"))));
    FText Error;
    const bool bCanRevive = Round->CanReviveDebugUnit(Controller, SelectedUnitId, Error);
    ReviveButton->SetIsEnabled(bCanRevive);
    const FText Availability = bCanRevive ? FText::FromString(TEXT("선택 아군의 스킬·장비를 유지한 채 HP를 전부 회복하고 부활합니다.")) : Error;
    ReviveButton->SetToolTipText(Availability);
    // Preserve revival feedback separately from ordinary loadout-edit errors for dead or finished units.
    // 사망하거나 전투가 끝난 유닛의 일반 장착 편집 오류와 부활 결과를 별도로 유지합니다.
    ReviveStatus->SetText(ReviveMessage.IsEmpty() || ReviveMessage.EqualTo(Availability) ? Availability : bCanRevive ? ReviveMessage : FText::Format(FText::FromString(TEXT("{0}\n{1}")), ReviveMessage, Availability));
}

void UCombatDebugWidget::RebuildSpawnOptions()
{
    const ACombatDebugGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatDebugGameMode>() : nullptr;
    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        const bool bEnemy = TeamIndex == 1;
        UComboBoxString* Choice = bEnemy ? EnemySpawnChoice : AllySpawnChoice;
        TMap<FString, FName>& Options = bEnemy ? EnemySpawnOptions : AllySpawnOptions;
        const FName SelectedId = Options.FindRef(Choice->GetSelectedOption());
        Options.Reset();
        Choice->ClearOptions();
        TArray<FName> Ids;
        TArray<FText> Names;
        if (Mode) Mode->GetDebugSpawnOptions(bEnemy, Ids, Names);
        FString SelectedLabel;
        for (int32 Index = 0; Index < FMath::Min(Ids.Num(), Names.Num()); ++Index)
        {
            if (Ids[Index].IsNone()) continue;
            const FString Label = FString::Printf(TEXT("%d · %s"), Index + 1, *Names[Index].ToString());
            Options.Add(Label, Ids[Index]);
            Choice->AddOption(Label);
            if (Ids[Index] == SelectedId) SelectedLabel = Label;
        }
        if (!SelectedLabel.IsEmpty()) Choice->SetSelectedOption(SelectedLabel);
        else if (Choice->GetOptionCount() > 0) Choice->SetSelectedIndex(0);
    }
}

void UCombatDebugWidget::RefreshSpawnState()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const ACombatDebugGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatDebugGameMode>() : nullptr;
    int32 AllyCount = 0;
    int32 EnemyCount = 0;
    if (Round)
    {
        for (const FCombatRoundUnitView& Unit : Round->GetView().Units)
        {
            if (Unit.bEnemy) ++EnemyCount;
            else ++AllyCount;
        }
    }
    SpawnCount->SetText(FText::FromString(FString::Printf(TEXT("전체 캐릭터 %d / 8 · 아군 %d · 적군 %d\n사망한 캐릭터도 인원에 포함됩니다."), AllyCount + EnemyCount, AllyCount, EnemyCount)));
    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        const bool bEnemy = TeamIndex == 1;
        UComboBoxString* Choice = bEnemy ? EnemySpawnChoice : AllySpawnChoice;
        UButton* Button = bEnemy ? EnemySpawnButton : AllySpawnButton;
        UTextBlock* Result = bEnemy ? EnemySpawnStatus : AllySpawnStatus;
        const TMap<FString, FName>& Options = bEnemy ? EnemySpawnOptions : AllySpawnOptions;
        const FText& Message = bEnemy ? EnemySpawnMessage : AllySpawnMessage;
        FText Error;
        bool bCanSpawn = Mode && Mode->CanSpawnDebugUnit(Controller, bEnemy, Error);
        if (!Mode) Error = FText::FromString(TEXT("디버그 전투 준비 후 사용할 수 있습니다."));
        if (bCanSpawn && !Options.Contains(Choice->GetSelectedOption()))
        {
            bCanSpawn = false;
            Error = FText::FromString(TEXT("추가할 캐릭터가 없습니다."));
        }
        Button->SetIsEnabled(bCanSpawn);
        Choice->SetIsEnabled(!Options.IsEmpty());
        const FText Availability = bCanSpawn ? FText::FromString(bEnemy ? TEXT("선택한 적군을 빈 적 진영 칸에 추가합니다.") : TEXT("선택한 아군을 빈 아군 진영 칸에 추가하고 장착 대상으로 선택합니다.")) : Error;
        Button->SetToolTipText(Availability);
        Result->SetText(Message.IsEmpty() || Message.EqualTo(Availability) ? Availability : bCanSpawn ? Message : FText::Format(FText::FromString(TEXT("{0}\n{1}")), Message, Availability));
    }
}

void UCombatDebugWidget::SpawnUnit(bool bEnemy)
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatDebugGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatDebugGameMode>() : nullptr;
    UComboBoxString* Choice = bEnemy ? EnemySpawnChoice : AllySpawnChoice;
    const TMap<FString, FName>& Options = bEnemy ? EnemySpawnOptions : AllySpawnOptions;
    const FName* OptionId = Options.Find(Choice->GetSelectedOption());
    FText& Message = bEnemy ? EnemySpawnMessage : AllySpawnMessage;
    int32 NewUnitId = INDEX_NONE;
    FText Error;
    if (!Mode || !OptionId)
    {
        Message = FText::FromString(TEXT("추가할 캐릭터를 선택하세요."));
        RefreshSpawnState();
        return;
    }
    if (Mode->SpawnDebugUnit(Controller, bEnemy, *OptionId, NewUnitId, Error))
    {
        Message = FText::FromString(FString::Printf(TEXT("%s 추가 완료 · %s"), bEnemy ? TEXT("적군") : TEXT("아군"), *Choice->GetSelectedOption()));
        if (!bEnemy)
        {
            SelectedUnitId = NewUnitId;
            ActionMessage = FText::GetEmpty();
            ReviveMessage = FText::GetEmpty();
        }
        ObservedRevision = INDEX_NONE;
    }
    else Message = Error;
    RefreshState();
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

void UCombatDebugWidget::ToggleRevivePanel()
{
    const bool bOpen = RevivePanel->GetVisibility() == ESlateVisibility::Collapsed;
    RevivePanel->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (UTextBlock* Label = Cast<UTextBlock>(ReviveToggleButton->GetContent())) Label->SetText(FText::FromString(bOpen ? TEXT("아군 부활 창 접기") : TEXT("아군 부활 창 열기")));
    RefreshReviveState();
}

void UCombatDebugWidget::ReviveSelectedAlly()
{
    ACombatDebugPlayerController* Controller = GetOwningPlayer<ACombatDebugPlayerController>();
    ACombatRoundCoordinator* Round = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Round) return;
    FText Error;
    const bool bSucceeded = Round->ReviveDebugUnit(Controller, SelectedUnitId, Error);
    ReviveMessage = bSucceeded ? Round->GetView().Message : Error;
    RebuildLists();
    RefreshState();
}

void UCombatDebugWidget::ToggleSpawnPanel()
{
    const bool bOpen = SpawnPanel->GetVisibility() == ESlateVisibility::Collapsed;
    SpawnPanel->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (UTextBlock* Label = Cast<UTextBlock>(SpawnToggleButton->GetContent())) Label->SetText(FText::FromString(bOpen ? TEXT("캐릭터 추가 창 접기") : TEXT("캐릭터 추가 창 열기")));
    if (bOpen) RebuildSpawnOptions();
    RefreshSpawnState();
}

void UCombatDebugWidget::SpawnAlly()
{
    SpawnUnit(false);
}

void UCombatDebugWidget::SpawnEnemy()
{
    SpawnUnit(true);
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
        ReviveMessage = FText::GetEmpty();
        AllySpawnMessage = FText::GetEmpty();
        EnemySpawnMessage = FText::GetEmpty();
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
    const int32* UnitId = UnitOptions.Find(Value);
    SelectedUnitId = UnitId ? *UnitId : INDEX_NONE;
    ActionMessage = FText::GetEmpty();
    ReviveMessage = FText::GetEmpty();
    RebuildLists();
    RefreshReviveState();
}
