#include "UnitBase.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

#include "Combat/CombatManager.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Controller/UnitAIController.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/LatentActionManager.h"

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
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    bReplicates = true;
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

    if (AttributeSet)
    {
        AttributeSet->InitMaxHP(InitMaxHP);
        AttributeSet->InitHP(InitMaxHP);
    }

    DefaultBattleRotation = GetActorRotation();
}

void AUnitBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bIsActiveTurn = false;
    CancelCurrentAction();

    if (IsValid(CurrentTile) && CurrentTile->GetOccupyingUnit() == this)
    {
        CurrentTile->SetOccupyingUnit(nullptr);
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
}

void AUnitBase::SetTeam(ETeam NewTeam)
{
    if (!HasAuthority())
        return;  

    Team = NewTeam;

    if (CurrentTile)
    {
        CurrentTile->UpdateTileVisual();
    }
}

void AUnitBase::OnTurnStart()
{
    bIsActiveTurn = true;
    ResetActionPoint();
    ResetSubActionPoint();
    bTurnMustEndAfterCurrentAction = false;
}

void AUnitBase::OnTurnEnd()
{
    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;
}

bool AUnitBase::HasEnoughActionPoint(int32 Cost) const
{
    return CurrentActionPoint >= Cost;
}

bool AUnitBase::ConsumeActionPoint(int32 Cost)
{
    if (!HasEnoughActionPoint(Cost))
    {
        return false;
    }

    CurrentActionPoint -= Cost;

    if (CurrentActionPoint <= 0)
    {
        bTurnMustEndAfterCurrentAction = true;
    }

    return true;
}

bool AUnitBase::HasEnoughSubActionPoint(int32 Cost) const
{
    return CurrentSubActionPoint >= Cost;
}

bool AUnitBase::ConsumeSubActionPoint(int32 Cost)
{
    if (!HasEnoughSubActionPoint(Cost))
    {
        return false;
    }

    CurrentSubActionPoint -= Cost;

    return true;
}

bool AUnitBase::IsUnitAlive() const
{
    return !bIsDead;
}

void AUnitBase::Die()
{
    if (bIsDead)
        return;

    bIsDead = true;
    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;

    CancelCurrentAction();
    ClearSkillContext();
    ClearMoveContext();
    ClearItemContext();
    CurrentActionType = EUnitActionType::None;
    MovePhase = EUnitMovePhase::None;

    //사망 시 타일 점유 해제
    if (CurrentTile)
    {
        CurrentTile->SetOccupyingUnit(nullptr);
        CurrentTile = nullptr;
    }

    GetCharacterMovement()->DisableMovement();

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetAllBodiesSimulatePhysics(true);

    FVector Impulse;

    if (!DeathImpulse.IsNearlyZero())
    {
        Impulse = DeathImpulse;
    }
    else
    {
        Impulse = FMath::VRand() * 2000.0f;
        Impulse.Z = FMath::Abs(Impulse.Z) + 500.0f;
    }
    GetMesh()->AddImpulse(Impulse, NAME_None, true);

    //UE_LOG(LogTemp, Log, TEXT("[Death] %s died"), *GetName());
    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        CombatManager->RefreshTileProtectedByFront();
    }

    OnUnitDied.Broadcast(this);
}

void AUnitBase::BeginCurrentAction(EUnitActionType ActionType)
{
    ++CurrentActionSerial;
    CurrentActionType = ActionType;
    ActionOriginTile = CurrentTile;
    ActionOriginTransform = GetActorTransform();
}

void AUnitBase::CompleteCurrentAction(EUnitActionResult Result)
{
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

    OnUnitActionCompleted(CompletedType, Result);
    OnActionCompleted.Broadcast(this, CompletedType, Result);
}

void AUnitBase::OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
}

void AUnitBase::CancelCurrentAction()
{
    CompleteCurrentAction(EUnitActionResult::Cancelled);
}

void AUnitBase::RestoreActionOrigin()
{
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

    CurrentTile->SetOccupyingUnit(this);

    ACombatManager* CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (CombatManager)
    {
        CombatManager->RefreshTileProtectedByFront();
    }
}


void AUnitBase::MoveToTile(ACombatGridTile* TargetTile)
{
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
    // A delayed snap callback must not finish a newer action after cancellation.
    // 취소 후 늦게 도착한 위치 보정 콜백이 새로운 행동을 완료하면 안 됩니다.
    if (ActionSerial == static_cast<int32>(CurrentActionSerial))
    {
        OnSnapToTileFinished();
    }
}

void AUnitBase::OnSnapToTileFinished()
{
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
    if (!IsBusy())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] HandleMoveFailed | Unit=%s | MovePhase=%d | ActionType=%d"), *GetNameSafe(this), static_cast<int32>(MovePhase), static_cast<int32>(CurrentActionType));

    CompleteCurrentAction(Result);
}

AUnitAIController* AUnitBase::GetOrCreateAIController()
{
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

    if (!SkillData || !TargetTile || !SkillData->AbilityClass || !AbilitySystem || !HasEnoughActionPoint(SkillData->ActionPointCost))
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
    if (CurrentActionType != EUnitActionType::Skill || MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        return;
    }

    if (!PendingSkillData || !AbilitySystem || !PendingSkillAbilityClass)
    {
        CompleteSkillExecution(EUnitActionResult::Failed);
        return;
    }

    const ESkillTargetRule TargetRule = PendingSkillData->TargetRule;
    if (TargetRule == ESkillTargetRule::EnemyUnit || TargetRule == ESkillTargetRule::AllyUnit || TargetRule == ESkillTargetRule::AnyUnit)
    {
        if (!IsValid(PendingTargetUnit) || !PendingTargetUnit->IsUnitAlive())
        {
            CompleteSkillExecution(EUnitActionResult::Failed);
            return;
        }
    }
    else if (!IsValid(PendingSkillTargetTile))
    {
        CompleteSkillExecution(EUnitActionResult::Failed);
        return;
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
    TArray<AUnitBase*> Result;

    if (!PendingSkillData)
    {
        return Result;
    }

    if (PendingSkillData->AreaType == ESkillAreaType::Single)
    {
        AUnitBase* TargetUnit = PendingTargetUnit;

        if (!TargetUnit && PendingSkillTargetTile)
        {
            TargetUnit = PendingSkillTargetTile->GetOccupyingUnit();
        }

        if (!TargetUnit)
        {
            return Result;
        }

        if (TargetUnit == this)
        {
            return Result;
        }

        if (!TargetUnit->IsUnitAlive())
        {
            return Result;
        }

        Result.Add(TargetUnit);
        return Result;
    }

    ACombatGridTile* CenterTile = nullptr;

    if (PendingSkillData->AreaType == ESkillAreaType::AroundTarget)
    {
        CenterTile = PendingSkillTargetTile;
    }
    else if (PendingSkillData->AreaType == ESkillAreaType::AroundSelf)
    {
        CenterTile = CurrentTile;
    }
    else
    {
        return Result;
    }

    if (!CenterTile)
    {
        return Result;
    }

    ACombatGridManager* CombatGridManager = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatGridManager::StaticClass()));

    if (!CombatGridManager)
    {
        return Result;
    }

    const TArray<ACombatGridTile*> AreaTiles = CombatGridManager->GetTilesInChebyshevRange(CenterTile, PendingSkillData->AreaRadius);

    return UCombatTargetingLibrary::CollectUniqueAliveUnitsFromTiles(AreaTiles, this);
}

void AUnitBase::OnSkillFinished()
{
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
    CompleteCurrentAction(EUnitActionResult::Succeeded);
}

void AUnitBase::ClearMoveContext()
{
    PendingTile = nullptr;
}

void AUnitBase::StartItemAction(AUnitBase* TargetUnit)
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

    if (!HasEnoughSubActionPoint(1))
    {
        return;
    }

    if (!ConsumeSubActionPoint(1))
    {
        return;
    }

    

    BeginCurrentAction(EUnitActionType::Item);
    PendingTargetUnit = TargetUnit;

    ExecuteItemAtTarget();
}

void AUnitBase::ExecuteItemAtTarget()
{
    OnItemFinished();
}

void AUnitBase::OnItemFinished()
{
    CompleteCurrentAction(EUnitActionResult::Succeeded);
}

void AUnitBase::ClearItemContext()
{
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
