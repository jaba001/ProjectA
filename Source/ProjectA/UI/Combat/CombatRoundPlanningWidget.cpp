#include "UI/Combat/CombatRoundPlanningWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/CombatRoundPlayerController.h"
#include "UI/Theme/DemonicUITheme.h"
#include "Unit/UnitBase.h"

namespace
{
    FString RoundPhaseName(ECombatRoundPhase Phase)
    {
        switch (Phase)
        {
        case ECombatRoundPhase::WaitingForPlayers: return TEXT("참가자 대기");
        case ECombatRoundPhase::Planning: return TEXT("행동 계획");
        case ECombatRoundPhase::Resolving: return TEXT("자동 전투");
        case ECombatRoundPhase::Finished: return TEXT("전투 종료");
        case ECombatRoundPhase::Suspended: return TEXT("세션 중단");
        default: return TEXT("준비 중");
        }
    }

    FString ActionPhaseName(ECombatRoundActionPhase Phase)
    {
        switch (Phase)
        {
        case ECombatRoundActionPhase::Planned: return TEXT("계획");
        case ECombatRoundActionPhase::Waiting: return TEXT("시작 대기");
        case ECombatRoundActionPhase::Approaching: return TEXT("접근");
        case ECombatRoundActionPhase::Casting: return TEXT("시전");
        case ECombatRoundActionPhase::Returning: return TEXT("복귀");
        case ECombatRoundActionPhase::Complete: return TEXT("완료");
        case ECombatRoundActionPhase::Cancelled: return TEXT("취소");
        default: return FString();
        }
    }

    FString CoordLabel(FIntPoint Coord)
    {
        return FString::Printf(TEXT("행 %d · 열 %d"), Coord.X, Coord.Y);
    }
}

TOptional<FUIInputConfig> UCombatRoundPlanningWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UTextBlock* UCombatRoundPlanningWidget::AddText(UVerticalBox* Box, const FString& Text, int32 FontSize)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 18, FontSize);
    Box->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 4.f));
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
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 5.f));
    return Button;
}

UComboBoxString* UCombatRoundPlanningWidget::AddCombo(UVerticalBox* Box, const FString& Label)
{
    AddText(Box, Label);
    UComboBoxString* Combo = WidgetTree->ConstructWidget<UDemonicComboBoxString>();
    Box->AddChildToVerticalBox(Combo)->SetPadding(FMargin(0.f, 2.f, 0.f, 5.f));
    return Combo;
}

void UCombatRoundPlanningWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    USizeBox* PlanningSize = WidgetTree->ConstructWidget<USizeBox>();
    PlanningSize->SetWidthOverride(390.f);
    UOverlaySlot* PlanningSlot = Root->AddChildToOverlay(PlanningSize);
    PlanningSlot->SetHorizontalAlignment(HAlign_Right);
    PlanningSlot->SetVerticalAlignment(VAlign_Fill);
    PlanningSlot->SetPadding(FMargin(0.f, 12.f));
    UBorder* PlanningPanel = WidgetTree->ConstructWidget<UBorder>();
    Theme.StylePanel(PlanningPanel);
    PlanningPanel->SetPadding(FMargin(14.f));
    PlanningSize->SetContent(PlanningPanel);
    UScrollBox* PlanningScroll = WidgetTree->ConstructWidget<UScrollBox>();
    PlanningPanel->SetContent(PlanningScroll);
    UVerticalBox* Controls = WidgetTree->ConstructWidget<UVerticalBox>();
    PlanningScroll->AddChild(Controls);
    Header = AddText(Controls, TEXT("라운드 전투"), 20);
    Theme.AddDivider(WidgetTree, Controls);
    AddText(Controls, TEXT("아군을 선택해 스킬과 대상을 지정하고 계획 적용을 누르세요. 소유한 모든 생존 아군의 계획을 확인한 뒤 준비 완료를 누릅니다."));
    UnitChoice = AddCombo(Controls, TEXT("조작할 아군"));
    SkillChoice = AddCombo(Controls, TEXT("스킬"));
    SkillDescription = AddText(Controls, FString(), 14);
    TargetChoice = AddCombo(Controls, TEXT("대상 유닛"));
    TargetTileChoice = AddCombo(Controls, TEXT("공격 대상 타일 · 행/열은 0부터 시작"));
    DestinationChoice = AddCombo(Controls, TEXT("접근 / 이동 목적지 · 잔류 스킬은 아군 칸만 허용"));
    UnitChoice->OnSelectionChanged.AddDynamic(this, &UCombatRoundPlanningWidget::HandleUnitChanged);
    SkillChoice->OnSelectionChanged.AddDynamic(this, &UCombatRoundPlanningWidget::HandleSkillChanged);
    ApplyButton = AddButton(Controls, TEXT("선택한 유닛의 계획 적용"));
    ApplyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleApplyPlan);
    ReadyButton = AddButton(Controls, TEXT("준비 완료"));
    ReadyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleReady);
    UnreadyButton = AddButton(Controls, TEXT("준비 취소"));
    UnreadyButton->OnClicked.AddDynamic(this, &UCombatRoundPlanningWidget::HandleUnready);
    Status = AddText(Controls, TEXT("세션에 연결하고 있습니다."), 14);
    AddText(Controls, TEXT("속도 차 1당 시작 지연 0.1초\n모든 행동·복귀·투사체 처리가 끝나면 다음 계획을 시작합니다."), 13);

    USizeBox* RosterSize = WidgetTree->ConstructWidget<USizeBox>();
    RosterSize->SetWidthOverride(310.f);
    UOverlaySlot* RosterSlot = Root->AddChildToOverlay(RosterSize);
    RosterSlot->SetHorizontalAlignment(HAlign_Left);
    RosterSlot->SetVerticalAlignment(VAlign_Fill);
    RosterSlot->SetPadding(FMargin(0.f, 12.f));
    UBorder* RosterPanel = WidgetTree->ConstructWidget<UBorder>();
    Theme.StylePanel(RosterPanel);
    RosterPanel->SetPadding(FMargin(12.f));
    RosterSize->SetContent(RosterPanel);
    UScrollBox* RosterScroll = WidgetTree->ConstructWidget<UScrollBox>();
    RosterPanel->SetContent(RosterScroll);
    UVerticalBox* RosterBox = WidgetTree->ConstructWidget<UVerticalBox>();
    RosterScroll->AddChild(RosterBox);
    UTextBlock* RosterHeader = AddText(RosterBox, TEXT("아군 계획 / 고정된 적 의도"), 18);
    Theme.AddDivider(WidgetTree, RosterBox);
    Roster = AddText(RosterBox, FString(), 14);
    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Header, true, 20);
    Theme.StyleText(RosterHeader, true, 18);
    Theme.StyleButton(ReadyButton, true);

    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Column = 0; Column < 4; ++Column)
        {
            const FIntPoint Coord(Row, Column);
            TargetCoords.Add(Coord);
            TargetTileChoice->AddOption(CoordLabel(Coord));
            if (Column < 2)
            {
                DestinationCoords.Add(Coord);
                DestinationChoice->AddOption(CoordLabel(Coord));
            }
        }
    }
    TargetTileChoice->SetSelectedIndex(0);
    DestinationChoice->SetSelectedIndex(0);
    RefreshView();
}

void UCombatRoundPlanningWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshElapsed += InDeltaTime;
    if (RefreshElapsed < 0.1f) return;
    RefreshElapsed = 0.f;
    RefreshView();
}

bool UCombatRoundPlanningWidget::RefreshOptions(const ACombatRoundCoordinator* Coordinator, int32 OwnerSlot)
{
    TArray<int32> NewOwnIds;
    TArray<FName> NewSkillIds;
    for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
    {
        if (!(Unit.HP > 0.f) || (IsValid(Unit.Unit) && !Unit.Unit->IsUnitAlive())) continue;
        if (OwnerSlot > 0 && !Unit.bEnemy && Unit.OwnerSlot == OwnerSlot) NewOwnIds.Add(Unit.UnitId);
    }
    const int32 SelectedUnitId = NewOwnIds.Contains(GetSelectedUnitId()) ? GetSelectedUnitId() : NewOwnIds.IsEmpty() ? INDEX_NONE : NewOwnIds[0];
    const FCombatRoundUnitView* SelectedUnit = Coordinator->GetView().Units.FindByPredicate([SelectedUnitId](const FCombatRoundUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
    for (const FCombatRoundSkill& Skill : Coordinator->GetSkills())
    {
        if (SelectedUnit && SelectedUnit->SkillIds.Contains(Skill.SkillId)) NewSkillIds.Add(Skill.SkillId);
    }
    if (NewOwnIds == OwnUnitIds && NewSkillIds == SkillIds) return false;
    const int32 PreviousUnit = GetSelectedUnitId();
    const bool bReloadCommand = PreviousUnit != SelectedUnitId || NewSkillIds != SkillIds;
    const FCombatRoundSkill* PreviousSkill = GetSelectedSkill();
    const FName PreviousSkillId = PreviousSkill ? PreviousSkill->SkillId : NAME_None;
    bUpdatingOptions = true;
    OwnUnitIds = MoveTemp(NewOwnIds);
    SkillIds = MoveTemp(NewSkillIds);
    UnitChoice->ClearOptions();
    SkillChoice->ClearOptions();
    for (int32 UnitId : OwnUnitIds) UnitChoice->AddOption(FString::Printf(TEXT("아군 #%d"), UnitId));
    for (const FName SkillId : SkillIds)
    {
        if (const FCombatRoundSkill* Skill = Coordinator->FindSkill(SkillId)) SkillChoice->AddOption(Skill->Name.ToString());
    }
    UnitChoice->SetSelectedIndex(FMath::Max(0, OwnUnitIds.IndexOfByKey(PreviousUnit)));
    SkillChoice->SetSelectedIndex(FMath::Max(0, SkillIds.IndexOfByKey(PreviousSkillId)));
    bUpdatingOptions = false;
    // Another owned unit leaving the roster must not discard this unit's draft.
    // 다른 소유 유닛이 목록에서 빠져도 현재 유닛의 초안을 버리지 않습니다.
    return bReloadCommand;
}

void UCombatRoundPlanningWidget::RefreshTargetOptions()
{
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    TArray<int32> NewTargetIds;
    if (Coordinator && Skill)
    {
        for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
        {
            if (Coordinator->IsValidUnitTarget(GetSelectedUnitId(), Skill->SkillId, Unit.UnitId)) NewTargetIds.Add(Unit.UnitId);
        }
    }
    if (NewTargetIds == TargetUnitIds) return;
    const int32 PreviousTarget = TargetUnitIds.IsValidIndex(TargetChoice->GetSelectedIndex()) ? TargetUnitIds[TargetChoice->GetSelectedIndex()] : INDEX_NONE;
    TGuardValue<bool> UpdatingOptions(bUpdatingOptions, true);
    TargetUnitIds = MoveTemp(NewTargetIds);
    TargetChoice->ClearOptions();
    for (int32 UnitId : TargetUnitIds)
    {
        const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
        TargetChoice->AddOption(FString::Printf(TEXT("%s #%d"), Unit && Unit->bEnemy ? TEXT("적") : TEXT("아군"), UnitId));
    }
    if (!TargetUnitIds.IsEmpty()) TargetChoice->SetSelectedIndex(FMath::Max(0, TargetUnitIds.IndexOfByKey(PreviousTarget)));
}

void UCombatRoundPlanningWidget::RefreshView()
{
    ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const bool bConnected = IsValid(Coordinator);
    const bool bPlanning = bConnected && Coordinator->GetView().Phase == ECombatRoundPhase::Planning;
    const bool bEditable = bPlanning && Controller->GetRoundParticipantSlot() > 0 && Controller->IsRoundInputEnabled() && !Controller->IsRoundRequestPending();
    bool bValidDraft = false;
    bool bReadyPlans = false;
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
        RefreshTargetOptions();
        FText DraftError;
        FText ReadyError;
        bValidDraft = bPlanning && Coordinator->CanPlanCommand(BuildSelectedCommand(), DraftError);
        bReadyPlans = bPlanning && CanReadyPlans(ReadyError);
        Header->SetText(FText::FromString(FString::Printf(TEXT("라운드 %d · %s\n내 아군 %d명 · %.1f초 · 투사체 %d"), View.RoundNumber, *RoundPhaseName(View.Phase), OwnUnitIds.Num(), View.ElapsedSeconds, View.PendingProjectiles)));
        FString RosterText;
        for (const FCombatRoundUnitView& Unit : View.Units)
        {
            const FCombatRoundSkill* Skill = Coordinator->FindSkill(Unit.Command.SkillId);
            const FString Owner = Unit.bEnemy ? TEXT("적 AI") : Unit.OwnerSlot == 0 ? TEXT("아군 AI") : Unit.OwnerSlot == Controller->GetRoundParticipantSlot() ? TEXT("아군 / 나") : TEXT("아군 / 팀원");
            RosterText += FString::Printf(TEXT("%s #%d\nHP %.0f · 보호 %.0f · 속도 %d\n기준 칸 (%d,%d) · 시작 +%.1f초\n%s · %s\n"), *Owner, Unit.UnitId, Unit.HP, Unit.Guard, Unit.Speed, Unit.HomeCoord.X, Unit.HomeCoord.Y, Unit.StartDelay, Unit.bReady ? TEXT("준비 완료") : TEXT("준비 전"), *ActionPhaseName(Unit.ActionPhase));
            if (Skill)
            {
                RosterText += Skill->Name.ToString();
                if (Skill->Kind != ECombatRoundSkillKind::Wait)
                {
                    RosterText += Skill->Kind == ECombatRoundSkillKind::GroundAttack ? FString::Printf(TEXT(" → 타일 (%d,%d)"), Unit.Command.TargetCoord.X, Unit.Command.TargetCoord.Y) : FString::Printf(TEXT(" → 유닛 #%d"), Unit.Command.TargetUnitId);
                    if (Skill->Approach == ECombatRoundApproach::Tile) RosterText += FString::Printf(TEXT(" / 접근 (%d,%d)%s"), Unit.Command.DestinationCoord.X, Unit.Command.DestinationCoord.Y, Skill->bRemainAtDestination ? TEXT(" 잔류") : TEXT(" 후 복귀"));
                }
                RosterText += TEXT("\n");
            }
            else RosterText += TEXT("계획 미선택\n");
            if (!Unit.Status.IsEmpty()) RosterText += Unit.Status.ToString() + TEXT("\n");
            RosterText += TEXT("\n");
        }
        Roster->SetText(FText::FromString(RosterText));
        const FString Unapplied = bPlanning && HasUnappliedChanges() ? TEXT("\n선택한 유닛의 변경사항이 아직 적용되지 않았습니다.") : TEXT("");
        const FText ValidationError = !bValidDraft ? DraftError : Unapplied.IsEmpty() ? ReadyError : FText::GetEmpty();
        const FString Validation = bPlanning && !OwnUnitIds.IsEmpty() && !ValidationError.IsEmpty() ? TEXT("\n") + ValidationError.ToString() : FString();
        Status->SetText(FText::FromString(View.Message.ToString() + TEXT("\n") + Controller->GetRoundRequestStatus().ToString() + Unapplied + Validation));
    }
    else if (Controller) Status->SetText(Controller->GetRoundRequestStatus().IsEmpty() ? FText::FromString(TEXT("아레나와 참가자 연결을 기다리고 있습니다.")) : Controller->GetRoundRequestStatus());
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    const bool bHasSelection = GetSelectedUnitId() != INDEX_NONE && Skill;
    UnitChoice->SetIsEnabled(bEditable && !OwnUnitIds.IsEmpty());
    SkillChoice->SetIsEnabled(bEditable && !OwnUnitIds.IsEmpty());
    TargetChoice->SetIsEnabled(bEditable && bHasSelection && !TargetUnitIds.IsEmpty());
    TargetTileChoice->SetIsEnabled(bEditable && bHasSelection && Skill->Kind == ECombatRoundSkillKind::GroundAttack);
    DestinationChoice->SetIsEnabled(bEditable && bHasSelection && Skill->Approach == ECombatRoundApproach::Tile);
    ApplyButton->SetIsEnabled(bEditable && bValidDraft && HasUnappliedChanges());
    ReadyButton->SetIsEnabled(bEditable && bReadyPlans);
    UnreadyButton->SetIsEnabled(bEditable && !OwnUnitIds.IsEmpty());
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
    const int32 Index = SkillChoice ? SkillChoice->GetSelectedIndex() : INDEX_NONE;
    return Coordinator && SkillIds.IsValidIndex(Index) ? Coordinator->FindSkill(SkillIds[Index]) : nullptr;
}

void UCombatRoundPlanningWidget::LoadSelectedCommand()
{
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Coordinator) return;
    const int32 UnitId = GetSelectedUnitId();
    const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
    if (!Unit) return;
    bUpdatingOptions = true;
    const int32 SkillIndex = SkillIds.IndexOfByKey(Unit->Command.SkillId);
    SkillChoice->SetSelectedIndex(FMath::Max(0, SkillIndex));
    RefreshDestinationOptions();
    RefreshTargetOptions();
    if (!TargetUnitIds.IsEmpty()) TargetChoice->SetSelectedIndex(FMath::Max(0, TargetUnitIds.IndexOfByKey(Unit->Command.TargetUnitId)));
    const FIntPoint TargetCoord = SkillIndex == INDEX_NONE ? FIntPoint(Unit->HomeCoord.X, 2) : Unit->Command.TargetCoord;
    TargetTileChoice->SetSelectedIndex(FMath::Max(0, TargetCoords.IndexOfByKey(TargetCoord)));
    DestinationChoice->SetSelectedIndex(FMath::Max(0, DestinationCoords.IndexOfByKey(SkillIndex == INDEX_NONE ? Unit->HomeCoord : Unit->Command.DestinationCoord)));
    bUpdatingOptions = false;
    RefreshSkillDescription();
}

void UCombatRoundPlanningWidget::RefreshSkillDescription()
{
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    if (!Skill)
    {
        SkillDescription->SetText(FText::GetEmpty());
        return;
    }
    const FString TargetLoss = Skill->TargetLoss == ECombatRoundTargetLoss::Cancel ? TEXT("목표 사망 시 취소") : Skill->TargetLoss == ECombatRoundTargetLoss::KeepLocation ? TEXT("목표 상실 시 마지막 위치 유지") : TEXT("목표 상실 시 가까운 적 탐색");
    const FString Approach = Skill->Approach == ECombatRoundApproach::None ? TEXT("제자리 발동") : Skill->Approach == ECombatRoundApproach::Unit ? TEXT("유닛을 향해 실제 접근") : TEXT("지정 타일로 실제 이동");
    FString Text = FString::Printf(TEXT("%s · 시전 %.2f초\n%s · AP %d / 보조 AP %d\n"), *Approach, Skill->WindupSeconds, Skill->bRemainAtDestination ? TEXT("아군 목적지에 잔류") : TEXT("접근 후 원래 칸으로 복귀"), Skill->ActionPointCost, Skill->SubActionPointCost);
    if (Skill->Kind == ECombatRoundSkillKind::Wait) Text = TEXT("이번 라운드의 주요 행동을 하지 않습니다.");
    else if (Skill->Kind == ECombatRoundSkillKind::Guard) Text += FString::Printf(TEXT("보호량 %.0f · 범위 %.0f · 속도순 발동\n이번 라운드 동안 유지 · 겹치면 큰 보호량 적용\n%s"), Skill->Power, Skill->HitRange, *TargetLoss);
    else
    {
        Text += FString::Printf(TEXT("피해 %.0f · %s %.0f\n%s"), Skill->Power, Skill->Kind == ECombatRoundSkillKind::Projectile ? TEXT("투사체 반경") : TEXT("타격 범위"), Skill->Kind == ECombatRoundSkillKind::Projectile ? Skill->ProjectileRadius : Skill->HitRange, *TargetLoss);
        if (Skill->Kind == ECombatRoundSkillKind::Projectile) Text += FString::Printf(TEXT("\n%s · 비행속도 %.0f · 수명 %.1f초\n%s · 발사 후 시전자 사망에도 유지"), Skill->bHoming ? TEXT("유도 투사체") : TEXT("직선 투사체"), Skill->ProjectileSpeed, Skill->ProjectileLifetime, Skill->bTargetOnly ? TEXT("지정 적만 충돌 판정") : TEXT("경로의 적 충돌 판정"));
    }
    SkillDescription->SetText(FText::FromString(Text));
}

void UCombatRoundPlanningWidget::RefreshDestinationOptions()
{
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    const int32 PreviousIndex = DestinationChoice->GetSelectedIndex();
    const FIntPoint PreviousCoord = DestinationCoords.IsValidIndex(PreviousIndex) ? DestinationCoords[PreviousIndex] : FIntPoint::ZeroValue;
    DestinationCoords.Reset();
    DestinationChoice->ClearOptions();
    for (const FIntPoint Coord : TargetCoords)
    {
        if (Skill && Skill->bRemainAtDestination && !CombatRoundRules::IsOwnTerritory(false, Coord)) continue;
        DestinationCoords.Add(Coord);
        DestinationChoice->AddOption(CoordLabel(Coord));
    }
    DestinationChoice->SetSelectedIndex(FMath::Max(0, DestinationCoords.IndexOfByKey(PreviousCoord)));
}

void UCombatRoundPlanningWidget::HandleUnitChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingOptions) return;
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    if (Controller && Controller->GetRoundCoordinator()) RefreshOptions(Controller->GetRoundCoordinator(), Controller->GetRoundParticipantSlot());
    LoadSelectedCommand();
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleSkillChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingOptions) return;
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    RefreshDestinationOptions();
    RefreshTargetOptions();
    if (Coordinator && Skill)
    {
        if (Skill->Kind == ECombatRoundSkillKind::GroundAttack && Skill->Approach == ECombatRoundApproach::Tile && !Skill->bRemainAtDestination && TargetCoords.IsValidIndex(TargetTileChoice->GetSelectedIndex())) DestinationChoice->SetSelectedIndex(DestinationCoords.IndexOfByKey(TargetCoords[TargetTileChoice->GetSelectedIndex()]));
    }
    RefreshSkillDescription();
    RefreshView();
}

void UCombatRoundPlanningWidget::HandleApplyPlan()
{
    ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    if (!Controller || !Skill || GetSelectedUnitId() == INDEX_NONE) return;
    const ACombatRoundCoordinator* Coordinator = Controller->GetRoundCoordinator();
    FText Error;
    if (!Coordinator || !Coordinator->CanPlanCommand(BuildSelectedCommand(), Error))
    {
        RefreshView();
        return;
    }
    Controller->SubmitRoundPlan(BuildSelectedCommand());
    RefreshView();
}

FCombatRoundCommand UCombatRoundPlanningWidget::BuildSelectedCommand() const
{
    FCombatRoundCommand Command;
    Command.UnitId = GetSelectedUnitId();
    if (const FCombatRoundSkill* Skill = GetSelectedSkill()) Command.SkillId = Skill->SkillId;
    if (TargetUnitIds.IsValidIndex(TargetChoice->GetSelectedIndex())) Command.TargetUnitId = TargetUnitIds[TargetChoice->GetSelectedIndex()];
    if (TargetCoords.IsValidIndex(TargetTileChoice->GetSelectedIndex())) Command.TargetCoord = TargetCoords[TargetTileChoice->GetSelectedIndex()];
    if (DestinationCoords.IsValidIndex(DestinationChoice->GetSelectedIndex())) Command.DestinationCoord = DestinationCoords[DestinationChoice->GetSelectedIndex()];
    return Command;
}

bool UCombatRoundPlanningWidget::HasUnappliedChanges() const
{
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    const FCombatRoundSkill* Skill = GetSelectedSkill();
    if (!Coordinator || !Skill) return false;
    const FCombatRoundCommand Draft = BuildSelectedCommand();
    const FCombatRoundUnitView* Unit = Coordinator->GetView().Units.FindByPredicate([Draft](const FCombatRoundUnitView& Entry) { return Entry.UnitId == Draft.UnitId; });
    if (!Unit) return false;
    const FCombatRoundCommand& Applied = Unit->Command;
    if (Draft.SkillId != Applied.SkillId) return true;
    if (Skill->Kind == ECombatRoundSkillKind::Wait) return false;
    if (Skill->Kind == ECombatRoundSkillKind::GroundAttack ? Draft.TargetCoord != Applied.TargetCoord : Draft.TargetUnitId != Applied.TargetUnitId) return true;
    return Skill->Approach == ECombatRoundApproach::Tile && Draft.DestinationCoord != Applied.DestinationCoord;
}

void UCombatRoundPlanningWidget::HandleReady()
{
    FText Error;
    if (!CanReadyPlans(Error))
    {
        RefreshView();
        return;
    }
    if (ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer())) Controller->SetRoundReady(true);
    RefreshView();
}

bool UCombatRoundPlanningWidget::CanReadyPlans(FText& OutError) const
{
    OutError = FText::GetEmpty();
    const ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer());
    const ACombatRoundCoordinator* Coordinator = Controller ? Controller->GetRoundCoordinator() : nullptr;
    if (!Coordinator || OwnUnitIds.IsEmpty() || HasUnappliedChanges()) return false;
    for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
    {
        if (!OwnUnitIds.Contains(Unit.UnitId)) continue;
        FText Error;
        if (!Coordinator->CanPlanCommand(Unit.Command, Error))
        {
            OutError = FText::FromString(FString::Printf(TEXT("아군 #%d의 계획을 확인하세요: %s"), Unit.UnitId, *Error.ToString()));
            return false;
        }
    }
    return true;
}

void UCombatRoundPlanningWidget::HandleUnready()
{
    if (ACombatRoundPlayerController* Controller = Cast<ACombatRoundPlayerController>(GetOwningPlayer())) Controller->SetRoundReady(false);
    RefreshView();
}
