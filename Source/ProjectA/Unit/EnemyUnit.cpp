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

    // 현재 행동 종료 후에도 턴 지속 가능하면
    // 복귀 완료 뒤 다음 행동 판단으로 넘어간다.
    if (!MustEndTurnAfterCurrentAction())
    {
        bPendingNextActionAfterReturn = true;
        return;
    }

    // AP 고갈로 턴 종료가 예약된 상태면
    // 복귀 완료 후 바로 종료한다.
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
    // 이동 행동 평가는 아직 구현하지 않았으므로
    // 현재 단계에서는 안전하게 턴 종료로 보낸다.
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

        //UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EvaluateSkillAction | SkillClass=%s | SkillData=%s"), *GetNameSafe(SkillClass), *GetNameSafe(SkillData));

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

    //UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] EvaluateSkillCandidate | SkillData=%s | TargetUnit=%s | TargetTile=%s | Score=%.2f"), *GetNameSafe(Decision.SkillData), *GetNameSafe(Decision.TargetUnit), *GetNameSafe(Decision.TargetTile), Decision.Score);

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
		//TODO: 테스트용 기본공격 가중치, 추후 스킬 슬롯별 가중치로 대체 필요
        return EvaluateDefaultAttackScore(Candidate) + 100000.f;
    }

    return EvaluateSkillSlotScore(SkillData, Candidate);
}

AUnitBase* AEnemyUnit::FindBestSkillTarget(USkillDefinitionDataAsset* SkillData) const
{
    AUnitBase* BestTarget = nullptr;
    float BestScore = -TNumericLimits<float>::Max();

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
        // 저체력 적 우선
        Score += EvaluateLowHPScore(Candidate);
        break;
    }
    case 1:
    {
        // Skill Slot 2:
        // 고체력 적 우선
        Score += EvaluateHighHPScore(Candidate);
        break;
    }
    case 2:
    {
        // Skill Slot 3:
        // 현재는 거리만 사용
        break;
    }
    case 3:
    {
        // Skill Slot 4:
        // 현재는 거리만 사용
        break;
    }
    default:
    {
        // EquippedSkillAbilityClasses에 없는 스킬이면 기본 규칙만 사용
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

    //UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] ApplyDecision | ActionType=%d | SkillData=%s | CurrentTarget=%s | CurrentTargetTile=%s"), static_cast<int32>(Decision.ActionType), *GetNameSafe(Decision.SkillData), *GetNameSafe(CurrentTarget), *GetNameSafe(CurrentTargetTile));

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