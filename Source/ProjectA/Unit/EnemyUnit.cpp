#include "EnemyUnit.h"

#include "EngineUtils.h"

#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Grid/Combat/CombatGridTile.h"

AEnemyUnit::AEnemyUnit()
{
    InitMaxHP = 150.f;
    MaxActionPoint = 2;
}

void AEnemyUnit::OnTurnStart()
{
    Super::OnTurnStart();

    SetTurnState(EEnemyTurnState::StartTurn);
}

void AEnemyUnit::OnSkillFinished()
{
    Super::OnSkillFinished();

    if (CurrentTurnState != EEnemyTurnState::WaitSkillComplete)
    {
        return;
    }

    // If the turn can continue after the current action,
    // proceed to the next decision after returning to the original tile
    if (!MustEndTurnAfterCurrentAction())
    {
        bPendingNextActionAfterReturn = true;
        return;
    }

    // If turn end is already scheduled due to AP depletion,
    // finish the turn after returning
    bPendingNextActionAfterReturn = false;
}

void AEnemyUnit::OnReturnToOriginalTileFinished()
{
    Super::OnReturnToOriginalTileFinished();

    if (CurrentTurnState != EEnemyTurnState::WaitSkillComplete)
    {
        return;
    }

    if (bPendingNextActionAfterReturn && !MustEndTurnAfterCurrentAction())
    {
        bPendingNextActionAfterReturn = false;
        SetTurnState(EEnemyTurnState::DecideAction);
        return;
    }

    bPendingNextActionAfterReturn = false;
    SetTurnState(EEnemyTurnState::EndTurn);
}

void AEnemyUnit::SetTurnState(EEnemyTurnState NewState)
{
    CurrentTurnState = NewState;

    switch (CurrentTurnState)
    {
    case EEnemyTurnState::StartTurn:
    {
        EnterStartTurnState();
        break;
    }
    case EEnemyTurnState::DecideAction:
    {
        EnterDecideActionState();
        break;
    }
    case EEnemyTurnState::Move:
    {
        EnterMoveState();
        break;
    }
    case EEnemyTurnState::WaitMoveComplete:
    {
        EnterWaitMoveCompleteState();
        break;
    }
    case EEnemyTurnState::Skill:
    {
        EnterSkillState();
        break;
    }
    case EEnemyTurnState::WaitSkillComplete:
    {
        EnterWaitSkillCompleteState();
        break;
    }
    case EEnemyTurnState::EndTurn:
    {
        EnterEndTurnState();
        break;
    }
    default:
    {
        break;
    }
    }
}

EEnemyTurnState AEnemyUnit::GetTurnState() const
{
    return CurrentTurnState;
}

void AEnemyUnit::EnterStartTurnState()
{
    CurrentTarget = nullptr;
    CurrentTargetTile = nullptr;
    bPendingNextActionAfterReturn = false;
    CurrentDecision = FEnemyActionDecision();

    SetTurnState(EEnemyTurnState::DecideAction);
}

void AEnemyUnit::EnterDecideActionState()
{
    const TArray<TSubclassOf<UGameplayAbility>> SkillClasses = GetAvailableSkillAbilityClasses();

    bool bHasValidSkill = false;

    for (const TSubclassOf<UGameplayAbility>& SkillClass : SkillClasses)
    {
        if (SkillClass)
        {
            bHasValidSkill = true;
            break;
        }
    }

    if (!bHasValidSkill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] No available skill | Unit=%s"), *GetName());
    }

    CurrentDecision = DecideBestAction();

    //UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] Decision | Unit=%s | ActionType=%d | SkillData=%s | Ability=%s | Target=%s | Score=%.2f"), *GetName(), static_cast<int32>(CurrentDecision.ActionType), *GetNameSafe(CurrentDecision.SkillData), CurrentDecision.SkillData ? *GetNameSafe(CurrentDecision.SkillData->AbilityClass) : TEXT("None"), *GetNameSafe(CurrentDecision.TargetUnit), CurrentDecision.Score);

    ApplyDecision(CurrentDecision);
}

void AEnemyUnit::EnterMoveState()
{
    // Movement action evaluation is not implemented yet,
    // so fallback safely to ending the turn
    if (!CurrentTargetTile)
    {
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    StartMoveAction(CurrentTargetTile);
    SetTurnState(EEnemyTurnState::WaitMoveComplete);
}

void AEnemyUnit::EnterWaitMoveCompleteState()
{
}

void AEnemyUnit::EnterSkillState()
{
    if (!CurrentDecision.SkillData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EnterSkillState Failed | Reason=NoSkillData | Unit=%s"), *GetName());
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    if (!CurrentDecision.SkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EnterSkillState Failed | Reason=NoAbility | Unit=%s"), *GetName());
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    if (!CurrentTarget || !CurrentTarget->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EnterSkillState Failed | Reason=InvalidTarget | Unit=%s"), *GetName());
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    if (!CurrentTargetTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EnterSkillState Failed | Reason=NoTargetTile | Unit=%s"), *GetName());
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    if (!HasEnoughActionPoint(CurrentDecision.SkillData->ActionPointCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EnterSkillState Failed | Reason=NoAP | Unit=%s"), *GetName());
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    //UE_LOG(LogTemp, Log, TEXT("[EnemyAI] ExecuteSkill | Unit=%s | SkillData=%s | TargetTile=(%d,%d)"), *GetName(), *GetNameSafe(CurrentDecision.SkillData), CurrentTargetTile ? CurrentTargetTile->GridCoord.X : -1, CurrentTargetTile ? CurrentTargetTile->GridCoord.Y : -1);

    StartSkill(CurrentDecision.SkillData, CurrentTargetTile);
    SetTurnState(EEnemyTurnState::WaitSkillComplete);
}

void AEnemyUnit::EnterWaitSkillCompleteState()
{
}

void AEnemyUnit::EnterEndTurnState()
{
    FinishEnemyTurn();
}

void AEnemyUnit::FinishEnemyTurn()
{
    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());

    if (!PC)
    {
        return;
    }

    PC->RequestEndTurn();
}

FEnemyActionDecision AEnemyUnit::DecideBestAction() const
{
    FEnemyActionDecision BestDecision;
    BestDecision.Score = -TNumericLimits<float>::Max();

    const FEnemyActionDecision SkillDecision = EvaluateSkillAction();
    const FEnemyActionDecision WaitDecision = EvaluateWaitAction();

    if (SkillDecision.Score > BestDecision.Score)
    {
        BestDecision = SkillDecision;
    }

    if (WaitDecision.Score > BestDecision.Score)
    {
        BestDecision = WaitDecision;
    }

    return BestDecision;
}

FEnemyActionDecision AEnemyUnit::EvaluateSkillAction() const
{
    FEnemyActionDecision BestDecision;
    BestDecision.ActionType = EEnemyActionType::Skill;
    BestDecision.Score = -TNumericLimits<float>::Max();

    const TArray<TSubclassOf<UGameplayAbility>> SkillClasses = GetAvailableSkillAbilityClasses();

    for (const TSubclassOf<UGameplayAbility>& SkillClass : SkillClasses)
    {
        if (!SkillClass)
        {
            continue;
        }

        USkillDefinitionDataAsset* SkillData = FindSkillDataByAbilityClass(SkillClass);

        if (!SkillData)
        {
            continue;
        }

        const FEnemyActionDecision CandidateDecision = EvaluateSkillCandidate(SkillData);

        if (CandidateDecision.Score > BestDecision.Score)
        {
            BestDecision = CandidateDecision;
        }
    }

    return BestDecision;
}

FEnemyActionDecision AEnemyUnit::EvaluateSkillCandidate(USkillDefinitionDataAsset* SkillData) const
{
    FEnemyActionDecision Decision;
    Decision.ActionType = EEnemyActionType::Skill;
    Decision.SkillData = SkillData;
    Decision.Score = -TNumericLimits<float>::Max();

    if (!SkillData)
    {
        return Decision;
    }

    AUnitBase* BestTarget = FindBestSkillTarget(SkillData);

    if (!BestTarget)
    {
        return Decision;
    }

    ACombatGridTile* TargetTile = BestTarget->GetCurrentTile();

    if (!TargetTile)
    {
        return Decision;
    }

    Decision.TargetUnit = BestTarget;
    Decision.TargetTile = TargetTile;
    Decision.Score = SkillBaseScore + EvaluateSkillTargetScore(SkillData, BestTarget);

    return Decision;
}

FEnemyActionDecision AEnemyUnit::EvaluateWaitAction() const
{
    FEnemyActionDecision Decision;
    Decision.ActionType = EEnemyActionType::Wait;
    Decision.Score = WaitBaseScore;

    return Decision;
}

float AEnemyUnit::EvaluateSkillTargetScore(USkillDefinitionDataAsset* SkillData, AUnitBase* Candidate) const
{
    if (!SkillData || !Candidate)
    {
        return -TNumericLimits<float>::Max();
    }

    if (SkillData->AbilityClass == DefaultAttackAbilityClass)
    {
        // Temporary weight for default attack, to be replaced with slot-based weights
        return EvaluateDefaultAttackScore(Candidate) + 100000.f;
    }

    return EvaluateSkillSlotScore(SkillData, Candidate);
}

AUnitBase* AEnemyUnit::FindBestSkillTarget(USkillDefinitionDataAsset* SkillData) const
{
    AUnitBase* BestTarget = nullptr;
    float BestScore = -TNumericLimits<float>::Max();

    // TODO: Apply the same target-rule and front-protection validation used by player skill targeting.
    for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
    {
        AUnitBase* Candidate = *It;

        if (!Candidate)
        {
            continue;
        }

        if (Candidate == this)
        {
            continue;
        }

        if (!Candidate->IsUnitAlive())
        {
            continue;
        }

        if (Candidate->GetTeam() != ETeam::Player)
        {
            continue;
        }

        const float Score = EvaluateSkillTargetScore(SkillData, Candidate);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Candidate;
        }
    }

    return BestTarget;
}

float AEnemyUnit::EvaluateDefaultAttackScore(AUnitBase* Candidate) const
{
    if (!Candidate)
    {
        return -TNumericLimits<float>::Max();
    }

    const float Distance = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());

    return -(Distance * DistanceWeight);
}

float AEnemyUnit::EvaluateSkillSlotScore(USkillDefinitionDataAsset* SkillData, AUnitBase* Candidate) const
{
    if (!SkillData || !SkillData->AbilityClass || !Candidate)
    {
        return -TNumericLimits<float>::Max();
    }

    const float Distance = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
    float Score = -(Distance * DistanceWeight);

    const int32 SkillIndex = EquippedSkillAbilityClasses.IndexOfByKey(SkillData->AbilityClass);

    switch (SkillIndex)
    {
    case 0:
    {
        // Skill Slot 1:
        // Prefer low HP targets
        Score += EvaluateLowHPScore(Candidate);
        break;
    }
    case 1:
    {
        // Skill Slot 2:
        // Prefer high HP targets
        Score += EvaluateHighHPScore(Candidate);
        break;
    }
    case 2:
    {
        // Skill Slot 3:
        // Currently distance-only
        break;
    }
    case 3:
    {
        // Skill Slot 4:
        // Currently distance-only
        break;
    }
    default:
    {
        // Use default rule if not found in EquippedSkillAbilityClasses
        break;
    }
    }

    return Score;
}

float AEnemyUnit::EvaluateLowHPScore(AUnitBase* Candidate) const
{
    if (!Candidate)
    {
        return 0.0f;
    }

    const UAS_Unit* CandidateAttributeSet = Candidate->GetAttributeSet();

    if (!CandidateAttributeSet)
    {
        return 0.0f;
    }

    const float HP = CandidateAttributeSet->GetHP();
    const float MaxHP = CandidateAttributeSet->GetMaxHP();

    if (MaxHP <= 0.0f)
    {
        return 0.0f;
    }

    const float HPRatio = HP / MaxHP;

    return (1.0f - HPRatio) * LowHPWeight;
}

float AEnemyUnit::EvaluateHighHPScore(AUnitBase* Candidate) const
{
    if (!Candidate)
    {
        return 0.0f;
    }

    const UAS_Unit* CandidateAttributeSet = Candidate->GetAttributeSet();

    if (!CandidateAttributeSet)
    {
        return 0.0f;
    }

    const float HP = CandidateAttributeSet->GetHP();
    const float MaxHP = CandidateAttributeSet->GetMaxHP();

    if (MaxHP <= 0.0f)
    {
        return 0.0f;
    }

    const float HPRatio = HP / MaxHP;

    return HPRatio * HighHPWeight;
}

void AEnemyUnit::ApplyDecision(const FEnemyActionDecision& Decision)
{
    CurrentDecision = Decision;
    CurrentTarget = Decision.TargetUnit;
    CurrentTargetTile = Decision.TargetTile;

    switch (Decision.ActionType)
    {
    case EEnemyActionType::Skill:
    {
        SetTurnState(EEnemyTurnState::Skill);
        break;
    }
    case EEnemyActionType::Move:
    {
        SetTurnState(EEnemyTurnState::Move);
        break;
    }
    case EEnemyActionType::Wait:
    {
        SetTurnState(EEnemyTurnState::EndTurn);
        break;
    }
    default:
    {
        SetTurnState(EEnemyTurnState::EndTurn);
        break;
    }
    }
}
