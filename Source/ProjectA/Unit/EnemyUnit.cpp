#include "EnemyUnit.h"

#include "EngineUtils.h"

#include "Combat/CombatManager.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Grid/Combat/CombatGridTile.h"

AEnemyUnit::AEnemyUnit()
{
    InitMaxHP = 150.f;
    MaxActionPoint = 2;
}

void AEnemyUnit::OnTurnStart()
{
    if (!HasAuthority())
    {
        return;
    }

    Super::OnTurnStart();

    SetTurnState(EEnemyTurnState::StartTurn);
}

void AEnemyUnit::OnTurnEnd()
{
    if (!HasAuthority())
    {
        return;
    }

    Super::OnTurnEnd();
    GetWorldTimerManager().ClearTimer(ActionContinuationTimer);
    CurrentTurnState = EEnemyTurnState::None;
    CurrentDecision = FEnemyActionDecision();
    CurrentTarget = nullptr;
    CurrentTargetTile = nullptr;
}

void AEnemyUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(ActionContinuationTimer);
    Super::EndPlay(EndPlayReason);
}

void AEnemyUnit::OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
    Super::OnUnitActionCompleted(ActionType, Result);

    if (!HasAuthority() || !IsActiveTurn() || !IsUnitAlive())
    {
        return;
    }

    if (CurrentTurnState != EEnemyTurnState::WaitSkillComplete && CurrentTurnState != EEnemyTurnState::WaitMoveComplete)
    {
        return;
    }

    // Defer decisions so synchronous ability completion cannot reactivate GAS recursively.
    // 동기 어빌리티 종료가 GAS를 재귀 활성화하지 않도록 다음 판단을 지연합니다.
    GetWorldTimerManager().ClearTimer(ActionContinuationTimer);
    ActionContinuationTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Result]()
    {
        if (!IsActiveTurn() || !IsUnitAlive() || IsBusy())
        {
            return;
        }

        if (Result != EUnitActionResult::Succeeded || MustEndTurnAfterCurrentAction())
        {
            SetTurnState(EEnemyTurnState::EndTurn);
            return;
        }

        SetTurnState(EEnemyTurnState::DecideAction);
    }));
}

void AEnemyUnit::SetTurnState(EEnemyTurnState NewState)
{
    // Replicated turn flags must never start a client-side AI state machine.
    // 복제된 턴 플래그가 클라이언트 AI 상태 머신을 실행하면 안 됩니다.
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentTurnState == NewState)
    {
        return;
    }

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
    // Validate the selected destination before starting movement.
    // 이동 시작 전에 선택한 목적지를 확인합니다.
    ACombatManager* Manager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));
    if (!CurrentTargetTile || !Manager || !Manager->CalculateReachableMoveTiles(this).Contains(CurrentTargetTile))
    {
        SetTurnState(EEnemyTurnState::EndTurn);
        return;
    }

    SetTurnState(EEnemyTurnState::WaitMoveComplete);
    const uint32 PreviousActionSerial = CurrentActionSerial;
    StartMoveAction(CurrentTargetTile);
    if (PreviousActionSerial == CurrentActionSerial)
    {
        OnUnitActionCompleted(EUnitActionType::Move, EUnitActionResult::Failed);
    }
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

    if (!UCombatTargetingLibrary::IsValidSkillTarget(this, CurrentDecision.SkillData, CurrentTargetTile))
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

    SetTurnState(EEnemyTurnState::WaitSkillComplete);
    const uint32 PreviousActionSerial = CurrentActionSerial;
    StartSkill(CurrentDecision.SkillData, CurrentTargetTile);
    if (PreviousActionSerial == CurrentActionSerial)
    {
        OnUnitActionCompleted(EUnitActionType::Skill, EUnitActionResult::Failed);
    }
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
    if (!IsActiveTurn() || IsBusy())
    {
        return;
    }

    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));
    if (CombatManager && CombatManager->GetCurrentUnit() == this)
    {
        CombatManager->RequestEndTurnForUnit(this);
    }
}

FEnemyActionDecision AEnemyUnit::DecideBestAction() const
{
    FEnemyActionDecision BestDecision;
    BestDecision.Score = -TNumericLimits<float>::Max();

    const FEnemyActionDecision SkillDecision = EvaluateSkillAction();
    const FEnemyActionDecision WaitDecision = EvaluateWaitAction();
    const FEnemyActionDecision MoveDecision = EvaluateMoveAction();
    if (MoveDecision.Score > BestDecision.Score)
    {
        BestDecision = MoveDecision;
    }

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

    if (!SkillData || !SkillData->AbilityClass || SkillData->ActionPointCost <= 0 || !HasEnoughActionPoint(SkillData->ActionPointCost))
    {
        return Decision;
    }

    ACombatGridTile* TargetTile = FindBestSkillTargetTile(SkillData);

    if (!TargetTile)
    {
        return Decision;
    }

    Decision.TargetUnit = TargetTile->GetOccupyingUnit();
    Decision.TargetTile = TargetTile;
    // Compare useful target coverage per AP; stable ties retain the default attack.
    // AP당 유효 대상 수를 비교하며 동점이면 기본 공격을 유지합니다.
    const int32 TargetCount = FMath::Max(1, UCombatTargetingLibrary::ResolveSkillAreaTargets(const_cast<AEnemyUnit*>(this), SkillData, TargetTile).Num());
    Decision.Score = SkillBaseScore * TargetCount / SkillData->ActionPointCost + EvaluateSkillTileScore(SkillData, TargetTile);

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
        return EvaluateDefaultAttackScore(Candidate) + EvaluateLowHPScore(Candidate);
    }

    return EvaluateSkillSlotScore(SkillData, Candidate);
}

ACombatGridTile* AEnemyUnit::FindBestSkillTargetTile(USkillDefinitionDataAsset* SkillData) const
{
    ACombatGridTile* BestTarget = nullptr;
    float BestScore = -TNumericLimits<float>::Max();

    // Include empty tiles and allies using the same eligibility as player selection.
    // 플레이어 선택과 같은 허용 규칙으로 빈 타일과 아군도 후보에 포함합니다.
    for (TActorIterator<ACombatGridTile> It(GetWorld()); It; ++It)
    {
        ACombatGridTile* Candidate = *It;
        if (!UCombatTargetingLibrary::IsValidSkillTarget(this, SkillData, Candidate))
        {
            continue;
        }

        const float Score = EvaluateSkillTileScore(SkillData, Candidate);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Candidate;
        }
    }

    return BestTarget;
}

float AEnemyUnit::EvaluateSkillTileScore(USkillDefinitionDataAsset* SkillData, ACombatGridTile* Candidate) const
{
    if (AUnitBase* Unit = Candidate->GetOccupyingUnit())
    {
        return EvaluateSkillTargetScore(SkillData, Unit);
    }

    float Score = -(FVector::Dist(GetActorLocation(), Candidate->GetActorLocation()) / 200.0f) * DistanceWeight;
    return Score;
}

float AEnemyUnit::EvaluateDefaultAttackScore(AUnitBase* Candidate) const
{
    if (!Candidate)
    {
        return -TNumericLimits<float>::Max();
    }

    const float Distance = (FVector::Dist(GetActorLocation(), Candidate->GetActorLocation()) / 200.0f);

    return -(Distance * DistanceWeight);
}

float AEnemyUnit::EvaluateSkillSlotScore(USkillDefinitionDataAsset* SkillData, AUnitBase* Candidate) const
{
    if (!SkillData || !SkillData->AbilityClass || !Candidate)
    {
        return -TNumericLimits<float>::Max();
    }

    const float Distance = (FVector::Dist(GetActorLocation(), Candidate->GetActorLocation()) / 200.0f);
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

FEnemyActionDecision AEnemyUnit::EvaluateMoveAction() const
{
    FEnemyActionDecision Decision;
    Decision.ActionType = EEnemyActionType::Move;
    if (!HasEnoughSubActionPoint(1) || !GetCurrentTile())
    {
        return Decision;
    }
    ACombatManager* Manager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));
    if (!Manager)
    {
        return Decision;
    }
    const auto NearestOpponentDistance = [this](const FVector& Location)
    {
        float Distance = TNumericLimits<float>::Max();
        for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
        {
            if (It->IsUnitAlive() && It->GetTeam() != GetTeam())
            {
                Distance = FMath::Min(Distance, FVector::Dist(Location, It->GetActorLocation()));
            }
        }
        return Distance;
    };
    const float CurrentDistance = NearestOpponentDistance(GetCurrentTile()->GetActorLocation());
    for (ACombatGridTile* Tile : Manager->CalculateReachableMoveTiles(const_cast<AEnemyUnit*>(this)))
    {
        const float Gain = CurrentDistance - NearestOpponentDistance(Tile->GetActorLocation());
        // Only advance toward an opponent; lateral and retreating moves keep SubAP.
        // 상대에게 가까워지는 이동만 선택하고 횡이동이나 후퇴에는 SubAP를 쓰지 않습니다.
        if (Gain > 1.0f && Gain / 200.0f > Decision.Score)
        {
            Decision.TargetTile = Tile;
            Decision.Score = Gain / 200.0f;
        }
    }
    return Decision;
}
