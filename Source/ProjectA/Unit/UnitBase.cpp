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
    if (!TargetTile)
    {
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
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
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
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
        return;
    }

    AUnitAIController* AICon = GetOrCreateAIController();

    if (!AICon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ReturnToOriginalTile failed: AICon is null"));
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
        return;
    }

    FVector Center = Tile->GetActorLocation();
    Center.Z = GetActorLocation().Z;

    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.UUID = 1001;
    LatentInfo.Linkage = 0;
    LatentInfo.ExecutionFunction = FName(TEXT("OnSnapToTileFinished"));

    UKismetSystemLibrary::MoveComponentTo(
        GetCapsuleComponent(),
        Center,
        TargetRotation,
        false,
        false,
        0.5f,
        false,
        EMoveComponentAction::Move,
        LatentInfo
    );
}

void AUnitBase::OnSnapToTileFinished()
{
    if (MovePhase == EUnitMovePhase::ReturningToOriginalTile)
    {
        MovePhase = EUnitMovePhase::None;
        ClearSkillContext();
        OnReturnToOriginalTileFinished();
        return;
    }

    if (MovePhase == EUnitMovePhase::MovingToTile)
    {
        MovePhase = EUnitMovePhase::None;

        if (CurrentActionType == EUnitActionType::Move)
        {
            OnMoveActionFinished();
        }

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
    if (!SkillData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | SkillData is null"));
        return;
    }

    if (!TargetTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | TargetTile is null"));
        return;
    }

    if (!SkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | AbilityClass is null"));
        return;
    }

    PendingSkillData = SkillData;
    PendingSkillAbilityClass = SkillData->AbilityClass;
    PendingSkillTargetTile = TargetTile;
    PendingTargetUnit = TargetTile->GetOccupyingUnit();

    OriginalTileBeforeSkill = CurrentTile;

    CurrentActionType = EUnitActionType::Skill;

    if (SkillData->bMoveToTarget)
    {
        AUnitBase* TargetUnit = TargetTile->GetOccupyingUnit();

        if (!TargetUnit)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | MoveToTarget requires occupied target tile"));
            return;
        }

        MoveToTarget(TargetUnit);
        return;
    }

    if (!AbilitySystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitBase] StartSkill failed | AbilitySystem is null"));
        return;
    }

    const bool bActivated = AbilitySystem->TryActivateAbilityByClass(SkillData->AbilityClass);

    //UE_LOG(LogTemp, Log, TEXT("[UnitBase] StartSkill Activate | Unit=%s | Ability=%s | Activated=%d | TargetTile=(%d,%d) | TargetUnit=%s"), *GetNameSafe(this), *GetNameSafe(SkillData->AbilityClass), bActivated ? 1 : 0, TargetTile->GridCoord.X, TargetTile->GridCoord.Y, *GetNameSafe(PendingTargetUnit));

}

void AUnitBase::ExecuteSkillAtTarget()
{
    
    if (MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] ExecuteSkillAtTarget Return | Reason=InvalidMovePhase"));
        return;
    }

    if (!PendingSkillData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=PendingSkillDataNull"));
        OnSkillFinished();
        return;
    }

    if (PendingSkillData->TargetRule == ESkillTargetRule::EnemyUnit || PendingSkillData->TargetRule == ESkillTargetRule::AllyUnit || PendingSkillData->TargetRule == ESkillTargetRule::AnyUnit)
    {
        if (!PendingTargetUnit || !PendingTargetUnit->IsUnitAlive())
        {
            UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=InvalidTargetUnit"));
            OnSkillFinished();
            return;
        }
    }
    else if (PendingSkillData->TargetRule == ESkillTargetRule::EnemyTile || PendingSkillData->TargetRule == ESkillTargetRule::AllyTile || PendingSkillData->TargetRule == ESkillTargetRule::AnyTile)
    {
        if (!PendingSkillTargetTile)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=InvalidTargetTile"));
            OnSkillFinished();
            return;
        }
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

    if (!AbilitySystem || !PendingSkillData || !PendingSkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] ExecuteSkillAtTarget Return | Reason=NoASCOrAbilityClass | ASC=%d | SkillData=%s | AbilityClass=%s"), AbilitySystem ? 1 : 0, *GetNameSafe(PendingSkillData), PendingSkillData ? *GetNameSafe(PendingSkillData->AbilityClass) : TEXT("None"));
        OnSkillFinished();
        return;
    }

    const bool bActivated = AbilitySystem->TryActivateAbilityByClass(PendingSkillAbilityClass);
    
    if (!bActivated)
    {
        OnSkillFinished();
    }
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
    //UE_LOG(LogTemp, Log, TEXT("[Skill] OnSkillFinished: %s"), *GetName());

    if (bIsDead)
    {
        return;
    }

    if (MovePhase != EUnitMovePhase::WaitingForSkill)
    {
        return;
    }

    ReturnToOriginalTile();
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

    if (MovePhase != EUnitMovePhase::None)
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

    
    CurrentActionType = EUnitActionType::Move;

    MoveToTile(TargetTile);
}

void AUnitBase::OnMoveActionFinished()
{
    CurrentActionType = EUnitActionType::None;
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

    if (MovePhase != EUnitMovePhase::None)
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

    

    CurrentActionType = EUnitActionType::Item;
    PendingTargetUnit = TargetUnit;

    ExecuteItemAtTarget();
}

void AUnitBase::ExecuteItemAtTarget()
{
    OnItemFinished();
}

void AUnitBase::OnItemFinished()
{
    CurrentActionType = EUnitActionType::None;
    ClearItemContext();
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