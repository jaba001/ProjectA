#include "UI/Combat/CombatRoundPlanningWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Controller/CombatRoundPlayerController.h"
#include "Game/Encounter/CombatArena.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "UI/Theme/DemonicUITheme.h"
#include "UI/Debug/CombatUnitHealthDebugWidget.h"
#include "Unit/UnitBase.h"

namespace
{
    FString RoundPhaseName(ECombatRoundPhase Phase)
    {
        switch (Phase)
        {
        case ECombatRoundPhase::WaitingForPlayers: return TEXT("참가자 대기");
        case ECombatRoundPhase::Planning: return TEXT("행동 선택");
        case ECombatRoundPhase::Resolving: return TEXT("전투 진행");
        case ECombatRoundPhase::Finished: return TEXT("전투 종료");
        case ECombatRoundPhase::Suspended: return TEXT("세션 중단");
        default: return TEXT("준비 중");
        }
    }

    FString UnitLabel(const FCombatRoundUnitView& Unit)
    {
        if (IsValid(Unit.Unit) && !Unit.Unit->RuntimeCharacterName.IsEmpty()) return Unit.Unit->RuntimeCharacterName.ToString();
        return FString::Printf(TEXT("%s #%d"), Unit.bEnemy ? TEXT("적") : TEXT("아군"), Unit.UnitId);
    }
}

void UCombatRoundSkillButton::InitializeSkill(FName InSkillId)
{
    SkillId = InSkillId;
    OnClicked.AddUniqueDynamic(this, &UCombatRoundSkillButton::HandleClicked);
}

void UCombatRoundSkillButton::HandleClicked()
{
    OnSkillPicked.Broadcast(SkillId);
}

TOptional<FUIInputConfig> UCombatRoundPlanningWidget::GetDesiredInputConfig() const
{
    // Temporary capture forwards the first viewport mouse-down and releases on mouse-up.
    // 임시 캡처로 최초 뷰포트 마우스 누름을 전달하고 버튼을 놓으면 캡처를 해제합니다.
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false);
}

UTextBlock* UCombatRoundPlanningWidget::AddText(UVerticalBox* Box, const FString& Text, int32 FontSize)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 18, FontSize);
    Box->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 3.f));
    return Label;
}

UButton* UCombatRoundPlanningWidget::AddButton(UVerticalBox* Box, const FString& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    UDemonicUITheme::Get().StyleText(Label, false, 16);
    UDemonicUITheme::Get().StyleButton(Button);
    Button->SetContent(Label);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 4.f));
    return Button;
}

void UCombatRoundPlanningWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    UCombatUnitHealthDebugWidget* UnitHealth = WidgetTree->ConstructWidget<UCombatUnitHealthDebugWidget>();
    UnitHealth->SetOwningPlayer(GetOwningPlayer());
    UOverlaySlot* HealthSlot = Root->AddChildToOverlay(UnitHealth);
    HealthSlot->SetHorizontalAlignment(HAlign_Fill);
    HealthSlot->SetVerticalAlignment(VAlign_Fill);
#endif

    // Keep the battlefield open between separately bounded information and command panels.
    // 정보와 조작 패널의 크기를 각각 제한하여 패널 사이 전장 영역을 비워 둡니다.
    const auto MakePanel = [this](float Width, float MaxHeight, bool bInteractive, UVerticalBox*& Content)
    {
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
        if (Width > 0.f) Size->SetWidthOverride(Width);
        Size->SetMaxDesiredHeight(MaxHeight);
        Size->SetVisibility(bInteractive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
        UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
        Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.016f, 0.022f, 0.92f), 8.f, FLinearColor(0.3f, 0.25f, 0.18f, 0.85f), 1.f));
        Panel->SetPadding(FMargin(14.f, 10.f));
        Size->SetContent(Panel);
        Content = WidgetTree->ConstructWidget<UVerticalBox>();
        if (bInteractive)
        {
            UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
            Panel->SetContent(Scroll);
            Scroll->AddChild(Content);
        }
        else Panel->SetContent(Content);
        return Size;
    };

    UVerticalBox* RosterBox = nullptr;
    USizeBox* RosterSize = MakePanel(620.f, 104.f, false, RosterBox);
    UOverlaySlot* RosterSlot = Root->AddChildToOverlay(RosterSize);
    RosterSlot->SetHorizontalAlignment(HAlign_Center);
    RosterSlot->SetVerticalAlignment(VAlign_Top);
    RosterSlot->SetPadding(FMargin(24.f, 16.f));
    Header = AddText(RosterBox, TEXT("라운드 전투"), 20);
    Roster = AddText(RosterBox, FString(), 14);
    Header->SetJustification(ETextJustify::Center);
    Roster->SetJustification(ETextJustify::Center);

    UVerticalBox* EnemyBox = nullptr;
    USizeBox* EnemySize = MakePanel(340.f, 280.f, true, EnemyBox);
    UOverlaySlot* EnemySlot = Root->AddChildToOverlay(EnemySize);
    EnemySlot->SetHorizontalAlignment(HAlign_Right);
    EnemySlot->SetVerticalAlignment(VAlign_Top);
    // Reserve the upper-right corner for the existing cooperative session controls.
    // 기존 협동 세션 조작을 위해 오른쪽 위 모서리에 여유 공간을 둡니다.
    EnemySlot->SetPadding(FMargin(24.f, 152.f, 24.f, 0.f));
    AddText(EnemyBox, TEXT("대상 정보"), 18);
    TargetDetails = AddText(EnemyBox, TEXT("공격할 적을 클릭하세요."), 16);
    EnemyRoster = AddText(EnemyBox, FString(), 14);

    UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    BottomRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UOverlaySlot* BottomSlot = Root->AddChildToOverlay(BottomRow);
    BottomSlot->SetHorizontalAlignment(HAlign_Fill);
    BottomSlot->SetVerticalAlignment(VAlign_Bottom);
    BottomSlot->SetPadding(FMargin(24.f, 0.f, 24.f, 24.f));

    UVerticalBox* PartyBox = nullptr;
    USizeBox* PartySize = MakePanel(620.f, 280.f, true, PartyBox);
    UHorizontalBoxSlot* PartySlot = BottomRow->AddChildToHorizontalBox(PartySize);
    PartySlot->SetVerticalAlignment(VAlign_Bottom);
    PartySlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
    AddText(PartyBox, TEXT("파티 현황"), 18);
    PartyList = WidgetTree->ConstructWidget<UHorizontalBox>();
    PartyBox->AddChildToVerticalBox(PartyList);

    UVerticalBox* SkillsBox = nullptr;
    USizeBox* SkillsSize = MakePanel(0.f, 300.f, true, SkillsBox);
    UHorizontalBoxSlot* SkillsSlot = BottomRow->AddChildToHorizontalBox(SkillsSize);
    SkillsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SkillsSlot->SetVerticalAlignment(VAlign_Bottom);
    SkillsSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
    UnitDetails = AddText(SkillsBox, TEXT("조작할 아군"), 18);
    UnitChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), TEXT("ControlledUnitChoice"));
    SkillsBox->AddChildToVerticalBox(UnitChoice);
    UnitChoice->OnSelectionChanged.AddDynamic(this, &UCombatRoundPlanningWidget::HandleUnitChanged);
    SkillList = WidgetTree->ConstructWidget<UWrapBox>();
    SkillList->SetInnerSlotPadding(FVector2D(6.f, 6.f));
    SkillsBox->AddChildToVerticalBox(SkillList);
    SkillDescription = AddText(SkillsBox, FString(), 14);
    CancelSkillPlanButton = AddButton(SkillsBox, TEXT("스킬 선택 취소"));
    CancelSkillPlanButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleCancelSkillPlan);
    Status = AddText(SkillsBox, FString(), 14);

    UVerticalBox* ActionsBox = nullptr;
    USizeBox* ActionsSize = MakePanel(340.f, 340.f, true, ActionsBox);
    BottomRow->AddChildToHorizontalBox(ActionsSize)->SetVerticalAlignment(VAlign_Bottom);
    AddText(ActionsBox, TEXT("행동 계획"), 18);
    MovePlanDetails = AddText(ActionsBox, TEXT("SAP 이동: 예약 없음"), 14);
    MoveButton = AddButton(ActionsBox, TEXT("이동 예약 · SAP 1"));
    MoveButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleMove);
    CancelMovePlanButton = AddButton(ActionsBox, TEXT("이동 예약 취소"));
    CancelMovePlanButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleCancelMovePlan);
    ReadyButton = AddButton(ActionsBox, TEXT("준비 완료"));
    ReadyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleReady);
    UnreadyButton = AddButton(ActionsBox, TEXT("준비 취소"));
    UnreadyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleUnready);
    AddText(ActionsBox, TEXT("준비 완료 후\n① SAP 이동 → ② 선택한 스킬\n스킬 미선택 시 턴 넘기기"), 13);
    Theme.ApplyControls(WidgetTree);
    Theme.StyleButton(ReadyButton, true);
    RefreshView();
}

void UCombatRoundPlanningWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    UnbindWorldInput();
    BoundController = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    if (BoundController.IsValid())
    {
        BoundController->OnRoundWorldUnitClicked.AddUObject(this, &UCombatRoundPlanningWidget::HandleWorldUnitClicked);
        BoundController->OnRoundWorldTileClicked.AddUObject(this, &UCombatRoundPlanningWidget::HandleWorldTileClicked);
    }
    RefreshView();
}

void UCombatRoundPlanningWidget::UnbindWorldInput()
{
    if (BoundController.IsValid())
    {
        BoundController->OnRoundWorldUnitClicked.RemoveAll(this);
        BoundController->OnRoundWorldTileClicked.RemoveAll(this);
    }
    BoundController.Reset();
    bChoosingMove = false;
    ClearHighlights();
}

void UCombatRoundPlanningWidget::NativeOnDeactivated()
{
    UnbindWorldInput();
    Super::NativeOnDeactivated();
}

void UCombatRoundPlanningWidget::NativeDestruct()
{
    UnbindWorldInput();
    Super::NativeDestruct();
}

void UCombatRoundPlanningWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshElapsed += InDeltaTime;
    if (RefreshElapsed < 0.1f) return;
    RefreshElapsed = 0.f;
    RefreshView();
}

bool UCombatRoundPlanningWidget::CanEdit() const
{
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    return IsActivated() && IsValid(Coordinator) && Coordinator->GetView().Phase == ECombatRoundPhase::Planning && Controller->GetRoundParticipantSlot() > 0 && Controller->IsRoundInputEnabled() && !Controller->IsRoundRequestPending();
}

int32 UCombatRoundPlanningWidget::GetSelectedUnitId() const
{
    const int32 Index = UnitChoice ? UnitChoice->GetSelectedIndex() : INDEX_NONE;
    return OwnUnitIds.IsValidIndex(Index) ? OwnUnitIds[Index] : INDEX_NONE;
}

const FCombatRoundSkill* UCombatRoundPlanningWidget::GetSelectedSkill() const
{
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    return Coordinator && SkillIds.Contains(SelectedSkillId) ? Coordinator->FindSkill(SelectedSkillId) : nullptr;
}

bool UCombatRoundPlanningWidget::RefreshOptions(const ACombatRoundCoordinator* Coordinator, int32 OwnerSlot)
{
    TArray<int32> NewOwnIds;
    for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
    {
        if (Unit.HP > 0.f && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive() && OwnerSlot > 0 && !Unit.bEnemy && Unit.OwnerSlot == OwnerSlot) NewOwnIds.Add(Unit.UnitId);
    }
    const int32 PreviousId = GetSelectedUnitId();
    const int32 SelectedId = NewOwnIds.Contains(PreviousId) ? PreviousId : NewOwnIds.IsEmpty() ? INDEX_NONE : NewOwnIds[0];
    const FCombatRoundUnitView* Selected = Coordinator->GetView().Units.FindByPredicate([SelectedId](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedId; });
    TArray<FName> NewSkills;
    if (Selected)
    {
        // Replicated unit identifiers can arrive before the skill catalog; populate buttons when definitions arrive.
        // 복제된 유닛 식별자가 스킬 목록보다 먼저 도착할 수 있으므로 정의가 도착하면 버튼을 채웁니다.
        for (FName SkillId : Selected->SkillIds)
        {
            if (Coordinator->FindSkill(SkillId)) NewSkills.Add(SkillId);
        }
    }
    if (OwnUnitIds == NewOwnIds && SkillIds == NewSkills) return false;
    const bool bReload = SelectedId != PreviousId || SkillIds != NewSkills;
    TGuardValue<bool> Updating(bUpdatingOptions, true);
    OwnUnitIds = MoveTemp(NewOwnIds);
    SkillIds = NewSkills;
    UnitChoice->ClearOptions();
    for (int32 UnitId : OwnUnitIds)
    {
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
        UnitChoice->AddOption(Unit ? FString::Printf(TEXT("%s #%d"), *UnitLabel(*Unit), UnitId) : FString::FromInt(UnitId));
    }
    if (!OwnUnitIds.IsEmpty()) UnitChoice->SetSelectedIndex(OwnUnitIds.IndexOfByKey(SelectedId));
    SkillList->ClearChildren();
    SkillButtons.Reset();
    for (FName SkillId : SkillIds)
    {
        const FCombatRoundSkill* Skill = Coordinator->FindSkill(SkillId);
        if (!Skill) continue;
        UCombatRoundSkillButton* Button = WidgetTree->ConstructWidget<UCombatRoundSkillButton>();
        Button->InitializeSkill(SkillId);
        Button->OnSkillPicked.AddUObject(this, &UCombatRoundPlanningWidget::HandleSkillPicked);
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s · AP %d"), *Skill->Name.ToString(), Skill->ActionPointCost)));
        UDemonicUITheme::Get().StyleText(Label, false, 16);
        Button->SetContent(Label);
        UDemonicUITheme::Get().StyleButton(Button);
        SkillList->AddChildToWrapBox(Button)->SetPadding(FMargin(3.f));
        SkillButtons.Add(Button);
    }
    return bReload;
}

void UCombatRoundPlanningWidget::LoadSelectedCommand()
{
    SelectedSkillId = NAME_None;
    SelectedTargetId = INDEX_NONE;
    bHasTargetTile = false;
    bChoosingMove = false;
    LocalStatus = FText::GetEmpty();
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Coordinator) return;
    const int32 UnitId = GetSelectedUnitId();
    const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
    if (!Unit || !SkillIds.Contains(Unit->Command.SkillId)) return;
    SelectedSkillId = Unit->Command.SkillId;
    SelectedTargetId = Unit->Command.TargetUnitId;
    SelectedTargetCoord = Unit->Command.TargetCoord;
    bHasTargetTile = true;
}

FCombatRoundCommand UCombatRoundPlanningWidget::BuildCommand(FName SkillId) const
{
    FCombatRoundCommand Command;
    Command.UnitId = GetSelectedUnitId();
    Command.SkillId = SkillId;
    Command.TargetUnitId = SelectedTargetId;
    Command.TargetCoord = SelectedTargetCoord;
    Command.DestinationCoord = SelectedTargetCoord;
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundSkill* Skill = Coordinator ? Coordinator->FindSkill(SkillId) : nullptr;
    if (Coordinator && Skill && Skill->bRemainAtDestination)
    {
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([Command](const FCombatRoundUnitView& Entry) { return Entry.UnitId == Command.UnitId; });
        if (Unit) Command.DestinationCoord = Unit->HomeCoord;
    }
    return Command;
}

void UCombatRoundPlanningWidget::HandleUnitChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingOptions) return;
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    if (Controller && Controller->GetRoundCoordinator()) RefreshOptions(Controller->GetRoundCoordinator(), Controller->GetRoundParticipantSlot());
    LoadSelectedCommand();
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleWorldUnitClicked(int32 UnitId)
{
    if (!CanEdit()) return;
    if (OwnUnitIds.Contains(UnitId) && UnitId != GetSelectedUnitId())
    {
        UnitChoice->SetSelectedIndex(OwnUnitIds.IndexOfByKey(UnitId));
        return;
    }
    const ACombatRoundCoordinator* Coordinator = BoundController.IsValid() ? BoundController->GetRoundCoordinator() : nullptr;
    const FCombatRoundUnitView* Unit = Coordinator ? Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; }) : nullptr;
    if (!Unit || !(Unit->HP > 0.f) || !IsValid(Unit->Unit) || !Unit->Unit->IsUnitAlive()) return;
    if (bChoosingMove)
    {
        LocalStatus = FText::FromString(TEXT("이동할 아군 빈칸을 선택하세요. 점유된 칸에는 이동할 수 없습니다."));
    }
    else
    {
        SelectedTargetId = UnitId;
        SelectedTargetCoord = Unit->HomeCoord;
        bHasTargetTile = true;
        LocalStatus = FText::FromString(TEXT("사용할 스킬을 누르세요. 선택한 행동을 확인한 뒤 준비 완료를 누릅니다."));
    }
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleWorldTileClicked(FIntPoint Coord)
{
    if (!CanEdit() || !BoundController.IsValid()) return;
    const ACombatRoundCoordinator* Coordinator = BoundController->GetRoundCoordinator();
    if (bChoosingMove)
    {
        FText Error;
        if (Coordinator->CanMoveUnit(GetSelectedUnitId(), Coord, Error))
        {
            bChoosingMove = false;
            LocalStatus = FText::GetEmpty();
            BoundController->SubmitRoundMove(GetSelectedUnitId(), Coord);
        }
        else LocalStatus = Error;
    }
    else
    {
        const FCombatRoundUnitView* Occupant = Coordinator->GetView().Units.FindByPredicate([Coord](const FCombatRoundUnitView& Unit) { return Unit.HomeCoord == Coord && Unit.HP > 0.f; });
        if (Occupant)
        {
            HandleWorldUnitClicked(Occupant->UnitId);
            return;
        }
        SelectedTargetId = INDEX_NONE;
        SelectedTargetCoord = Coord;
        bHasTargetTile = true;
        LocalStatus = FText::FromString(TEXT("빈칸을 선택했습니다. 기본 공격은 적을 클릭한 뒤 사용하세요."));
    }
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleSkillPicked(FName SkillId)
{
    if (!CanEdit() || !SkillIds.Contains(SkillId) || !BoundController.IsValid()) return;
    FText Error;
    const FCombatRoundCommand Command = BuildCommand(SkillId);
    if (!BoundController->GetRoundCoordinator()->CanPlanCommand(Command, Error))
    {
        LocalStatus = Error;
        RefreshView();
        return;
    }
    bChoosingMove = false;
    SelectedSkillId = SkillId;
    LocalStatus = FText::GetEmpty();
    BoundController->SubmitRoundPlan(Command);
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleMove()
{
    if (!CanEdit() || GetSelectedUnitId() == INDEX_NONE) return;
    bChoosingMove = !bChoosingMove;
    LocalStatus = FText::FromString(bChoosingMove ? TEXT("강조된 아군 빈칸을 한 번 클릭해 이동을 예약하세요. 준비 완료 전에는 이동하거나 SAP를 소모하지 않습니다.") : TEXT("목적지 선택을 닫았습니다. 이미 적용한 이동 예약은 유지됩니다."));
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleCancelMovePlan()
{
    if (!CanEdit() || GetSelectedUnitId() == INDEX_NONE || !BoundController.IsValid()) return;
    bChoosingMove = false;
    LocalStatus = FText::GetEmpty();
    BoundController->CancelRoundMove(GetSelectedUnitId());
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleCancelSkillPlan()
{
    if (!CanEdit() || GetSelectedUnitId() == INDEX_NONE || !BoundController.IsValid()) return;
    const int32 UnitId = GetSelectedUnitId();
    const FCombatRoundUnitView* Unit = BoundController->GetRoundCoordinator()->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
    if (!Unit) return;
    FCombatRoundCommand Command;
    Command.UnitId = UnitId;
    Command.TargetCoord = Unit->HomeCoord;
    Command.DestinationCoord = Unit->HomeCoord;
    bChoosingMove = false;
    LocalStatus = FText::GetEmpty();
    BoundController->SubmitRoundPlan(Command);
    RefreshView();
}

bool UCombatRoundPlanningWidget::CanReadyPlans(FText& OutError) const
{
    OutError = FText::GetEmpty();
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Coordinator || OwnUnitIds.IsEmpty() || bChoosingMove) return false;
    for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
    {
        if (!OwnUnitIds.Contains(Unit.UnitId)) continue;
        if (!Coordinator->CanPlanCommand(Unit.Command, OutError)) return false;
    }
    return true;
}

void UCombatRoundPlanningWidget::HandleReady()
{
    FText Error;
    if (CanEdit() && CanReadyPlans(Error) && BoundController.IsValid()) BoundController->SetRoundReady(true);
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleUnready()
{
    if (CanEdit() && BoundController.IsValid()) BoundController->SetRoundReady(false);
    RefreshView();
}

void UCombatRoundPlanningWidget::ClearHighlights()
{
    for (const TWeakObjectPtr<ACombatGridTile>& Tile : HighlightedTiles)
    {
        if (Tile.IsValid()) Tile->ClearHighlightVisual();
    }
    HighlightedTiles.Reset();
}

void UCombatRoundPlanningWidget::RefreshHighlights()
{
    ClearHighlights();
    if (!CanEdit() || !BoundController.IsValid()) return;
    const ACombatRoundCoordinator* Coordinator = BoundController->GetRoundCoordinator();
    const ACombatArena* Arena = Coordinator->GetArena();
    const ACombatGridManager* Grid = Arena ? Arena->Grid.Get() : nullptr;
    if (!Grid) return;
    const int32 UnitId = GetSelectedUnitId();
    const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
    for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Grid->TileMap)
    {
        if (!IsValid(Entry.Value)) continue;
        FText Error;
        if (bChoosingMove && Coordinator->CanMoveUnit(GetSelectedUnitId(), Entry.Key, Error))
        {
            Entry.Value->ApplyMovableTileVisual();
            HighlightedTiles.Add(Entry.Value);
        }
        else if (!bChoosingMove && Unit && Unit->bHasMovePlan && Entry.Key == Unit->MoveDestinationCoord)
        {
            Entry.Value->ApplyMovableTileVisual();
            HighlightedTiles.Add(Entry.Value);
        }
        else if (!bChoosingMove && bHasTargetTile && Entry.Key == SelectedTargetCoord)
        {
            Entry.Value->ApplySkillTargetTileVisual();
            HighlightedTiles.Add(Entry.Value);
        }
    }
}

void UCombatRoundPlanningWidget::RefreshPartyCards(const ACombatRoundCoordinator* Coordinator, int32 OwnerSlot)
{
    TArray<int32> NewPartyIds;
    for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
    {
        if (!Unit.bEnemy) NewPartyIds.Add(Unit.UnitId);
    }
    if (PartyUnitIds != NewPartyIds)
    {
        PartyUnitIds = MoveTemp(NewPartyIds);
        PartyList->ClearChildren();
        PartyCards.Reset();
        PartyNames.Reset();
        PartyDetails.Reset();
        for (int32 Index = 0; Index < PartyUnitIds.Num(); ++Index)
        {
            UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
            Card->SetPadding(FMargin(8.f));
            UHorizontalBoxSlot* CardSlot = PartyList->AddChildToHorizontalBox(Card);
            CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            CardSlot->SetPadding(FMargin(3.f, 4.f));
            UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>();
            Card->SetContent(Details);
            UTextBlock* Name = AddText(Details, FString(), 16);
            Name->SetAutoWrapText(false);
            Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
            PartyCards.Add(Card);
            PartyNames.Add(Name);
            PartyDetails.Add(AddText(Details, FString(), 13));
        }
    }
    for (int32 Index = 0; Index < PartyUnitIds.Num(); ++Index)
    {
        const int32 UnitId = PartyUnitIds[Index];
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
        if (!Unit) continue;
        const bool bSelected = UnitId == GetSelectedUnitId();
        const FLinearColor Accent = Unit->HP <= 0.f ? FLinearColor(0.5f, 0.18f, 0.16f) : bSelected ? FLinearColor(0.95f, 0.72f, 0.3f) : FLinearColor(0.16f, 0.5f, 0.42f);
        PartyCards[Index]->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.024f, 0.03f, 0.036f, 0.96f), 4.f, Accent, bSelected ? 2.f : 1.f));
        const FText Name = FText::FromString(UnitLabel(*Unit));
        PartyNames[Index]->SetText(Name);
        PartyNames[Index]->SetToolTipText(Name);
        const FString Control = Unit->OwnerSlot == 0 ? TEXT("AI") : Unit->OwnerSlot == OwnerSlot ? TEXT("나") : TEXT("팀원");
        FString Detail = FString::Printf(TEXT("%s · HP %.0f"), *Control, Unit->HP);
        if (IsValid(Unit->Unit)) Detail += FString::Printf(TEXT("\nAP %d · SAP %d"), Unit->Unit->GetCurrentActionPoint(), Unit->Unit->GetCurrentSubActionPoint());
        const FCombatRoundSkill* Planned = Coordinator->FindSkill(Unit->Command.SkillId);
        const FString Plan = Unit->HP <= 0.f ? TEXT("사망") : Planned ? Planned->Name.ToString() : Unit->Command.SkillId.IsNone() ? TEXT("턴 넘기기") : TEXT("스킬 확인 필요");
        Detail += TEXT("\n") + Plan;
        if (Unit->bHasMovePlan) Detail += FString::Printf(TEXT("\n이동 (%d,%d)"), Unit->MoveDestinationCoord.X, Unit->MoveDestinationCoord.Y);
        if (Unit->HP > 0.f && Unit->bReady) Detail += TEXT("\n준비 완료");
        PartyDetails[Index]->SetText(FText::FromString(Detail));
    }
}

void UCombatRoundPlanningWidget::RefreshView()
{
    ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const bool bConnected = IsValid(Coordinator);
    const bool bEditable = CanEdit();
    bool bCanMove = false;
    bool bReadyPlans = false;
    bool bAnyReady = false;
    bool bHasMovePlan = false;
    bool bHasSkillPlan = false;
    FText ReadyError;
    if (bConnected)
    {
        const FCombatRoundView& View = Coordinator->GetView();
        const bool bReloadCommand = RefreshOptions(Coordinator, Controller->GetRoundParticipantSlot());
        if (bReloadCommand || ObservedCombatId != View.CombatId || ObservedRound != View.RoundNumber)
        {
            ObservedCombatId = View.CombatId;
            ObservedRound = View.RoundNumber;
            LoadSelectedCommand();
        }
        if (View.Phase != ECombatRoundPhase::Planning) bChoosingMove = false;
        const FString Phase = Coordinator->IsSAPMovementInProgress() ? TEXT("SAP 이동 실행") : View.Phase == ECombatRoundPhase::Resolving ? TEXT("AP 행동 실행") : RoundPhaseName(View.Phase);
        Header->SetText(FText::FromString(FString::Printf(TEXT("라운드 %d · %s"), View.RoundNumber, *Phase)));
        FString Enemies;
        int32 LivingAllies = 0;
        int32 LivingEnemies = 0;
        int32 ReadyUnits = 0;
        int32 PlanningUnits = 0;
        const FCombatRoundUnitView* SelectedUnit = nullptr;
        const FCombatRoundUnitView* Target = nullptr;
        for (const FCombatRoundUnitView& Unit : View.Units)
        {
            if (Unit.HP > 0.f)
            {
                if (Unit.bEnemy) ++LivingEnemies;
                else ++LivingAllies;
                if (!Unit.bEnemy && Unit.OwnerSlot > 0)
                {
                    ++PlanningUnits;
                    if (Unit.bReady) ++ReadyUnits;
                }
            }
            if (Unit.bEnemy)
            {
                const FCombatRoundSkill* Planned = Coordinator->FindSkill(Unit.Command.SkillId);
                const FString Plan = Unit.HP <= 0.f ? TEXT("사망") : Planned ? Planned->Name.ToString() : TEXT("대기");
                if (!Enemies.IsEmpty()) Enemies += TEXT("\n\n");
                Enemies += FString::Printf(TEXT("%s · HP %.0f\n%s%s"), *UnitLabel(Unit), Unit.HP, *Plan, Unit.bReady ? TEXT(" · 준비 완료") : TEXT(""));
                if (Unit.bHasMovePlan) Enemies += FString::Printf(TEXT(" · SAP (%d,%d)"), Unit.MoveDestinationCoord.X, Unit.MoveDestinationCoord.Y);
            }
            if (Unit.UnitId == GetSelectedUnitId()) SelectedUnit = &Unit;
            if (Unit.UnitId == SelectedTargetId && Unit.HP > 0.f) Target = &Unit;
            if (OwnUnitIds.Contains(Unit.UnitId) && Unit.bReady) bAnyReady = true;
        }
        Roster->SetText(FText::FromString(FString::Printf(TEXT("아군 %d · 적 %d  |  준비 유닛 %d / %d"), LivingAllies, LivingEnemies, ReadyUnits, PlanningUnits)));
        EnemyRoster->SetText(FText::FromString(Enemies));
        RefreshPartyCards(Coordinator, Controller->GetRoundParticipantSlot());
        if (SelectedUnit && IsValid(SelectedUnit->Unit))
        {
            UnitDetails->SetText(FText::FromString(FString::Printf(TEXT("%s\nHP %.0f · AP %d · SAP %d · 속도 %s"), *UnitLabel(*SelectedUnit), SelectedUnit->HP, SelectedUnit->Unit->GetCurrentActionPoint(), SelectedUnit->Unit->GetCurrentSubActionPoint(), *FText::AsNumber(SelectedUnit->Speed).ToString())));
            bHasMovePlan = SelectedUnit->bHasMovePlan;
            MovePlanDetails->SetText(FText::FromString(bHasMovePlan ? FString::Printf(TEXT("SAP 이동: (%d,%d) · 비용 1"), SelectedUnit->MoveDestinationCoord.X, SelectedUnit->MoveDestinationCoord.Y) : TEXT("SAP 이동: 예약 없음")));
            const ACombatArena* Arena = Coordinator->GetArena();
            if (Arena && Arena->Grid)
            {
                for (const TPair<FIntPoint, ACombatGridTile*>& Tile : Arena->Grid->TileMap)
                {
                    FText Error;
                    if (Coordinator->CanMoveUnit(SelectedUnit->UnitId, Tile.Key, Error))
                    {
                        bCanMove = true;
                        break;
                    }
                }
            }
            const FCombatRoundSkill* Applied = Coordinator->FindSkill(SelectedUnit->Command.SkillId);
            bHasSkillPlan = !SelectedUnit->Command.SkillId.IsNone();
            SelectedSkillId = SelectedUnit->Command.SkillId;
            const FCombatRoundUnitView* AppliedTarget = View.Units.FindByPredicate([SelectedUnit](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnit->Command.TargetUnitId; });
            SkillDescription->SetText(FText::FromString(Applied ? FString::Printf(TEXT("AP 행동: %s → %s\n모든 SAP 이동이 끝난 뒤 사용합니다."), *Applied->Name.ToString(), AppliedTarget ? *UnitLabel(*AppliedTarget) : TEXT("선택한 타일")) : bHasSkillPlan ? TEXT("선택한 스킬을 확인할 수 없습니다. 스킬을 다시 선택하거나 취소하세요.") : TEXT("스킬 미선택 · 준비 완료 시 턴을 넘깁니다.\n행동 비용 없음 · 예약한 이동은 SAP 1 소모")));
        }
        else
        {
            UnitDetails->SetText(FText::FromString(TEXT("전투 관전")));
            MovePlanDetails->SetText(FText::GetEmpty());
            SkillDescription->SetText(FText::FromString(TEXT("생존한 아군 AI가 자동으로 행동합니다.")));
        }
        if (Target)
        {
            SelectedTargetCoord = Target->HomeCoord;
            TargetDetails->SetText(FText::FromString(FString::Printf(TEXT("대상: %s · HP %.0f"), *UnitLabel(*Target), Target->HP)));
        }
        else
        {
            SelectedTargetId = INDEX_NONE;
            TargetDetails->SetText(FText::FromString(bChoosingMove ? TEXT("이동할 아군 빈칸을 클릭하세요.") : TEXT("공격할 적을 클릭하세요.")));
        }
        bReadyPlans = CanReadyPlans(ReadyError);
        FString Message = LocalStatus.IsEmpty() ? Controller->GetRoundRequestStatus().ToString() : LocalStatus.ToString();
        if (!View.Message.IsEmpty()) Message = View.Message.ToString() + (Message.IsEmpty() ? FString() : TEXT("\n") + Message);
        Status->SetText(FText::FromString(Message));
    }
    else Status->SetText(FText::FromString(TEXT("전투 연결을 기다리고 있습니다.")));
    UnitChoice->SetVisibility(OwnUnitIds.Num() > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UnitChoice->SetIsEnabled(bEditable);
    SkillList->SetVisibility(bConnected && Coordinator->GetView().Phase == ECombatRoundPhase::Planning && bHasTargetTile && !bChoosingMove ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    for (UCombatRoundSkillButton* Button : SkillButtons)
    {
        FText Error;
        const bool bValid = bConnected && Coordinator->CanPlanCommand(BuildCommand(Button->GetSkillId()), Error);
        Button->SetIsEnabled(bEditable && bValid && !bChoosingMove);
        Button->SetToolTipText(Error);
        UDemonicUITheme::Get().StyleButton(Button, Button->GetSkillId() == SelectedSkillId);
    }
    MoveButton->SetIsEnabled(bEditable && (bChoosingMove || bCanMove));
    if (UTextBlock* Label = Cast<UTextBlock>(MoveButton->GetContent())) Label->SetText(FText::FromString(bChoosingMove ? TEXT("목적지 선택 닫기") : bHasMovePlan ? TEXT("이동 예약 변경") : TEXT("이동 예약 · SAP 1")));
    CancelMovePlanButton->SetVisibility(bHasMovePlan ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CancelMovePlanButton->SetIsEnabled(bEditable && bHasMovePlan);
    CancelSkillPlanButton->SetVisibility(bHasSkillPlan ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CancelSkillPlanButton->SetIsEnabled(bEditable && bHasSkillPlan);
    ReadyButton->SetIsEnabled(bEditable && bReadyPlans && !bAnyReady);
    ReadyButton->SetToolTipText(ReadyError);
    UnreadyButton->SetVisibility(bAnyReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UnreadyButton->SetIsEnabled(bEditable && bAnyReady);
    RefreshHighlights();
}
