#include "UI/Combat/CombatRoundPlanningWidget.h"
#include "InputCoreTypes.h"

#include "Blueprint/WidgetTree.h"
#include "AbilitySystemComponent.h"
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
#include "UI/ProjectALocalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "UI/Debug/CombatUnitHealthDebugWidget.h"
#include "Unit/UnitBase.h"

namespace
{
    FText RoundPhaseName(ECombatRoundPhase Phase)
    {
        switch (Phase)
        {
        case ECombatRoundPhase::WaitingForPlayers: return NSLOCTEXT("CombatPlanning", "PhaseWaiting", "참가자 대기");
        case ECombatRoundPhase::Planning: return NSLOCTEXT("CombatPlanning", "PhasePlanning", "행동 선택");
        case ECombatRoundPhase::Resolving: return NSLOCTEXT("CombatPlanning", "PhaseResolving", "전투 진행");
        case ECombatRoundPhase::Finished: return NSLOCTEXT("CombatPlanning", "PhaseFinished", "전투 종료");
        case ECombatRoundPhase::Suspended: return NSLOCTEXT("CombatPlanning", "PhaseSuspended", "세션 중단");
        default: return NSLOCTEXT("CombatPlanning", "PhasePreparing", "준비 중");
        }
    }

    FText UnitLabel(const FCombatRoundUnitView& Unit)
    {
        if (IsValid(Unit.Unit) && !Unit.Unit->RuntimeCharacterName.IsEmpty())
        {
            // Only authored enemy class names have matching content entries; player and Snapshot names stay unchanged.
            // 제작된 적 클래스 이름만 콘텐츠 항목과 일치하며 플레이어와 Snapshot 이름은 유지합니다.
            return Unit.bEnemy ? ProjectALocalization::AssetName(FSoftObjectPath(Unit.Unit->GetClass()), Unit.Unit->RuntimeCharacterName) : Unit.Unit->RuntimeCharacterName;
        }
        return FText::Format(NSLOCTEXT("CombatPlanning", "UnitFallback", "{0} #{1}"), Unit.bEnemy ? NSLOCTEXT("CombatPlanning", "Enemy", "적") : NSLOCTEXT("CombatPlanning", "Ally", "아군"), FText::AsNumber(Unit.UnitId));
    }

    // Preserve localized number formatting while displaying HP without fractional digits.
    // HP를 소수점 없이 표시하면서 언어별 숫자 형식을 유지합니다.
    FText HealthLabel(float HP)
    {
        FNumberFormattingOptions Options;
        Options.SetMinimumFractionalDigits(0);
        Options.SetMaximumFractionalDigits(0);
        return FText::AsNumber(HP, &Options);
    }

    FText SkillName(const FCombatRoundSkill& Skill)
    {
        return ProjectALocalization::SkillName(Skill.SkillId, Skill.Name);
    }

    FText SkillCostLabel(const FCombatRoundSkill& Skill)
    {
        return FText::Format(NSLOCTEXT("CombatPlanning", "SkillCost", "AP {0} · SAP {1}"), FText::AsNumber(Skill.ActionPointCost), FText::AsNumber(Skill.SubActionPointCost));
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

UTextBlock* UCombatRoundPlanningWidget::AddText(UVerticalBox* Box, const FText& Text, int32 FontSize)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 18, FontSize);
    Box->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 3.f));
    return Label;
}

UButton* UCombatRoundPlanningWidget::AddButton(UVerticalBox* Box, const FText& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
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
    const auto MakePanel = [this](float Width, float MaxHeight, bool bInteractive, UVerticalBox*& Content, UVerticalBox** FixedFooter = nullptr)
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
            Scroll->AddChild(Content);
            if (FixedFooter)
            {
                // Keep confirmation controls visible while long planning details scroll above them.
                // 긴 계획 설명을 위에서 스크롤하는 동안 확정 조작은 계속 보이게 유지합니다.
                UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
                Panel->SetContent(Layout);
                Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                *FixedFooter = WidgetTree->ConstructWidget<UVerticalBox>();
                Layout->AddChildToVerticalBox(*FixedFooter);
            }
            else Panel->SetContent(Scroll);
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
    Header = AddText(RosterBox, NSLOCTEXT("CombatPlanning", "BattleTitle", "라운드 전투"), 20);
    Roster = AddText(RosterBox, FText::GetEmpty(), 14);
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
    AddText(EnemyBox, NSLOCTEXT("CombatPlanning", "TargetTitle", "대상 정보"), 18);
    TargetDetails = AddText(EnemyBox, NSLOCTEXT("CombatPlanning", "TargetInitialHint", "대상을 클릭하세요. 내 아군 대상 지정은 Shift+클릭입니다."), 16);
    EnemyRoster = AddText(EnemyBox, FText::GetEmpty(), 14);

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
    AddText(PartyBox, NSLOCTEXT("CombatPlanning", "PartyTitle", "파티 현황"), 18);
    PartyList = WidgetTree->ConstructWidget<UHorizontalBox>();
    PartyBox->AddChildToVerticalBox(PartyList);

    UVerticalBox* SkillsBox = nullptr;
    USizeBox* SkillsSize = MakePanel(0.f, 300.f, true, SkillsBox);
    UHorizontalBoxSlot* SkillsSlot = BottomRow->AddChildToHorizontalBox(SkillsSize);
    SkillsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SkillsSlot->SetVerticalAlignment(VAlign_Bottom);
    SkillsSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
    UnitDetails = AddText(SkillsBox, NSLOCTEXT("CombatPlanning", "ControlledUnit", "조작할 아군"), 18);
    UnitChoice = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), TEXT("ControlledUnitChoice"));
    SkillsBox->AddChildToVerticalBox(UnitChoice);
    UnitChoice->OnSelectionChanged.AddDynamic(this, &UCombatRoundPlanningWidget::HandleUnitChanged);
    SkillList = WidgetTree->ConstructWidget<UWrapBox>();
    SkillList->SetInnerSlotPadding(FVector2D(6.f, 6.f));
    SkillsBox->AddChildToVerticalBox(SkillList);
    SkillDescription = AddText(SkillsBox, FText::GetEmpty(), 14);
    CancelSkillPlanButton = AddButton(SkillsBox, NSLOCTEXT("CombatPlanning", "CancelSkill", "스킬 선택 취소"));
    CancelSkillPlanButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleCancelSkillPlan);
    Status = AddText(SkillsBox, FText::GetEmpty(), 14);

    UVerticalBox* ActionsBox = nullptr;
    UVerticalBox* ReadyActions = nullptr;
    USizeBox* ActionsSize = MakePanel(340.f, 340.f, true, ActionsBox, &ReadyActions);
    BottomRow->AddChildToHorizontalBox(ActionsSize)->SetVerticalAlignment(VAlign_Bottom);
    AddText(ActionsBox, NSLOCTEXT("CombatPlanning", "PlanTitle", "행동 계획"), 18);
    MovePlanDetails = AddText(ActionsBox, NSLOCTEXT("CombatPlanning", "NoMovePlan", "SAP 이동: 예약 없음"), 14);
    MoveButton = AddButton(ActionsBox, NSLOCTEXT("CombatPlanning", "PlanMove", "이동 예약 · SAP 1"));
    MoveButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleMove);
    CancelMovePlanButton = AddButton(ActionsBox, NSLOCTEXT("CombatPlanning", "CancelMove", "이동 예약 취소"));
    CancelMovePlanButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleCancelMovePlan);
    ReadyPlanDetails = AddText(ActionsBox, FText::GetEmpty(), 14);
    ReadyButton = AddButton(ReadyActions, NSLOCTEXT("CombatPlanning", "Ready", "준비 완료"));
    ReadyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleReady);
    UnreadyButton = AddButton(ReadyActions, NSLOCTEXT("CombatPlanning", "Unready", "준비 취소"));
    UnreadyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleUnready);
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
    bHasObservedState = false;
    ObservedState = FCombatPlanningRefreshState();
    MovableCoords.Reset();
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
    if (!IsActivated()) return;
    RefreshElapsed += InDeltaTime;
    if (RefreshElapsed < 0.1f) return;
    RefreshElapsed = 0.f;
    RefreshView(false);
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
    if (OwnUnitIds == NewOwnIds && SkillIds == NewSkills && ObservedTextRevision == FTextLocalizationManager::Get().GetTextRevision()) return false;
    const bool bReload = SelectedId != PreviousId || SkillIds != NewSkills;
    TGuardValue<bool> Updating(bUpdatingOptions, true);
    OwnUnitIds = MoveTemp(NewOwnIds);
    SkillIds = NewSkills;
    UnitChoice->ClearOptions();
    for (int32 UnitId : OwnUnitIds)
    {
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
        UnitChoice->AddOption(Unit ? FText::Format(NSLOCTEXT("CombatPlanning", "UnitOption", "{0} #{1}"), UnitLabel(*Unit), FText::AsNumber(UnitId)).ToString() : FText::AsNumber(UnitId).ToString());
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
        Label->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "SkillCard", "{0}\n{1}"), SkillName(*Skill), SkillCostLabel(*Skill)));
        Label->SetAutoWrapText(true);
        Label->SetWrapTextAt(300.f);
        Label->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
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
    if (Coordinator && Skill && RunRecoveryRules::IsConsumable(*Skill))
    {
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([Command](const FCombatRoundUnitView& Entry) { return Entry.UnitId == Command.UnitId; });
        Command.TargetUnitId = Command.UnitId;
        if (Unit) Command.TargetCoord = Command.DestinationCoord = Unit->HomeCoord;
    }
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
    const APlayerController* PlayerController = GetOwningPlayer();
    const bool bSelectFriendlyTarget = PlayerController && (PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift));
    if (OwnUnitIds.Contains(UnitId) && UnitId != GetSelectedUnitId() && !bSelectFriendlyTarget)
    {
        UnitChoice->SetSelectedIndex(OwnUnitIds.IndexOfByKey(UnitId));
        return;
    }
    const ACombatRoundCoordinator* Coordinator = BoundController.IsValid() ? BoundController->GetRoundCoordinator() : nullptr;
    const FCombatRoundUnitView* Unit = Coordinator ? Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; }) : nullptr;
    if (!Unit || !(Unit->HP > 0.f) || !IsValid(Unit->Unit) || !Unit->Unit->IsUnitAlive()) return;
    if (bChoosingMove)
    {
        LocalStatus = NSLOCTEXT("CombatPlanning", "EmptyMoveTileHint", "이동할 아군 빈칸을 선택하세요. 점유된 칸에는 이동할 수 없습니다.");
    }
    else
    {
        SelectedTargetId = UnitId;
        SelectedTargetCoord = Unit->HomeCoord;
        bHasTargetTile = true;
        LocalStatus = NSLOCTEXT("CombatPlanning", "ChooseSkillHint", "사용할 스킬을 누르세요. 선택한 행동을 확인한 뒤 준비 완료를 누릅니다.");
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
        LocalStatus = NSLOCTEXT("CombatPlanning", "TileSelectedHint", "타일을 대상으로 지정했습니다. 이 타일에 사용할 수 있는 스킬을 선택하세요.");
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
    LocalStatus = bChoosingMove ? NSLOCTEXT("CombatPlanning", "MoveChoosingHint", "강조된 아군 빈칸을 한 번 클릭해 이동을 예약하세요. 준비 완료 전에는 이동하거나 SAP를 소모하지 않습니다.") : NSLOCTEXT("CombatPlanning", "MoveChoosingClosed", "목적지 선택을 닫았습니다. 이미 적용한 이동 예약은 유지됩니다.");
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
    if (!Coordinator)
    {
        OutError = NSLOCTEXT("CombatPlanning", "ReadyWaitingForConnection", "전투 연결을 기다리고 있습니다.");
        return false;
    }
    if (OwnUnitIds.IsEmpty())
    {
        OutError = NSLOCTEXT("CombatPlanning", "ReadyNoOwnedUnits", "준비를 변경할 생존 직접 조작 캐릭터가 없습니다.");
        return false;
    }
    if (bChoosingMove)
    {
        OutError = NSLOCTEXT("CombatPlanning", "ReadyChooseDestination", "이동 목적지를 선택하거나 목적지 선택을 닫은 뒤 준비하세요.");
        return false;
    }
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
    for (const auto& Entry : HighlightedTiles)
    {
        if (Entry.Key.IsValid()) Entry.Key->ClearHighlightVisual();
    }
    HighlightedTiles.Reset();
}

void UCombatRoundPlanningWidget::RefreshHighlights()
{
    TMap<TWeakObjectPtr<ACombatGridTile>, bool> Desired;
    if (CanEdit() && BoundController.IsValid())
    {
        const ACombatRoundCoordinator* Coordinator = BoundController->GetRoundCoordinator();
        const ACombatArena* Arena = Coordinator->GetArena();
        const ACombatGridManager* Grid = Arena ? Arena->Grid.Get() : nullptr;
        const int32 UnitId = GetSelectedUnitId();
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
        if (Grid)
        {
            for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Grid->TileMap)
            {
                if (!IsValid(Entry.Value)) continue;
                if (bChoosingMove && MovableCoords.Contains(Entry.Key)) Desired.Add(Entry.Value, true);
                else if (!bChoosingMove && Unit && Unit->bHasMovePlan && Entry.Key == Unit->MoveDestinationCoord) Desired.Add(Entry.Value, true);
                else if (!bChoosingMove && bHasTargetTile && Entry.Key == SelectedTargetCoord) Desired.Add(Entry.Value, false);
            }
        }
    }
    // Change only tiles whose highlight changed; unchanged planning no longer resets their visuals.
    // 하이라이트가 바뀐 타일만 변경하여 동일한 계획에서 시각 상태를 재설정하지 않습니다.
    for (const auto& Entry : HighlightedTiles)
    {
        const bool* NewKind = Desired.Find(Entry.Key);
        if (Entry.Key.IsValid() && (!NewKind || *NewKind != Entry.Value)) Entry.Key->ClearHighlightVisual();
    }
    for (const auto& Entry : Desired)
    {
        const bool* OldKind = HighlightedTiles.Find(Entry.Key);
        if (OldKind && *OldKind == Entry.Value) continue;
        if (Entry.Value) Entry.Key->ApplyMovableTileVisual();
        else Entry.Key->ApplySkillTargetTileVisual();
    }
    HighlightedTiles = MoveTemp(Desired);
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
            UTextBlock* Name = AddText(Details, FText::GetEmpty(), 16);
            Name->SetAutoWrapText(false);
            Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
            PartyCards.Add(Card);
            PartyNames.Add(Name);
            PartyDetails.Add(AddText(Details, FText::GetEmpty(), 13));
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
        const FText Name = UnitLabel(*Unit);
        PartyNames[Index]->SetText(Name);
        PartyNames[Index]->SetToolTipText(Name);
        const FText Control = Unit->OwnerSlot == 0 ? NSLOCTEXT("CombatPlanning", "AI", "AI") : Unit->OwnerSlot == OwnerSlot ? NSLOCTEXT("CombatPlanning", "OwnerSelf", "나") : NSLOCTEXT("CombatPlanning", "OwnerTeammate", "팀원");
        FText Detail = FText::Format(NSLOCTEXT("CombatPlanning", "ControlHealth", "{0} · HP {1}"), Control, HealthLabel(Unit->HP));
        if (IsValid(Unit->Unit)) Detail = FText::Format(NSLOCTEXT("CombatPlanning", "DetailActionPoints", "{0}\nAP {1} · SAP {2}"), Detail, FText::AsNumber(Unit->Unit->GetCurrentActionPoint()), FText::AsNumber(Unit->Unit->GetCurrentSubActionPoint()));
        const FCombatRoundSkill* Planned = Coordinator->FindSkill(Unit->Command.SkillId);
        const FText Plan = Unit->HP <= 0.f ? NSLOCTEXT("CombatPlanning", "Dead", "사망") : Planned ? SkillName(*Planned) : Unit->Command.SkillId.IsNone() ? NSLOCTEXT("CombatPlanning", "SkipTurn", "턴 넘기기") : NSLOCTEXT("CombatPlanning", "UnknownSkill", "스킬 확인 필요");
        Detail = FText::Format(NSLOCTEXT("CombatPlanning", "DetailPlan", "{0}\n{1}"), Detail, Plan);
        if (Unit->bHasMovePlan) Detail = FText::Format(NSLOCTEXT("CombatPlanning", "DetailMove", "{0}\n이동 ({1},{2})"), Detail, FText::AsNumber(Unit->MoveDestinationCoord.X), FText::AsNumber(Unit->MoveDestinationCoord.Y));
        if (Unit->HP > 0.f && Unit->bReady) Detail = FText::Format(NSLOCTEXT("CombatPlanning", "DetailReady", "{0}\n준비 완료"), Detail);
        PartyDetails[Index]->SetText(Detail);
    }
}

FCombatPlanningRefreshState UCombatRoundPlanningWidget::CaptureRefreshState() const
{
    FCombatPlanningRefreshState State;
    ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    State.Controller = Controller;
    State.Coordinator = Coordinator;
    State.bActivated = IsActivated();
    State.SelectedUnit = GetSelectedUnitId();
    State.SelectedTarget = SelectedTargetId;
    State.TargetCoord = SelectedTargetCoord;
    State.bChoosingMove = bChoosingMove;
    State.bHasTarget = bHasTargetTile;
    State.LocalStatus = LocalStatus.ToString();
    if (Controller)
    {
        State.OwnerSlot = Controller->GetRoundParticipantSlot();
        State.RequestStatus = Controller->GetRoundRequestStatus().ToString();
        State.bInputEnabled = Controller->IsRoundInputEnabled();
        State.bRequestPending = Controller->IsRoundRequestPending();
    }
    if (!IsValid(Coordinator)) return State;
    State.View = Coordinator->GetView();
    State.Skills = Coordinator->GetSkills();
    State.bSAPMovement = Coordinator->IsSAPMovementInProgress();
    for (const FCombatRoundUnitView& Unit : State.View.Units)
    {
        FCombatPlanningUnitObservation& Observation = State.Units.AddDefaulted_GetRef();
        Observation.Unit = Unit.Unit.Get();
        Observation.Name = UnitLabel(Unit).ToString();
        if (!IsValid(Unit.Unit)) continue;
        Observation.bAlive = Unit.Unit->IsUnitAlive();
        Observation.AP = Unit.Unit->GetCurrentActionPoint();
        Observation.SAP = Unit.Unit->GetCurrentSubActionPoint();
        Observation.MoveRange = Unit.Unit->GetMoveRange();
        for (const FRunConsumableStack& Stack : Unit.Unit->Consumables) Observation.ConsumableQuantities.Add(Stack.Quantity);
        Observation.CurrentTile = Unit.Unit->GetCurrentTile();
        if (UAbilitySystemComponent* ASC = Unit.Unit->GetAbilitySystemComponent())
        {
            Observation.AbilitySystem = ASC;
            ASC->GetOwnedGameplayTags(Observation.Tags);
            ASC->GetBlockedAbilityTags(Observation.BlockedAbilityTags);
        }
    }
    ACombatArena* Arena = Coordinator->GetArena();
    ACombatGridManager* Grid = IsValid(Arena) ? Arena->Grid.Get() : nullptr;
    State.Arena = Arena;
    State.Grid = Grid;
    if (IsValid(Grid))
    {
        for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Grid->TileMap)
        {
            FCombatPlanningTileObservation& Tile = State.Tiles.AddDefaulted_GetRef();
            Tile.Coord = Entry.Key;
            Tile.Tile = Entry.Value;
            Tile.bValid = IsValid(Entry.Value);
            if (!Tile.bValid) continue;
            Tile.TileCoord = Entry.Value->GridCoord;
            Tile.Occupant = Entry.Value->GetOccupyingUnit();
            Tile.Territory = static_cast<uint8>(Entry.Value->GetTerritory());
            Tile.bProtected = Entry.Value->GetProtectedByFront();
        }
        State.Tiles.Sort([](const FCombatPlanningTileObservation& Left, const FCombatPlanningTileObservation& Right) { return Left.Coord.X == Right.Coord.X ? Left.Coord.Y < Right.Coord.Y : Left.Coord.X < Right.Coord.X; });
    }
    return State;
}

void UCombatRoundPlanningWidget::RefreshView(bool bForce)
{
    // String-backed selectors must also refresh when editor preview or game localization changes.
    // 문자열 기반 선택 목록은 에디터 미리보기나 게임 번역이 변경될 때도 갱신합니다.
    if (!bForce && bHasObservedState && ObservedTextRevision == FTextLocalizationManager::Get().GetTextRevision() && ObservedState == CaptureRefreshState()) return;
    MovableCoords.Reset();
    ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const bool bConnected = IsValid(Coordinator);
    const bool bEditable = CanEdit();
    bool bCanMove = false;
    bool bReadyPlans = false;
    bool bAnyReady = false;
    bool bHasMovePlan = false;
    bool bHasSkillPlan = false;
    int32 PlannedSkillCount = 0;
    int32 PlannedMoveCount = 0;
    int32 WaitingUnitCount = 0;
    FText ReadyLabel = NSLOCTEXT("CombatPlanning", "Ready", "준비 완료");
    FText InputHint = NSLOCTEXT("CombatPlanning", "WaitingForConnection", "전투 연결을 기다리고 있습니다.");
    TOptional<FLinearColor> InputColor = FLinearColor(0.95f, 0.72f, 0.3f);
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
        const FText Phase = Coordinator->IsSAPMovementInProgress() ? NSLOCTEXT("CombatPlanning", "ExecutingMovement", "SAP 이동 실행") : View.Phase == ECombatRoundPhase::Resolving ? NSLOCTEXT("CombatPlanning", "ExecutingActions", "AP 행동 실행") : RoundPhaseName(View.Phase);
        Header->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "RoundHeader", "라운드 {0} · {1}"), FText::AsNumber(View.RoundNumber), Phase));
        TArray<FText> Enemies;
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
                const FText Plan = Unit.HP <= 0.f ? NSLOCTEXT("CombatPlanning", "Dead", "사망") : Planned ? SkillName(*Planned) : NSLOCTEXT("CombatPlanning", "Waiting", "대기");
                FText Enemy = FText::Format(NSLOCTEXT("CombatPlanning", "EnemyDetail", "{0} · HP {1}\n{2}"), UnitLabel(Unit), HealthLabel(Unit.HP), Plan);
                if (Unit.bReady) Enemy = FText::Format(NSLOCTEXT("CombatPlanning", "EnemyReady", "{0} · 준비 완료"), Enemy);
                if (Unit.bHasMovePlan) Enemy = FText::Format(NSLOCTEXT("CombatPlanning", "EnemyMove", "{0} · SAP ({1},{2})"), Enemy, FText::AsNumber(Unit.MoveDestinationCoord.X), FText::AsNumber(Unit.MoveDestinationCoord.Y));
                Enemies.Add(Enemy);
            }
            if (Unit.UnitId == GetSelectedUnitId()) SelectedUnit = &Unit;
            if (Unit.UnitId == SelectedTargetId && Unit.HP > 0.f) Target = &Unit;
            if (OwnUnitIds.Contains(Unit.UnitId))
            {
                bAnyReady |= Unit.bReady;
                if (!Unit.Command.SkillId.IsNone()) ++PlannedSkillCount;
                if (Unit.bHasMovePlan) ++PlannedMoveCount;
                if (Unit.Command.SkillId.IsNone() && !Unit.bHasMovePlan) ++WaitingUnitCount;
            }
        }
        Roster->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "RosterSummary", "아군 {0} · 적 {1}  |  준비 유닛 {2} / {3}"), FText::AsNumber(LivingAllies), FText::AsNumber(LivingEnemies), FText::AsNumber(ReadyUnits), FText::AsNumber(PlanningUnits)));
        EnemyRoster->SetText(FText::Join(FText::FromString(TEXT("\n\n")), Enemies));
        RefreshPartyCards(Coordinator, Controller->GetRoundParticipantSlot());
        if (SelectedUnit && IsValid(SelectedUnit->Unit))
        {
            UnitDetails->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "SelectedUnitStats", "{0}\nHP {1} · AP {2} · SAP {3} · 속도 {4}"), UnitLabel(*SelectedUnit), HealthLabel(SelectedUnit->HP), FText::AsNumber(SelectedUnit->Unit->GetCurrentActionPoint()), FText::AsNumber(SelectedUnit->Unit->GetCurrentSubActionPoint()), FText::AsNumber(SelectedUnit->Speed)));
            if (!SelectedUnit->Unit->Consumables.IsEmpty())
            {
                int32 Quantity = 0;
                for (const FRunConsumableStack& Stack : SelectedUnit->Unit->Consumables) Quantity += Stack.Quantity;
                UnitDetails->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "ConsumableQuantity", "{0} · 소모품 {1}개"), UnitDetails->GetText(), FText::AsNumber(Quantity)));
            }
            bHasMovePlan = SelectedUnit->bHasMovePlan;
            MovePlanDetails->SetText(bHasMovePlan ? FText::Format(NSLOCTEXT("CombatPlanning", "MovePlan", "SAP 이동: ({0},{1}) · 비용 1"), FText::AsNumber(SelectedUnit->MoveDestinationCoord.X), FText::AsNumber(SelectedUnit->MoveDestinationCoord.Y)) : NSLOCTEXT("CombatPlanning", "NoMovePlan", "SAP 이동: 예약 없음"));
            const ACombatArena* Arena = Coordinator->GetArena();
            if (bEditable && Arena && Arena->Grid)
            {
                for (const TPair<FIntPoint, ACombatGridTile*>& Tile : Arena->Grid->TileMap)
                {
                    FText Error;
                    if (Coordinator->CanMoveUnit(SelectedUnit->UnitId, Tile.Key, Error))
                    {
                        bCanMove = true;
                        MovableCoords.Add(Tile.Key);
                    }
                }
            }
            const FCombatRoundSkill* Applied = Coordinator->FindSkill(SelectedUnit->Command.SkillId);
            bHasSkillPlan = !SelectedUnit->Command.SkillId.IsNone();
            SelectedSkillId = SelectedUnit->Command.SkillId;
            const FCombatRoundUnitView* AppliedTarget = View.Units.FindByPredicate([SelectedUnit](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnit->Command.TargetUnitId; });
            if (Applied)
            {
                // Describe the accepted command separately from the target currently being considered.
                // 현재 지정 중인 대상과 구분하여 서버에 반영된 예약 명령을 설명합니다.
                const FText TargetLabel = AppliedTarget ? UnitLabel(*AppliedTarget) : FText::Format(NSLOCTEXT("CombatPlanning", "TileLabel", "타일 ({0},{1})"), FText::AsNumber(SelectedUnit->Command.TargetCoord.X), FText::AsNumber(SelectedUnit->Command.TargetCoord.Y));
                const int32 TotalSAP = Applied->SubActionPointCost + (bHasMovePlan ? 1 : 0);
                SkillDescription->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "AcceptedSkillPlan", "예약된 스킬: {0} → {1}\n스킬 비용 {2} · 이동 포함 SAP 합계 {3}\n모든 SAP 이동이 끝난 뒤 사용합니다."), SkillName(*Applied), TargetLabel, SkillCostLabel(*Applied), FText::AsNumber(TotalSAP)));
            }
            else SkillDescription->SetText(bHasSkillPlan ? NSLOCTEXT("CombatPlanning", "UnknownSelectedSkill", "선택한 스킬을 확인할 수 없습니다. 스킬을 다시 선택하거나 취소하세요.") : bHasMovePlan ? NSLOCTEXT("CombatPlanning", "MoveOnlyPlan", "스킬 미선택 · 예약한 이동만 실행합니다.\n스킬 비용 없음 · 이동 SAP 1 소모") : NSLOCTEXT("CombatPlanning", "NoActionPlan", "스킬·이동 미선택 · 준비 완료 시 행동 없이 턴을 넘깁니다.\nAP·SAP 비용 없음"));
        }
        else
        {
            UnitDetails->SetText(NSLOCTEXT("CombatPlanning", "SpectateTitle", "전투 관전"));
            MovePlanDetails->SetText(FText::GetEmpty());
            SkillDescription->SetText(NSLOCTEXT("CombatPlanning", "SpectateHint", "생존한 아군 AI가 자동으로 행동합니다."));
        }
        if (Target)
        {
            SelectedTargetCoord = Target->HomeCoord;
            TargetDetails->SetText(bChoosingMove ? NSLOCTEXT("CombatPlanning", "ChooseMoveTile", "이동할 아군 빈칸을 클릭하세요.") : FText::Format(NSLOCTEXT("CombatPlanning", "SelectedTarget", "지정 대상: {0} · {1} · HP {2}\n스킬 버튼을 눌러 이 대상에게 행동을 예약합니다."), UnitLabel(*Target), Target->bEnemy ? NSLOCTEXT("CombatPlanning", "Enemy", "적") : NSLOCTEXT("CombatPlanning", "Ally", "아군"), HealthLabel(Target->HP)));
        }
        else
        {
            SelectedTargetId = INDEX_NONE;
            TargetDetails->SetText(bChoosingMove ? NSLOCTEXT("CombatPlanning", "ChooseMoveTile", "이동할 아군 빈칸을 클릭하세요.") : bHasTargetTile ? FText::Format(NSLOCTEXT("CombatPlanning", "SelectedTile", "지정 대상: 타일 ({0},{1})\n이 타일에 사용할 스킬을 선택하세요."), FText::AsNumber(SelectedTargetCoord.X), FText::AsNumber(SelectedTargetCoord.Y)) : NSLOCTEXT("CombatPlanning", "ChooseSkillTarget", "스킬 대상을 클릭하세요.\n내 다른 아군을 대상으로 지정하려면 Shift+클릭하세요."));
        }
        bReadyPlans = CanReadyPlans(ReadyError);
        if (View.Phase != ECombatRoundPhase::Planning)
        {
            ReadyLabel = View.Phase == ECombatRoundPhase::Resolving ? NSLOCTEXT("CombatPlanning", "ExecutingBattle", "전투 실행 중") : RoundPhaseName(View.Phase);
            InputHint = View.Phase == ECombatRoundPhase::Resolving ? NSLOCTEXT("CombatPlanning", "PlansLocked", "계획이 확정되었습니다. 다음 계획 단계에서 변경할 수 있습니다.") : RoundPhaseName(View.Phase);
        }
        else if (Controller->IsRoundRequestPending())
        {
            ReadyLabel = NSLOCTEXT("CombatPlanning", "ServerPending", "서버 확인 중");
            InputHint = NSLOCTEXT("CombatPlanning", "RequestPending", "요청 결과를 기다리고 있습니다. 확인 후 다시 선택할 수 있습니다.");
        }
        else if (OwnUnitIds.IsEmpty())
        {
            ReadyLabel = NSLOCTEXT("CombatPlanning", "Spectating", "관전 중");
            InputHint = ReadyError;
        }
        else if (!Controller->IsRoundInputEnabled())
        {
            InputHint = NSLOCTEXT("CombatPlanning", "MenuBlocksPlanning", "열린 메뉴를 닫으면 전투 계획을 변경할 수 있습니다.");
        }
        else if (bChoosingMove)
        {
            ReadyLabel = NSLOCTEXT("CombatPlanning", "ChoosingDestination", "목적지 선택 중");
            InputHint = ReadyError;
        }
        else if (bAnyReady)
        {
            ReadyLabel = NSLOCTEXT("CombatPlanning", "AlreadyReady", "준비 완료됨");
            InputHint = NSLOCTEXT("CombatPlanning", "ReadyAwaitingOthers", "다른 참가자의 준비를 기다립니다. 계획을 바꾸면 내 준비가 해제됩니다.");
            InputColor = FLinearColor(0.35f, 0.85f, 0.6f);
        }
        else if (!ReadyError.IsEmpty())
        {
            InputHint = ReadyError;
        }
        else
        {
            ReadyLabel = PlannedSkillCount == 0 ? PlannedMoveCount == 0 ? NSLOCTEXT("CombatPlanning", "ReadyNoAction", "행동 없이 준비 완료") : NSLOCTEXT("CombatPlanning", "ReadyMoveOnly", "이동만 준비 완료") : NSLOCTEXT("CombatPlanning", "Ready", "준비 완료");
            InputHint = NSLOCTEXT("CombatPlanning", "ReviewBeforeReady", "내 캐릭터의 예약을 확인한 뒤 준비하세요. 모두 준비하면 실행됩니다.");
            InputColor.Reset();
        }
        ReadyPlanDetails->SetText(FText::Format(NSLOCTEXT("CombatPlanning", "OwnPlanSummary", "내 계획 · 스킬 {0}명 · 이동 {1}명 · 무행동 {2}명\n{3}"), FText::AsNumber(PlannedSkillCount), FText::AsNumber(PlannedMoveCount), FText::AsNumber(WaitingUnitCount), InputHint));
        FText Message = View.Phase != ECombatRoundPhase::Planning ? FText::GetEmpty() : Controller->IsRoundRequestPending() || LocalStatus.IsEmpty() ? Controller->GetRoundRequestStatus() : LocalStatus;
        if (!View.Message.IsEmpty()) Message = Message.IsEmpty() ? View.Message : FText::Format(NSLOCTEXT("CombatPlanning", "CombinedStatus", "{0}\n{1}"), View.Message, Message);
        Status->SetText(Message);
    }
    else
    {
        Header->SetText(NSLOCTEXT("CombatPlanning", "ConnectionHeader", "전투 연결 대기"));
        Roster->SetText(FText::GetEmpty());
        EnemyRoster->SetText(FText::GetEmpty());
        UnitDetails->SetText(FText::GetEmpty());
        MovePlanDetails->SetText(FText::GetEmpty());
        SkillDescription->SetText(FText::GetEmpty());
        TargetDetails->SetText(FText::GetEmpty());
        ReadyPlanDetails->SetText(InputHint);
        Status->SetText(FText::GetEmpty());
    }
    UDemonicUITheme::Get().StyleText(ReadyPlanDetails, false, 14);
    if (InputColor.IsSet()) ReadyPlanDetails->SetColorAndOpacity(InputColor.GetValue());
    Status->SetVisibility(Status->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    UnitChoice->SetVisibility(OwnUnitIds.Num() > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UnitChoice->SetIsEnabled(bEditable);
    SkillList->SetVisibility(bConnected && Coordinator->GetView().Phase == ECombatRoundPhase::Planning && bHasTargetTile && !bChoosingMove ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    for (UCombatRoundSkillButton* Button : SkillButtons)
    {
        FText Error;
        const bool bValid = bConnected && Coordinator->CanPlanCommand(BuildCommand(Button->GetSkillId()), Error);
        Button->SetIsEnabled(bEditable && bValid && !bChoosingMove);
        const FCombatRoundSkill* Skill = bConnected ? Coordinator->FindSkill(Button->GetSkillId()) : nullptr;
        if (Skill)
        {
            const FText Label = FText::Format(NSLOCTEXT("CombatPlanning", "SkillCard", "{0}\n{1}"), SkillName(*Skill), SkillCostLabel(*Skill));
            if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent())) Text->SetText(Label);
            const FText Reason = !bEditable || bChoosingMove ? InputHint : Error;
            Button->SetToolTipText(Reason.IsEmpty() ? Label : FText::Format(NSLOCTEXT("CombatPlanning", "SkillAvailability", "{0}\n{1}"), Label, Reason));
        }
        else Button->SetToolTipText(Error);
        UDemonicUITheme::Get().StyleButton(Button, Button->GetSkillId() == SelectedSkillId);
    }
    MoveButton->SetIsEnabled(bEditable && (bChoosingMove || bCanMove));
    if (UTextBlock* Label = Cast<UTextBlock>(MoveButton->GetContent())) Label->SetText(bChoosingMove ? NSLOCTEXT("CombatPlanning", "CloseDestination", "목적지 선택 닫기") : bHasMovePlan ? NSLOCTEXT("CombatPlanning", "ChangeMovePlan", "이동 예약 변경") : NSLOCTEXT("CombatPlanning", "PlanMove", "이동 예약 · SAP 1"));
    CancelMovePlanButton->SetVisibility(bHasMovePlan ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CancelMovePlanButton->SetIsEnabled(bEditable && bHasMovePlan);
    CancelSkillPlanButton->SetVisibility(bHasSkillPlan ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CancelSkillPlanButton->SetIsEnabled(bEditable && bHasSkillPlan);
    ReadyButton->SetIsEnabled(bEditable && bReadyPlans && !bAnyReady);
    if (UTextBlock* Label = Cast<UTextBlock>(ReadyButton->GetContent())) Label->SetText(ReadyLabel);
    ReadyButton->SetToolTipText(!bEditable || bAnyReady || ReadyError.IsEmpty() ? InputHint : ReadyError);
    UnreadyButton->SetVisibility(bAnyReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UnreadyButton->SetIsEnabled(bEditable && bAnyReady);
    RefreshHighlights();
    ObservedState = CaptureRefreshState();
    ObservedTextRevision = FTextLocalizationManager::Get().GetTextRevision();
    bHasObservedState = true;
}
