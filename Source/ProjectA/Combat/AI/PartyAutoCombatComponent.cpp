#include "Combat/AI/PartyAutoCombatComponent.h"

#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
#include "Unit/PlayerUnit.h"

namespace
{
    bool TileComesFirst(const ACombatGridTile& A, const ACombatGridTile& B)
    {
        return A.GridCoord.X < B.GridCoord.X || (A.GridCoord.X == B.GridCoord.X && A.GridCoord.Y < B.GridCoord.Y);
    }

    EUnitActionType GetActionType(ECombatActionKind Kind)
    {
        switch (Kind)
        {
        case ECombatActionKind::Move:
            return EUnitActionType::Move;
        case ECombatActionKind::Skill:
            return EUnitActionType::Skill;
        case ECombatActionKind::HealingItem:
            return EUnitActionType::Item;
        default:
            return EUnitActionType::None;
        }
    }
}

UPartyAutoCombatComponent::UPartyAutoCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PendingActionType = EUnitActionType::None;
}

void UPartyAutoCombatComponent::InitializeCombat(ACombatManager* InCombatManager)
{
    Stop();
    APlayerUnit* Unit = GetPlayerUnit();
    CombatManager = IsValid(Unit) && Unit->HasAuthority() && IsValid(InCombatManager) && InCombatManager->HasAuthority() && InCombatManager->GetWorld() == Unit->GetWorld() ? InCombatManager : nullptr;
}

APlayerUnit* UPartyAutoCombatComponent::GetPlayerUnit() const
{
    return Cast<APlayerUnit>(GetOwner());
}

void UPartyAutoCombatComponent::StartTurn()
{
    Stop();
    APlayerUnit* Unit = GetPlayerUnit();
    ACombatManager* Manager = CombatManager.Get();
    UCombatActionAuthority* Authority = Manager ? Manager->GetActionAuthority() : nullptr;
    if (!IsValid(Unit) || !IsValid(Authority))
    {
        return;
    }
    ActiveTurn.CombatId = Authority->GetCombatInstanceId();
    ActiveTurn.ControlSessionId = Unit->GetAIControlSessionId();
    ActiveTurn.TurnSerial = Manager->GetTurnSerial();
    ActiveTurn.Generation = Generation;
    if (!MatchesCurrentContext(ActiveTurn))
    {
        Stop();
        return;
    }
    ScheduleDecision(false);
}

void UPartyAutoCombatComponent::Stop()
{
    ++Generation;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DecisionTimer);
    }
    ActiveTurn = FTurnContext();
    PendingActionContext = FTurnContext();
    PendingActionType = EUnitActionType::None;
    bWaitingForAction = false;
    bFinishTurnPending = false;
    bEndTurnRequested = false;
}

void UPartyAutoCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Stop();
    CombatManager.Reset();
    Super::EndPlay(EndPlayReason);
}

bool UPartyAutoCombatComponent::MatchesCurrentContext(const FTurnContext& Context) const
{
    APlayerUnit* Unit = GetPlayerUnit();
    ACombatManager* Manager = CombatManager.Get();
    const UCombatActionAuthority* Authority = Manager ? Manager->GetActionAuthority() : nullptr;
    return Context.Generation == Generation && Context.CombatId.IsValid() && Context.ControlSessionId.IsValid() && IsValid(Unit) && Unit->HasAuthority() && Unit->IsServerAIControlled() && Unit->IsActiveTurn() && Unit->IsUnitAlive() && Unit->GetAIControlSessionId() == Context.ControlSessionId && IsValid(Manager) && Manager->HasAuthority() && Manager->GetWorld() == Unit->GetWorld() && Manager->IsCombatActive() && !Manager->IsAwaitingTurnCheckpoint() && Manager->GetCurrentUnit() == Unit && Manager->GetRegisteredUnits().Contains(Unit) && Manager->GetTurnSerial() == Context.TurnSerial && IsValid(Authority) && Authority->GetCombatInstanceId() == Context.CombatId && Authority->GetUnitId(Unit).IsValid();
}

void UPartyAutoCombatComponent::ScheduleDecision(bool bFinishTurn)
{
    if (!MatchesCurrentContext(ActiveTurn) || bEndTurnRequested || !GetWorld())
    {
        return;
    }
    bFinishTurnPending |= bFinishTurn;
    const FTurnContext ScheduledContext = ActiveTurn;
    GetWorld()->GetTimerManager().ClearTimer(DecisionTimer);
    // Never decide recursively inside GAS completion, turn activation or checkpoint publication.
    // GAS 완료, 턴 활성화 또는 체크포인트 게시 안에서 재귀적으로 판단하지 않습니다.
    DecisionTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, ScheduledContext]()
    {
        if (!MatchesCurrentContext(ScheduledContext))
        {
            return;
        }
        DecideAction();
    }));
}

void UPartyAutoCombatComponent::HandleActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
    if (!bWaitingForAction || PendingActionType != ActionType || !MatchesCurrentContext(PendingActionContext))
    {
        return;
    }
    bWaitingForAction = false;
    PendingActionType = EUnitActionType::None;
    PendingActionContext = FTurnContext();
    ScheduleDecision(Result != EUnitActionResult::Succeeded || GetPlayerUnit()->MustEndTurnAfterCurrentAction());
}

void UPartyAutoCombatComponent::DecideAction()
{
    APlayerUnit* Unit = GetPlayerUnit();
    if (!MatchesCurrentContext(ActiveTurn) || bWaitingForAction || bEndTurnRequested || Unit->IsBusy())
    {
        return;
    }
    FCombatActionRequest Request;
    Request.Kind = ECombatActionKind::EndTurn;
    if (!bFinishTurnPending && !Unit->MustEndTurnAfterCurrentAction())
    {
        if (!ChooseHealingItem(Request) && !ChooseSkill(Request))
        {
            ChooseMove(Request);
        }
    }
    SubmitDecision(Request);
}

bool UPartyAutoCombatComponent::ChooseHealingItem(FCombatActionRequest& Request) const
{
    APlayerUnit* Unit = GetPlayerUnit();
    ACombatManager* Manager = CombatManager.Get();
    ACombatGridTile* Tile = Unit->GetCurrentTile();
    if (!Unit->CanUseHealingItem(Unit) || !IsValid(Tile) || Manager->GetTileByCoord(Tile->GridCoord) != Tile || Tile->GetOccupyingUnit() != Unit)
    {
        return false;
    }
    Request.Kind = ECombatActionKind::HealingItem;
    Request.TargetCoord = Tile->GridCoord;
    Request.TargetUnitId = Manager->GetActionAuthority()->GetUnitId(Unit);
    return true;
}

TArray<ACombatGridTile*> UPartyAutoCombatComponent::GetSortedCombatTiles() const
{
    TArray<ACombatGridTile*> Tiles;
    APlayerUnit* Unit = GetPlayerUnit();
    ACombatManager* Manager = CombatManager.Get();
    ACombatGridTile* CurrentTile = Unit ? Unit->GetCurrentTile() : nullptr;
    ACombatGridManager* Grid = IsValid(CurrentTile) ? CurrentTile->GetGridManager() : nullptr;
    if (!IsValid(Grid) || !IsValid(Manager) || Manager->GetTileByCoord(CurrentTile->GridCoord) != CurrentTile)
    {
        return Tiles;
    }
    for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Grid->TileMap)
    {
        ACombatGridTile* Tile = Entry.Value;
        if (IsValid(Tile) && Tile->GetWorld() == Unit->GetWorld() && Tile->GetGridManager() == Grid && Manager->GetTileByCoord(Entry.Key) == Tile)
        {
            Tiles.Add(Tile);
        }
    }
    Tiles.Sort(TileComesFirst);
    return Tiles;
}

bool UPartyAutoCombatComponent::ChooseSkill(FCombatActionRequest& Request) const
{
    APlayerUnit* Unit = GetPlayerUnit();
    ACombatManager* Manager = CombatManager.Get();
    UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        return false;
    }
    const TArray<ACombatGridTile*> Tiles = GetSortedCombatTiles();
    double BestScore = -TNumericLimits<double>::Max();
    bool bFound = false;
    // Strict improvement preserves equipped-skill order and coordinate order for equal scores.
    // 점수가 더 높은 경우만 교체하여 동점에는 장착 스킬 순서와 좌표 순서를 유지합니다.
    for (USkillDefinitionDataAsset* Skill : Unit->GetEquippedSkillDataAssets())
    {
        FGameplayAbilitySpec* Spec = IsValid(Skill) && Skill->AbilityClass ? ASC->FindAbilitySpecFromClass(Skill->AbilityClass) : nullptr;
        if (!UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->GetPrimaryAssetId().IsValid() || Skill->ActionPointCost <= 0 || !Spec || Spec->IsActive() || !Unit->HasEnoughActionPoint(Skill->ActionPointCost))
        {
            continue;
        }
        for (ACombatGridTile* Tile : Tiles)
        {
            if (!UCombatTargetingLibrary::IsValidSkillTarget(Unit, Skill, Tile))
            {
                continue;
            }
            AUnitBase* SelectedUnit = Tile->GetOccupyingUnit();
            if (SelectedUnit && !Manager->GetRegisteredUnits().Contains(SelectedUnit))
            {
                continue;
            }
            const TArray<AUnitBase*> Targets = UCombatTargetingLibrary::ResolveSkillAreaTargets(Unit, Skill, Tile);
            if (Targets.IsEmpty())
            {
                continue;
            }
            bool bOnlyRegisteredEnemies = true;
            double LowHPScore = 0.0;
            for (AUnitBase* Target : Targets)
            {
                ACombatGridTile* TargetTile = IsValid(Target) ? Target->GetCurrentTile() : nullptr;
                if (!IsValid(Target) || Target == Unit || Target->GetTeam() == Unit->GetTeam() || !Target->IsUnitAlive() || !Manager->GetRegisteredUnits().Contains(Target) || !IsValid(TargetTile) || Manager->GetTileByCoord(TargetTile->GridCoord) != TargetTile || TargetTile->GetOccupyingUnit() != Target || !Target->GetAttributeSet())
                {
                    bOnlyRegisteredEnemies = false;
                    break;
                }
                const double MaxHP = Target->GetAttributeSet()->GetMaxHP();
                if (MaxHP > 0.0)
                {
                    LowHPScore += (1.0 - FMath::Clamp(Target->GetAttributeSet()->GetHP() / MaxHP, 0.0, 1.0)) * 50.0;
                }
            }
            if (!bOnlyRegisteredEnemies)
            {
                continue;
            }
            const double Distance = FVector::Dist2D(Unit->GetActorLocation(), Tile->GetActorLocation());
            const double Score = 100.0 * Targets.Num() / Skill->ActionPointCost + LowHPScore / Targets.Num() - Distance / 200.0 * 10.0;
            if (Score > BestScore)
            {
                BestScore = Score;
                bFound = true;
                Request.Kind = ECombatActionKind::Skill;
                Request.SkillId = Skill->GetPrimaryAssetId();
                Request.TargetCoord = Tile->GridCoord;
                const bool bNeedsUnit = Skill->bMoveToTarget || Skill->TargetRule == ESkillTargetRule::EnemyUnit || Skill->TargetRule == ESkillTargetRule::AllyUnit || Skill->TargetRule == ESkillTargetRule::AnyUnit;
                Request.TargetUnitId = bNeedsUnit ? Manager->GetActionAuthority()->GetUnitId(SelectedUnit) : FGuid();
            }
        }
    }
    return bFound;
}

bool UPartyAutoCombatComponent::ChooseMove(FCombatActionRequest& Request) const
{
    APlayerUnit* Unit = GetPlayerUnit();
    ACombatManager* Manager = CombatManager.Get();
    ACombatGridTile* CurrentTile = Unit->GetCurrentTile();
    if (!Unit->HasEnoughSubActionPoint(1) || !IsValid(CurrentTile) || Manager->GetTileByCoord(CurrentTile->GridCoord) != CurrentTile)
    {
        return false;
    }
    const auto NearestEnemyDistance = [Unit, Manager](const FVector& Position)
    {
        double Distance = TNumericLimits<double>::Max();
        for (AUnitBase* Opponent : Manager->GetRegisteredUnits())
        {
            ACombatGridTile* Tile = IsValid(Opponent) ? Opponent->GetCurrentTile() : nullptr;
            if (IsValid(Opponent) && Opponent->IsUnitAlive() && Opponent->GetTeam() != Unit->GetTeam() && IsValid(Tile) && Manager->GetTileByCoord(Tile->GridCoord) == Tile && Tile->GetOccupyingUnit() == Opponent)
            {
                Distance = FMath::Min(Distance, FVector::Dist2D(Position, Tile->GetActorLocation()));
            }
        }
        return Distance;
    };
    const double CurrentDistance = NearestEnemyDistance(CurrentTile->GetActorLocation());
    if (CurrentDistance == TNumericLimits<double>::Max())
    {
        return false;
    }
    const TArray<ACombatGridTile*> Reachable = Manager->CalculateReachableMoveTiles(Unit);
    double BestGain = 1.0;
    bool bFound = false;
    for (ACombatGridTile* Tile : GetSortedCombatTiles())
    {
        if (!Reachable.Contains(Tile) || Tile->GetOccupyingUnit())
        {
            continue;
        }
        const double Gain = CurrentDistance - NearestEnemyDistance(Tile->GetActorLocation());
        if (Gain > BestGain)
        {
            BestGain = Gain;
            bFound = true;
            Request.Kind = ECombatActionKind::Move;
            Request.TargetCoord = Tile->GridCoord;
        }
    }
    return bFound;
}

void UPartyAutoCombatComponent::SubmitDecision(FCombatActionRequest Request)
{
    APlayerUnit* Unit = GetPlayerUnit();
    if (!MatchesCurrentContext(ActiveTurn) || Unit->IsBusy() || bWaitingForAction || bEndTurnRequested || RequestSequence == MAX_int64)
    {
        return;
    }
    ACombatManager* Manager = CombatManager.Get();
    UCombatActionAuthority* Authority = Manager->GetActionAuthority();
    Request.Version = 1;
    Request.RunId = Authority->GetRunIdentity().RunId;
    Request.HostEpoch = Authority->GetRunIdentity().HostEpoch;
    Request.CombatInstanceId = Authority->GetCombatInstanceId();
    Request.TurnSerial = Manager->GetTurnSerial();
    Request.UnitId = Authority->GetUnitId(Unit);
    Request.ParticipantBindingId.Invalidate();
    Request.RequestSequence = ++RequestSequence;
    LastDecision = Request;
    const FTurnContext SubmittedContext = ActiveTurn;
    bEndTurnRequested = Request.Kind == ECombatActionKind::EndTurn;
    bWaitingForAction = !bEndTurnRequested;
    PendingActionType = GetActionType(Request.Kind);
    PendingActionContext = SubmittedContext;
    // Completion may arrive before ExecuteServerAI returns, so publish the waiting context first.
    // ExecuteServerAI 반환 전에 완료될 수 있으므로 대기 문맥을 먼저 설정합니다.
    const FCombatActionResponse Response = Authority->ExecuteServerAI(Unit, Request, SubmittedContext.ControlSessionId);
    if (!MatchesCurrentContext(SubmittedContext) || bEndTurnRequested)
    {
        return;
    }
    if (Response.Result != ECombatRequestResult::Accepted || (bWaitingForAction && !Unit->IsBusy()))
    {
        bWaitingForAction = false;
        PendingActionType = EUnitActionType::None;
        PendingActionContext = FTurnContext();
        ScheduleDecision(true);
    }
}
