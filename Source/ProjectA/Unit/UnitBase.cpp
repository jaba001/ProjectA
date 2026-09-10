#include "UnitBase.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

#include "Combat/CombatManager.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Unit/PlayerUnit.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "DataAsset/SkillPoolDataAsset.h"
#include "Controller/UnitAIController.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/LatentActionManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameplayEffect.h"

namespace
{
    bool IsCheckpointAssetPath(const FSoftObjectPath& Path)
    {
        const FString Value = Path.ToString();
        return Path.IsValid() && Path.GetSubPathString().IsEmpty() && Value.Len() <= 512 && (Value.StartsWith(TEXT("/Game/")) || Value.StartsWith(TEXT("/Script/ProjectA.")));
    }

    bool ValidateCheckpointGAS(const UAbilitySystemComponent* ASC, FText& OutError)
    {
        if (!ASC)
        {
            OutError = FText::FromString(TEXT("체크포인트 유닛에 어빌리티 시스템이 없습니다."));
            return false;
        }
        if (!ASC->GetActiveEffects(FGameplayEffectQuery()).IsEmpty() || !ASC->GetOwnedGameplayTags().IsEmpty())
        {
            OutError = FText::FromString(TEXT("지속 효과, 쿨다운 또는 상태 태그가 있는 전투의 턴 복구는 지원하지 않습니다."));
            return false;
        }
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (!Spec.Ability || Spec.IsActive())
            {
                OutError = FText::FromString(TEXT("어빌리티가 실행 중인 상태는 확정 턴 경계가 아닙니다."));
                return false;
            }
            if (Spec.Ability && Spec.Ability->GetCooldownGameplayEffect())
            {
                OutError = FText::FromString(TEXT("쿨다운이 설정된 어빌리티의 턴 복구는 지원하지 않습니다."));
                return false;
            }
            const UGameplayEffect* CostEffect = Spec.Ability->GetCostGameplayEffect();
            if (Spec.Level != 1 || !Spec.GetDynamicSpecSourceTags().IsEmpty() || (CostEffect && CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant))
            {
                OutError = FText::FromString(TEXT("변경된 어빌리티 레벨, 태그 또는 지속 비용 효과의 턴 복구는 지원하지 않습니다."));
                return false;
            }
        }
        return true;
    }
}

AUnitBase::AUnitBase()
{
    PrimaryActorTick.bCanEverTick = false;

    bIsActiveTurn = false;
    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    OriginalTileBeforeSkill = nullptr;
    bSkillDamageApplied = false;
    MovePhase = EUnitMovePhase::None;

    GetCharacterMovement()->MaxWalkSpeed = 700.f;

    GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));

    AIControllerClass = AUnitAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AttributeSet = CreateDefaultSubobject<UAS_Unit>(TEXT("AttributeSet"));

    AbilitySystem->SetIsReplicated(true);
    // AI-controlled units expose attributes and cues without a player-owned ASC.
    // AI가 제어하는 유닛은 플레이어 소유 ASC 없이 어트리뷰트와 큐를 전달합니다.
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
}

UAbilitySystemComponent* AUnitBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void AUnitBase::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystem)
    {
        AbilitySystem->InitAbilityActorInfo(this, this);

        if (HasAuthority())
        {
            const TArray<TSubclassOf<UGameplayAbility>> SkillClasses = GetAvailableSkillAbilityClasses();

            for (const TSubclassOf<UGameplayAbility>& SkillClass : SkillClasses)
            {
                if (!SkillClass)
                {
                    continue;
                }

                AbilitySystem->GiveAbility(FGameplayAbilitySpec(SkillClass, 1, 0));
            }
        }
    }

    if (HasAuthority() && AttributeSet)
    {
        AttributeSet->InitMaxHP(InitMaxHP);
        AttributeSet->InitHP(InitMaxHP);
    }

    DefaultBattleRotation = GetActorRotation();
    if (bIsDead)
    {
        ApplyDeathPresentation();
    }
}

void AUnitBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        bIsActiveTurn = false;
        CancelCurrentAction();
        if (IsValid(CurrentTile) && CurrentTile->GetOccupyingUnit() == this)
        {
            CurrentTile->SetOccupyingUnit(nullptr);
        }
    }

    CurrentTile = nullptr;
    OnActionCompleted.Clear();
    OnUnitDied.Clear();
    Super::EndPlay(EndPlayReason);
}

void AUnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AUnitBase, Team);
    DOREPLIFETIME(AUnitBase, UnitIndex);
    DOREPLIFETIME(AUnitBase, RuntimeCharacterName);
    DOREPLIFETIME(AUnitBase, CurrentActionType);
    DOREPLIFETIME(AUnitBase, bIsActiveTurn);
    DOREPLIFETIME(AUnitBase, bTurnMustEndAfterCurrentAction);
    DOREPLIFETIME(AUnitBase, bIsDead);
    DOREPLIFETIME(AUnitBase, DeathImpulse);
    DOREPLIFETIME(AUnitBase, CurrentTile);
    DOREPLIFETIME(AUnitBase, MovePhase);
    DOREPLIFETIME(AUnitBase, MoveRange);
    DOREPLIFETIME(AUnitBase, HealingItemAmount);
    DOREPLIFETIME(AUnitBase, HealingItemCount);
    DOREPLIFETIME(AUnitBase, DefaultAttackAbilityClass);
    DOREPLIFETIME(AUnitBase, EquippedSkillAbilityClasses);
    DOREPLIFETIME(AUnitBase, EquippedSkillDataAssets);
    DOREPLIFETIME(AUnitBase, MaxActionPoint);
    DOREPLIFETIME(AUnitBase, CurrentActionPoint);
    DOREPLIFETIME(AUnitBase, MaxSubActionPoint);
    DOREPLIFETIME(AUnitBase, CurrentSubActionPoint);
}

void AUnitBase::OnRep_Team()
{
    if (IsValid(CurrentTile))
    {
        CurrentTile->UpdateTileVisual();
    }
}

void AUnitBase::OnRep_CurrentTile(ACombatGridTile* PreviousTile)
{
    if (IsValid(PreviousTile))
    {
        PreviousTile->UpdateTileVisual();
    }
    if (IsValid(CurrentTile))
    {
        CurrentTile->UpdateTileVisual();
    }
}

void AUnitBase::SetTeam(ETeam NewTeam)
{
    if (!HasAuthority())
    {
        return;
    }

    Team = NewTeam;
    ForceNetUpdate();

    if (CurrentTile)
    {
        CurrentTile->UpdateTileVisual();
    }
}

void AUnitBase::OnTurnStart()
{
    if (!HasAuthority())
    {
        return;
    }

    bIsActiveTurn = true;
    ResetActionPoint();
    ResetSubActionPoint();
    bTurnMustEndAfterCurrentAction = false;
    ForceNetUpdate();
}

void AUnitBase::OnTurnEnd()
{
    if (!HasAuthority())
    {
        return;
    }

    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;
    ForceNetUpdate();
}

void AUnitBase::ResetActionPoint()
{
    if (HasAuthority())
    {
        CurrentActionPoint = MaxActionPoint;
        ForceNetUpdate();
    }
}

void AUnitBase::ResetSubActionPoint()
{
    if (HasAuthority())
    {
        CurrentSubActionPoint = MaxSubActionPoint;
        ForceNetUpdate();
    }
}

bool AUnitBase::HasEnoughActionPoint(int32 Cost) const
{
    return Cost > 0 && CurrentActionPoint >= Cost;
}

bool AUnitBase::ConsumeActionPoint(int32 Cost)
{
    if (!HasAuthority() || !HasEnoughActionPoint(Cost))
    {
        return false;
    }

    CurrentActionPoint -= Cost;
    ForceNetUpdate();

    bTurnMustEndAfterCurrentAction = CurrentActionPoint <= 0 && CurrentSubActionPoint <= 0;

    return true;
}

bool AUnitBase::HasEnoughSubActionPoint(int32 Cost) const
{
    return CurrentSubActionPoint >= Cost;
}

bool AUnitBase::ConsumeSubActionPoint(int32 Cost)
{
    if (!HasAuthority() || !HasEnoughSubActionPoint(Cost))
    {
        return false;
    }

    CurrentSubActionPoint -= Cost;
    ForceNetUpdate();
    bTurnMustEndAfterCurrentAction = CurrentActionPoint <= 0 && CurrentSubActionPoint <= 0;

    return true;
}

bool AUnitBase::IsUnitAlive() const
{
    return !bIsDead;
}

void AUnitBase::Die()
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    bIsDead = true;
    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;

    CancelCurrentAction();
    ClearSkillContext();
    ClearMoveContext();
    ClearItemContext();
    CurrentActionType = EUnitActionType::None;
    MovePhase = EUnitMovePhase::None;

    // Release occupancy only on the authoritative death path.
    // 서버 사망 처리에서만 타일 점유를 해제합니다.
    if (CurrentTile)
    {
        CurrentTile->SetOccupyingUnit(nullptr);
        CurrentTile = nullptr;
    }

    // Generate the cosmetic impulse once on the server for every viewer.
    // 모든 관찰자에게 전달할 시각 임펄스는 서버에서 한 번 생성합니다.
    if (DeathImpulse.IsNearlyZero())
    {
        DeathImpulse = FMath::VRand() * 2000.0f;
        DeathImpulse.Z = FMath::Abs(DeathImpulse.Z) + 500.0f;
    }
    ApplyDeathPresentation();
    ForceNetUpdate();

    //UE_LOG(LogTemp, Log, TEXT("[Death] %s died"), *GetName());
    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        CombatManager->RefreshTileProtectedByFront();
    }

    OnUnitDied.Broadcast(this);
}

void AUnitBase::OnRep_Death()
{
    if (bIsDead)
    {
        ApplyDeathPresentation();
    }
}

void AUnitBase::ApplyDeathPresentation()
{
    if (bDeathPresentationApplied)
    {
        return;
    }

    bDeathPresentationApplied = true;
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetAllBodiesSimulatePhysics(true);
    GetMesh()->AddImpulse(DeathImpulse, NAME_None, true);
}

void AUnitBase::BeginCurrentAction(EUnitActionType ActionType)
{
    if (!HasAuthority())
    {
        return;
    }

    ++CurrentActionSerial;
    CurrentActionType = ActionType;
    ActionOriginTile = CurrentTile;
    ActionOriginTransform = GetActorTransform();
    ForceNetUpdate();
}

void AUnitBase::CompleteCurrentAction(EUnitActionResult Result)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentActionType == EUnitActionType::None)
    {
        return;
    }

    const EUnitActionType CompletedType = CurrentActionType;
    const FGameplayAbilitySpecHandle AbilityToCancel = ActiveSkillHandle;

    // Clear ownership before cancellation can invoke synchronous callbacks.
    // 취소가 동기 콜백을 호출하기 전에 행동 소유 상태를 해제합니다.
    CurrentActionType = EUnitActionType::None;
    MovePhase = EUnitMovePhase::None;
    if (AbilitySystem && SkillAbilityEndedHandle.IsValid())
    {
        AbilitySystem->OnAbilityEnded.Remove(SkillAbilityEndedHandle);
    }

    SkillAbilityEndedHandle.Reset();
    ActiveSkillHandle = FGameplayAbilitySpecHandle();

    if (AUnitAIController* AIController = Cast<AUnitAIController>(GetController()))
    {
        AIController->StopMovement();
    }

    if (GetWorld())
    {
        GetWorld()->GetLatentActionManager().RemoveActionsForObject(this);
    }

    if (Result != EUnitActionResult::Succeeded && IsUnitAlive())
    {
        RestoreActionOrigin();
    }

    ClearSkillContext();
    ClearMoveContext();
    ClearItemContext();
    ActionOriginTile = nullptr;
    bSkillRequiresReturn = false;

    if (AbilitySystem && AbilityToCancel.IsValid() && Result != EUnitActionResult::Succeeded)
    {
        AbilitySystem->CancelAbilityHandle(AbilityToCancel);
    }

    ForceNetUpdate();
    OnUnitActionCompleted(CompletedType, Result);
    OnActionCompleted.Broadcast(this, CompletedType, Result);
}

void AUnitBase::OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
}

void AUnitBase::CancelCurrentAction()
{
    if (!HasAuthority())
    {
        return;
    }

    CompleteCurrentAction(EUnitActionResult::Cancelled);
}

void AUnitBase::RestoreActionOrigin()
{
    if (!HasAuthority())
    {
        return;
    }

    // The original tile stays reserved during a skill approach and return.
    // 스킬 접근과 복귀 중에는 원래 타일의 점유를 유지합니다.
    if (IsValid(ActionOriginTile))
    {
        AUnitBase* Occupant = ActionOriginTile->GetOccupyingUnit();
        if (Occupant && Occupant != this)
        {
            UE_LOG(LogTemp, Error, TEXT("[UnitBase] Action origin reservation lost | Unit=%s | Tile=%s"), *GetNameSafe(this), *GetNameSafe(ActionOriginTile));
            return;
        }

        SetCurrentTile(ActionOriginTile);
        ActionOriginTile->SetOccupyingUnit(this);
    }

    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocationAndRotation(ActionOriginTransform.GetLocation(), ActionOriginTransform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
}

void AUnitBase::SetCurrentTile(ACombatGridTile* NewTile)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!NewTile || CurrentTile == NewTile)
        return;

    if (CurrentTile)
    {
        CurrentTile->SetOccupyingUnit(nullptr);
    }

    CurrentTile = NewTile;
    ForceNetUpdate();

    CurrentTile->SetOccupyingUnit(this);

    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        CombatManager->RefreshTileProtectedByFront();
    }
}


void AUnitBase::MoveToTile(ACombatGridTile* TargetTile)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentActionType == EUnitActionType::None)
    {
        BeginCurrentAction(EUnitActionType::Move);
    }

    if (!TargetTile)
    {
        HandleMoveFailed();
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        HandleMoveFailed();
        return;
    }

    PendingTile = TargetTile;
    PendingTargetUnit = nullptr;
    MovePhase = EUnitMovePhase::MovingToTile;

    FVector TargetLocation = TargetTile->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    AICon->MoveUnitToLocation(TargetLocation, 1.f);
}

void AUnitBase::MoveToTarget(AUnitBase* TargetUnit)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!TargetUnit || !TargetUnit->IsUnitAlive())
    {
        HandleMoveFailed();
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        HandleMoveFailed();
        return;
    }

    PendingTargetUnit = TargetUnit;
    PendingTile = nullptr;
    MovePhase = EUnitMovePhase::MovingToTarget;

    FVector TargetLocation = TargetUnit->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    AICon->MoveUnitToLocation(TargetLocation, 75.f);
}

void AUnitBase::ReturnToOriginalTile()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!OriginalTileBeforeSkill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ReturnToOriginalTile failed: OriginalTileBeforeSkill is null"));
        HandleMoveFailed();
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ReturnToOriginalTile failed: AICon is null"));
        HandleMoveFailed();
        return;
    }

    PendingTile = OriginalTileBeforeSkill;
    PendingTargetUnit = nullptr;
    MovePhase = EUnitMovePhase::ReturningToOriginalTile;

    FVector TargetLocation = OriginalTileBeforeSkill->GetActorLocation();
    TargetLocation.Z = GetActorLocation().Z;

    //UE_LOG(LogTemp, Log, TEXT("[Skill] Returning to original tile"));

    AICon->MoveUnitToLocation(TargetLocation, 2.f);
}

void AUnitBase::SnapToTile(ACombatGridTile* Tile, const FRotator& TargetRotation)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!Tile)
    {
        HandleMoveFailed();
        return;
    }

    FVector Center = Tile->GetActorLocation();
    Center.Z = GetActorLocation().Z;

    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.UUID = static_cast<int32>(CurrentActionSerial);
    LatentInfo.Linkage = static_cast<int32>(CurrentActionSerial);
    LatentInfo.ExecutionFunction = FName(TEXT("HandleActionSnapFinished"));

    UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), Center, TargetRotation, false, false, 0.5f, false, EMoveComponentAction::Move, LatentInfo);
}

void AUnitBase::HandleActionSnapFinished(int32 ActionSerial)
{
    if (!HasAuthority())
    {
        return;
    }

    // A delayed snap callback must not finish a newer action after cancellation.
    // 취소 후 늦게 도착한 위치 보정 콜백이 새로운 행동을 완료하면 안 됩니다.
    if (ActionSerial == static_cast<int32>(CurrentActionSerial))
    {
        OnSnapToTileFinished();
    }
}

void AUnitBase::OnSnapToTileFinished()
{
    if (!HasAuthority())
    {
        return;
    }

    if (MovePhase == EUnitMovePhase::ReturningToOriginalTile)
    {
        OnReturnToOriginalTileFinished();
        CompleteCurrentAction(EUnitActionResult::Succeeded);
        return;
    }

    if (MovePhase == EUnitMovePhase::MovingToTile)
    {
        OnMoveActionFinished();
        return;
    }
}

void AUnitBase::OnReturnToOriginalTileFinished()
{
}

void AUnitBase::HandleMoveCompleted()
{
    if (!HasAuthority())
    {
        return;
    }

    switch (MovePhase)
    {
    case EUnitMovePhase::MovingToTile:
    {
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            SnapToTile(PendingTile, DefaultBattleRotation);
            PendingTile = nullptr;
        }

        break;
    }
    case EUnitMovePhase::MovingToTarget:
    {
        MovePhase = EUnitMovePhase::WaitingForSkill;
        ExecuteSkillAtTarget();
        break;
    }
    case EUnitMovePhase::ReturningToOriginalTile:
    {
        if (PendingTile)
        {
            SetCurrentTile(PendingTile);
            SnapToTile(PendingTile, DefaultBattleRotation);
            PendingTile = nullptr;
        }

        break;
    }
    default:
    {
        break;
    }
    }

    //UE_LOG(LogTemp, Warning, TEXT("[UnitBase] HandleMoveCompleted | Unit=%s | MovePhase=%d | PendingSkillTargetTile=%s | PendingTargetUnit=%s"), *GetName(), static_cast<int32>(MovePhase), *GetNameSafe(PendingSkillTargetTile), *GetNameSafe(PendingTargetUnit));
}

void AUnitBase::HandleMoveFailed(EUnitActionResult Result)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!IsBusy())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] HandleMoveFailed | Unit=%s | MovePhase=%d | ActionType=%d"), *GetNameSafe(this), static_cast<int32>(MovePhase), static_cast<int32>(CurrentActionType));

    CompleteCurrentAction(Result);
}

AUnitAIController* AUnitBase::GetOrCreateAIController()
{
    if (!HasAuthority())
    {
        return nullptr;
    }

    AUnitAIController* AICon = Cast<AUnitAIController>(GetController());

    if (!AICon)
    {
        SpawnDefaultController();
        AICon = Cast<AUnitAIController>(GetController());
    }

    return AICon;
}

void AUnitBase::StartSkill(USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile)
{
    if (!HasAuthority() || !bIsActiveTurn || IsBusy() || !IsUnitAlive())
    {
        return;
    }

    BeginCurrentAction(EUnitActionType::Skill);

    if (!UCombatTargetingLibrary::IsValidSkillTarget(this, SkillData, TargetTile) || !SkillData->AbilityClass || !AbilitySystem || !HasEnoughActionPoint(SkillData->ActionPointCost))
    {
        CompleteCurrentAction(EUnitActionResult::Failed);
        return;
    }

    PendingSkillData = SkillData;
    PendingSkillAbilityClass = SkillData->AbilityClass;
    PendingSkillTargetTile = TargetTile;
    PendingTargetUnit = TargetTile->GetOccupyingUnit();
    OriginalTileBeforeSkill = CurrentTile;
    bSkillRequiresReturn = SkillData->bMoveToTarget;

    if (bSkillRequiresReturn)
    {
        if (!IsValid(OriginalTileBeforeSkill) || !IsValid(PendingTargetUnit) || !PendingTargetUnit->IsUnitAlive())
        {
            CompleteCurrentAction(EUnitActionResult::Failed);
            return;
        }

        MoveToTarget(PendingTargetUnit);
        return;
    }

    // In-place skills own a complete action without requiring a return move.
    // 제자리 스킬은 복귀 이동 없이도 독립된 행동 수명을 가집니다.
    MovePhase = EUnitMovePhase::WaitingForSkill;
    ExecuteSkillAtTarget();
}

void AUnitBase::ExecuteSkillAtTarget()
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentActionType != EUnitActionType::Skill || MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        return;
    }

    if (!UCombatTargetingLibrary::IsValidSkillTarget(this, PendingSkillData, PendingSkillTargetTile) || !AbilitySystem || !PendingSkillAbilityClass)
    {
        CompleteSkillExecution(EUnitActionResult::Failed);
        return;
    }

    const ESkillTargetRule TargetRule = PendingSkillData->TargetRule;
    if (PendingSkillData->bMoveToTarget || TargetRule == ESkillTargetRule::EnemyUnit || TargetRule == ESkillTargetRule::AllyUnit || TargetRule == ESkillTargetRule::AnyUnit)
    {
        if (!IsValid(PendingTargetUnit) || PendingTargetUnit != PendingSkillTargetTile->GetOccupyingUnit())
        {
            CompleteSkillExecution(EUnitActionResult::Failed);
            return;
        }
    }
    else
    {
        // Tile skills follow current occupancy; unit skills keep their selected unit.
        // 타일 스킬은 현재 점유 상태를 따르고 유닛 스킬은 선택한 유닛을 유지합니다.
        PendingTargetUnit = PendingSkillTargetTile->GetOccupyingUnit();
    }

    FVector LookTargetLocation = GetActorLocation();
    if (PendingTargetUnit)
    {
        LookTargetLocation = PendingTargetUnit->GetActorLocation();
    }
    else if (PendingSkillTargetTile)
    {
        LookTargetLocation = PendingSkillTargetTile->GetActorLocation();
    }

    FVector Direction = LookTargetLocation - GetActorLocation();
    Direction.Z = 0.0f;
    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    FGameplayAbilitySpec* AbilitySpec = AbilitySystem->FindAbilitySpecFromClass(PendingSkillAbilityClass);
    if (!AbilitySpec || AbilitySpec->IsActive())
    {
        CompleteSkillExecution(EUnitActionResult::Failed);
        return;
    }

    // Bind before activation: GAS may end a no-montage ability synchronously.
    // 몽타주가 없는 GAS 어빌리티는 동기로 끝날 수 있으므로 활성화 전에 연결합니다.
    ActiveSkillHandle = AbilitySpec->Handle;
    SkillAbilityEndedHandle = AbilitySystem->OnAbilityEnded.AddUObject(this, &AUnitBase::HandleSkillAbilityEnded);
    const uint32 ActivatingActionSerial = CurrentActionSerial;
    const bool bActivated = AbilitySystem->TryActivateAbility(ActiveSkillHandle);
    if (!bActivated && CurrentActionSerial == ActivatingActionSerial && CurrentActionType == EUnitActionType::Skill)
    {
        CompleteSkillExecution(EUnitActionResult::Failed);
    }
}

void AUnitBase::HandleSkillAbilityEnded(const FAbilityEndedData& EndedData)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentActionType != EUnitActionType::Skill || EndedData.AbilitySpecHandle != ActiveSkillHandle)
    {
        return;
    }

    EUnitActionResult Result = EUnitActionResult::Succeeded;
    if (EndedData.bWasCancelled)
    {
        Result = EUnitActionResult::Cancelled;
    }

    if (const UGA_AttackBase* AttackAbility = Cast<UGA_AttackBase>(EndedData.AbilityThatEnded))
    {
        Result = AttackAbility->GetActionResult();
    }

    AbilitySystem->OnAbilityEnded.Remove(SkillAbilityEndedHandle);
    SkillAbilityEndedHandle.Reset();
    ActiveSkillHandle = FGameplayAbilitySpecHandle();
    CompleteSkillExecution(Result);
}

void AUnitBase::CompleteSkillExecution(EUnitActionResult Result)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentActionType != EUnitActionType::Skill || MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        return;
    }

    if (Result == EUnitActionResult::Succeeded && bSkillRequiresReturn)
    {
        ReturnToOriginalTile();
        return;
    }

    SetActorRotation(DefaultBattleRotation);
    CompleteCurrentAction(Result);
}

TArray<AUnitBase*> AUnitBase::ResolveSkillTargetUnits()
{
    return UCombatTargetingLibrary::ResolveSkillAreaTargets(this, PendingSkillData, PendingSkillTargetTile);
}

void AUnitBase::OnSkillFinished()
{
    if (!HasAuthority())
    {
        return;
    }

    // GAS owns completion while its bound ability is still active.
    // 연결된 어빌리티가 활성 상태인 동안에는 GAS가 완료를 소유합니다.
    if (ActiveSkillHandle.IsValid())
    {
        return;
    }

    CompleteSkillExecution(EUnitActionResult::Succeeded);
}

void AUnitBase::ClearSkillContext()
{
    if (!HasAuthority())
    {
        return;
    }

    PendingTile = nullptr;
    PendingTargetUnit = nullptr;
    PendingSkillTargetTile = nullptr;
    PendingSkillData = nullptr;
    PendingSkillAbilityClass = nullptr;
    OriginalTileBeforeSkill = nullptr;
    bSkillDamageApplied = false;
    CurrentActionType = EUnitActionType::None;
}

void AUnitBase::StartMoveAction(ACombatGridTile* TargetTile)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!bIsActiveTurn)
    {
        return;
    }

    if (IsBusy() || !IsUnitAlive())
    {
        return;
    }

    if (!TargetTile)
    {
        return;
    }
    
    if (TargetTile == CurrentTile)
    {
        return;
    }

    if (!HasEnoughSubActionPoint(1))
    {
        return;
    }

    if (!ConsumeSubActionPoint(1))
    {
        return;
    }

    
    BeginCurrentAction(EUnitActionType::Move);

    MoveToTile(TargetTile);
}

void AUnitBase::OnMoveActionFinished()
{
    if (!HasAuthority())
    {
        return;
    }

    CompleteCurrentAction(EUnitActionResult::Succeeded);
}

void AUnitBase::ClearMoveContext()
{
    if (!HasAuthority())
    {
        return;
    }

    PendingTile = nullptr;
}

bool AUnitBase::CanUseHealingItem(AUnitBase* TargetUnit) const
{
    return IsActiveTurn() && !IsBusy() && IsUnitAlive() && HasEnoughSubActionPoint(1) && HealingItemCount > 0 && FMath::IsFinite(HealingItemAmount) && HealingItemAmount > 0.0f && IsValid(TargetUnit) && TargetUnit->GetWorld() == GetWorld() && TargetUnit->GetTeam() == GetTeam() && TargetUnit->IsUnitAlive() && TargetUnit->AbilitySystem && TargetUnit->AttributeSet && TargetUnit->AttributeSet->GetHP() < TargetUnit->AttributeSet->GetMaxHP();
}

void AUnitBase::StartItemAction(AUnitBase* TargetUnit)
{
    if (!HasAuthority() || !CanUseHealingItem(TargetUnit) || !ConsumeSubActionPoint(1))
    {
        return;
    }
    BeginCurrentAction(EUnitActionType::Item);
    PendingTargetUnit = TargetUnit;
    ExecuteItemAtTarget();
}

void AUnitBase::ExecuteItemAtTarget()
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentActionType != EUnitActionType::Item || !IsValid(PendingTargetUnit) || HealingItemCount <= 0)
    {
        return;
    }
    // Consume before notifying attribute listeners to prevent reentrant item use.
    // 속성 리스너 호출 전에 수량을 차감해 재진입 사용을 방지합니다.
    --HealingItemCount;
    AUnitBase* Target = PendingTargetUnit;
    PendingTargetUnit = nullptr;
    const float HP = Target->AttributeSet->GetHP();
    const float MaxHP = Target->AttributeSet->GetMaxHP();
    Target->AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), FMath::Min(MaxHP, HP + HealingItemAmount));
    OnItemFinished();
}

void AUnitBase::OnItemFinished()
{
    if (!HasAuthority())
    {
        return;
    }

    CompleteCurrentAction(EUnitActionResult::Succeeded);
}

void AUnitBase::ClearItemContext()
{
    if (!HasAuthority())
    {
        return;
    }

    PendingTargetUnit = nullptr;
}

TArray<TSubclassOf<UGameplayAbility>> AUnitBase::GetAvailableSkillAbilityClasses() const
{
    TArray<TSubclassOf<UGameplayAbility>> Result;

    if (DefaultAttackAbilityClass)
    {
        Result.Add(DefaultAttackAbilityClass);
    }

    for (const TSubclassOf<UGameplayAbility>& SkillClass : EquippedSkillAbilityClasses)
    {
        if (!SkillClass)
        {
            continue;
        }

        Result.Add(SkillClass);
    }

    return Result;
}

USkillDefinitionDataAsset* AUnitBase::FindSkillDataByAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const
{
    if (!AbilityClass)
    {
        return nullptr;
    }

    for (USkillDefinitionDataAsset* SkillData : EquippedSkillDataAssets)
    {
        if (!SkillData)
        {
            continue;
        }

        if (SkillData->AbilityClass == AbilityClass)
        {
            return SkillData;
        }
    }

    return nullptr;
}

bool AUnitBase::ConfigureProfession(float MaxHP, int32 AP, int32 SubAP, const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills)
{
    if (!HasAuthority() || IsBusy() || IsActiveTurn() || !AbilitySystem || !AttributeSet || !FMath::IsFinite(MaxHP) || MaxHP <= 0.0f || AP <= 0 || SubAP < 0 || Skills.IsEmpty())
    {
        return false;
    }
    for (USkillDefinitionDataAsset* Skill : Skills)
    {
        if (!UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->AbilityClass || Skill->ActionPointCost <= 0)
        {
            return false;
        }
    }
    InitMaxHP = MaxHP;
    MaxActionPoint = AP;
    MaxSubActionPoint = SubAP;
    ResetActionPoint();
    ResetSubActionPoint();
    EquippedSkillDataAssets = Skills;
    EquippedSkillAbilityClasses.Reset();
    DefaultAttackAbilityClass = Skills.IsEmpty() ? nullptr : Skills[0]->AbilityClass;
    for (USkillDefinitionDataAsset* Skill : Skills)
    {
        if (Skill->AbilityClass != DefaultAttackAbilityClass)
        {
            EquippedSkillAbilityClasses.AddUnique(Skill->AbilityClass);
        }
    }
    AbilitySystem->ClearAllAbilities();
    for (TSubclassOf<UGameplayAbility> Ability : GetAvailableSkillAbilityClasses())
    {
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(Ability, 1, 0));
    }
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), MaxHP);
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), MaxHP);
    return true;
}

bool AUnitBase::CaptureCheckpointState(FCombatCheckpointUnit& OutState, FText& OutError) const
{
    if (!HasAuthority() || IsBusy() || IsActiveTurn() || !AttributeSet || ActiveSkillHandle.IsValid() || SkillAbilityEndedHandle.IsValid())
    {
        OutError = FText::FromString(TEXT("서버의 비활성 유휴 유닛만 턴 체크포인트로 저장할 수 있습니다."));
        return false;
    }
    if (const AAIController* AI = Cast<AAIController>(GetController()); AI && AI->GetPathFollowingComponent() && AI->GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
    {
        OutError = FText::FromString(TEXT("이동 요청이 끝나지 않은 유닛은 턴 체크포인트로 저장할 수 없습니다."));
        return false;
    }
    if (!ValidateCheckpointGAS(AbilitySystem, OutError))
    {
        return false;
    }
    FCombatCheckpointUnit Captured = OutState;
    Captured.UnitClass = FSoftObjectPath(GetClass());
    Captured.DefaultAttackAbility = FSoftObjectPath(DefaultAttackAbilityClass.Get());
    Captured.Skills.Reset();
    if (!IsCheckpointAssetPath(Captured.UnitClass) || !IsCheckpointAssetPath(Captured.DefaultAttackAbility) || EquippedSkillDataAssets.IsEmpty())
    {
        OutError = FText::FromString(TEXT("저장 가능한 유닛 클래스와 기본 공격 및 장착 스킬 에셋이 필요합니다."));
        return false;
    }
    TSet<UClass*> CapturedAbilities;
    for (USkillDefinitionDataAsset* Skill : EquippedSkillDataAssets)
    {
        const FSoftObjectPath SkillPath(Skill);
        if (!IsValid(Skill) || !IsCheckpointAssetPath(SkillPath) || !Skill->AbilityClass || CapturedAbilities.Contains(Skill->AbilityClass))
        {
            OutError = FText::FromString(TEXT("임시 또는 해석할 수 없는 장착 스킬은 체크포인트에 저장할 수 없습니다."));
            return false;
        }
        Captured.Skills.Add(SkillPath);
        CapturedAbilities.Add(Skill->AbilityClass);
    }
    if (CapturedAbilities.Num() != AbilitySystem->GetActivatableAbilities().Num())
    {
        OutError = FText::FromString(TEXT("장착 목록으로 표현할 수 없는 추가 어빌리티는 체크포인트에 저장할 수 없습니다."));
        return false;
    }
    for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
    {
        if (!CapturedAbilities.Contains(Spec.Ability->GetClass()))
        {
            OutError = FText::FromString(TEXT("실제 부여된 어빌리티와 체크포인트 장착 목록이 일치하지 않습니다."));
            return false;
        }
    }
    Captured.Team = Team;
    const APlayerUnit* Player = Cast<APlayerUnit>(this);
    Captured.PartyControlMode = Player ? Player->GetPartyControlMode() : EPartyControlMode::Human;
    Captured.CharacterName = RuntimeCharacterName.IsEmpty() ? FText::FromString(GetName()) : RuntimeCharacterName;
    Captured.HP = AttributeSet->GetHP();
    Captured.MaxHP = AttributeSet->GetMaxHP();
    Captured.AP = CurrentActionPoint;
    Captured.MaxAP = MaxActionPoint;
    Captured.SubAP = CurrentSubActionPoint;
    Captured.MaxSubAP = MaxSubActionPoint;
    Captured.MoveRange = MoveRange;
    Captured.HealingItemCount = HealingItemCount;
    Captured.HealingItemAmount = HealingItemAmount;
    Captured.bDead = bIsDead;
    Captured.bHasTile = IsValid(CurrentTile);
    Captured.GridCoord = CurrentTile ? CurrentTile->GridCoord : FIntPoint::ZeroValue;
    Captured.Transform = GetActorTransform();
    OutState = MoveTemp(Captured);
    OutError = FText::GetEmpty();
    return true;
}

bool AUnitBase::RestoreCheckpointState(const FCombatCheckpointUnit& State, FText& OutError)
{
    if (!HasAuthority() || bCheckpointStateRestored || CurrentActionSerial != 0 || IsActiveTurn() || IsBusy() || bIsDead || CurrentTile || !AttributeSet || State.UnitClass != FSoftObjectPath(GetClass()))
    {
        OutError = FText::FromString(TEXT("체크포인트와 같은 클래스의 새 비활성 유닛에만 상태를 복원할 수 있습니다."));
        return false;
    }
    APlayerUnit* Player = Cast<APlayerUnit>(this);
    if ((State.PartyControlMode != EPartyControlMode::Human && State.PartyControlMode != EPartyControlMode::ServerAI) || (State.PartyControlMode == EPartyControlMode::ServerAI && (!Player || State.Team != ETeam::Player)))
    {
        OutError = FText::FromString(TEXT("저장된 파티 조작 모드가 유닛 유형과 일치하지 않습니다."));
        return false;
    }
    if (!ValidateCheckpointGAS(AbilitySystem, OutError))
    {
        return false;
    }
    if (!FMath::IsFinite(State.HP) || !FMath::IsFinite(State.MaxHP) || State.MaxHP <= 0.0f || State.MaxHP > 1000000.0f || State.HP < 0.0f || State.HP > State.MaxHP || State.bDead != (State.HP == 0.0f) || State.MaxAP < 1 || State.MaxAP > 100 || State.AP < 0 || State.AP > State.MaxAP || State.MaxSubAP < 0 || State.MaxSubAP > 100 || State.SubAP < 0 || State.SubAP > State.MaxSubAP || State.MoveRange < 0 || State.MoveRange > 32 || State.HealingItemCount < 0 || State.HealingItemCount > 1000 || !FMath::IsFinite(State.HealingItemAmount) || State.HealingItemAmount < 0.0f || State.HealingItemAmount > 1000000.0f || State.Transform.ContainsNaN() || State.Skills.IsEmpty() || State.Skills.Num() > 5 || (State.Team != ETeam::Player && State.Team != ETeam::Enemy))
    {
        OutError = FText::FromString(TEXT("체크포인트의 HP, 행동력, 이동 또는 회복약 상태가 유효하지 않습니다."));
        return false;
    }
    UClass* DefaultAbility = IsCheckpointAssetPath(State.DefaultAttackAbility) ? Cast<UClass>(State.DefaultAttackAbility.TryLoad()) : nullptr;
    if (!DefaultAbility || !DefaultAbility->IsChildOf(UGameplayAbility::StaticClass()))
    {
        OutError = FText::FromString(TEXT("체크포인트 기본 공격 어빌리티를 해석할 수 없습니다."));
        return false;
    }
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
    TSet<TSubclassOf<UGameplayAbility>> AbilityClasses;
    for (const FSoftObjectPath& Path : State.Skills)
    {
        USkillDefinitionDataAsset* Skill = IsCheckpointAssetPath(Path) ? Cast<USkillDefinitionDataAsset>(Path.TryLoad()) : nullptr;
        if (!UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->AbilityClass || Skill->ActionPointCost <= 0 || AbilityClasses.Contains(Skill->AbilityClass) || Skill->AbilityClass->GetDefaultObject<UGameplayAbility>()->GetCooldownGameplayEffect())
        {
            OutError = FText::FromString(TEXT("체크포인트 스킬이 없거나 중복되거나 지원하지 않는 쿨다운을 사용합니다."));
            return false;
        }
        Skills.Add(Skill);
        AbilityClasses.Add(Skill->AbilityClass);
        const UGameplayEffect* CostEffect = Skill->AbilityClass->GetDefaultObject<UGameplayAbility>()->GetCostGameplayEffect();
        if (CostEffect && CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
        {
            OutError = FText::FromString(TEXT("지속 비용 효과를 사용하는 체크포인트 스킬은 복원할 수 없습니다."));
            return false;
        }
    }
    if (!AbilityClasses.Contains(DefaultAbility) || !ConfigureProfession(State.MaxHP, State.MaxAP, State.MaxSubAP, Skills))
    {
        OutError = FText::FromString(TEXT("체크포인트 장착 스킬과 기본 공격을 복원할 수 없습니다."));
        return false;
    }
    // Restore values without replaying damage, death events, item use, or ability callbacks.
    // 피해, 사망 이벤트, 아이템 사용 또는 어빌리티 콜백을 재실행하지 않고 값을 복원합니다.
    DefaultAttackAbilityClass = DefaultAbility;
    EquippedSkillAbilityClasses.Reset();
    for (USkillDefinitionDataAsset* Skill : Skills)
    {
        if (Skill->AbilityClass != DefaultAttackAbilityClass)
        {
            EquippedSkillAbilityClasses.Add(Skill->AbilityClass);
        }
    }
    RuntimeCharacterName = State.CharacterName;
    Team = State.Team;
    MoveRange = State.MoveRange;
    CurrentActionPoint = State.AP;
    CurrentSubActionPoint = State.SubAP;
    HealingItemCount = State.HealingItemCount;
    HealingItemAmount = State.HealingItemAmount;
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), State.HP);
    bIsDead = State.bDead;
    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;
    MovePhase = EUnitMovePhase::None;
    CurrentActionType = EUnitActionType::None;
    // Fresh actors receive a fresh AI session; restoring a mode does not start an AI turn.
    // 새 액터에 새 AI 세션을 부여하며 조작 모드 복원만으로 AI 턴을 시작하지 않습니다.
    if (Player && !Player->ApplyPartyControlMode(State.PartyControlMode))
    {
        OutError = FText::FromString(TEXT("복구할 유닛의 파티 조작 모드를 적용하지 못했습니다."));
        return false;
    }
    SetActorTransform(State.Transform, false, nullptr, ETeleportType::TeleportPhysics);
    DefaultBattleRotation = State.Transform.Rotator();
    if (bIsDead)
    {
        ApplyDeathPresentation();
    }
    bCheckpointStateRestored = true;
    ForceNetUpdate();
    OutError = FText::GetEmpty();
    return true;
}

bool AUnitBase::ConfigureMoveRange(int32 InMoveRange)
{
    if (!HasAuthority() || IsBusy() || IsActiveTurn() || InMoveRange < 0 || InMoveRange > 32)
    {
        return false;
    }
    MoveRange = InMoveRange;
    return true;
}

bool AUnitBase::AcquireAndEquipSkill(USkillDefinitionDataAsset* Skill)
{
    if (!HasAuthority() || IsBusy() || !IsUnitAlive() || !AbilitySystem || !UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->AbilityClass || Skill->ActionPointCost <= 0 || GetAvailableSkillAbilityClasses().Contains(Skill->AbilityClass) || EquippedSkillAbilityClasses.Num() >= 4)
    {
        return false;
    }
    AbilitySystem->GiveAbility(FGameplayAbilitySpec(Skill->AbilityClass, 1, 0));
    EquippedSkillDataAssets.Add(Skill);
    EquippedSkillAbilityClasses.Add(Skill->AbilityClass);
    return true;
}

USkillDefinitionDataAsset* AUnitBase::AcquireSkillFromPool(USkillPoolDataAsset* Pool)
{
    if (!IsValid(Pool) || !HasAuthority() || IsBusy() || !IsUnitAlive())
    {
        return nullptr;
    }
    // Filter owned and invalid entries before rolling so duplicates do not waste rewards.
    // 획득 전 보유 및 잘못된 항목을 제외해 중복으로 보상을 잃지 않도록 합니다.
    double TotalWeight = 0.0;
    TArray<const FSkillPoolEntry*> Candidates;
    for (const FSkillPoolEntry& Entry : Pool->Entries)
    {
        if (Entry.Weight > 0 && UCombatTargetingLibrary::IsSupportedSkillArea(Entry.Skill) && Entry.Skill->AbilityClass && Entry.Skill->ActionPointCost > 0 && !GetAvailableSkillAbilityClasses().Contains(Entry.Skill->AbilityClass))
        {
            Candidates.Add(&Entry);
            TotalWeight += Entry.Weight;
        }
    }
    double Roll = FMath::FRand() * TotalWeight;
    for (const FSkillPoolEntry* Entry : Candidates)
    {
        Roll -= Entry->Weight;
        if (Roll <= 0.0)
        {
            return AcquireAndEquipSkill(Entry->Skill) ? Entry->Skill.Get() : nullptr;
        }
    }
    return nullptr;
}
