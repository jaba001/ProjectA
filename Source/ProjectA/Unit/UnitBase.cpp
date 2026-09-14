#include "UnitBase.h"

#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "EngineUtils.h"
#include "Grid/Combat/CombatGridTile.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "DataAsset/SkillPoolDataAsset.h"
#include "Controller/UnitAIController.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

AUnitBase::AUnitBase()
{
    PrimaryActorTick.bCanEverTick = false;

    bIsActiveTurn = false;

    GetCharacterMovement()->MaxWalkSpeed = 700.f;

    GetCapsuleComponent()->SetCollisionProfileName(TEXT("CombatUnit"));

    AIControllerClass = AUnitAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::Disabled;

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
    DOREPLIFETIME(AUnitBase, CombatSpeed);
    DOREPLIFETIME(AUnitBase, RuntimeCharacterName);
    DOREPLIFETIME(AUnitBase, bIsActiveTurn);
    DOREPLIFETIME(AUnitBase, bTurnMustEndAfterCurrentAction);
    DOREPLIFETIME(AUnitBase, bIsDead);
    DOREPLIFETIME(AUnitBase, DeathImpulse);
    DOREPLIFETIME(AUnitBase, CurrentTile);
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
    // Only the coordinator can start round actions.
    // 라운드 행동은 조정자만 시작할 수 있습니다.
    OnTurnEnd();
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

bool AUnitBase::IsBusy() const
{
    if (!GetWorld()) return false;
    for (TActorIterator<ACombatRoundCoordinator> It(GetWorld()); It; ++It)
    {
        if (!It->IsRoundSessionActive()) continue;
        for (const FCombatRoundUnitView& Entry : It->GetView().Units)
        {
            if (Entry.Unit == this && IsUnitAlive()) return true;
        }
    }
    return false;
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

void AUnitBase::CancelCurrentAction()
{
    if (!HasAuthority())
    {
        return;
    }
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
    }
    GetCharacterMovement()->StopMovementImmediately();
    if (AbilitySystem)
    {
        AbilitySystem->CancelAllAbilities();
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
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::MoveToTarget(AUnitBase* TargetUnit)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::ReturnToOriginalTile()
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::SnapToTile(ACombatGridTile* Tile, const FRotator& TargetRotation)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::OnSnapToTileFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::OnReturnToOriginalTileFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::HandleMoveCompleted()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::HandleMoveFailed(EUnitActionResult Result)
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::StartSkill(USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::ExecuteSkillAtTarget()
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

TArray<AUnitBase*> AUnitBase::ResolveSkillTargetUnits()
{
    return {};
}

void AUnitBase::OnSkillFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::ClearSkillContext()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::StartMoveAction(ACombatGridTile* TargetTile)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::OnMoveActionFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::ClearMoveContext()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

bool AUnitBase::CanUseHealingItem(AUnitBase* TargetUnit) const
{
    return false;
}

void AUnitBase::StartItemAction(AUnitBase* TargetUnit)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::ExecuteItemAtTarget()
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::OnItemFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::ClearItemContext()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
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
        FCombatRoundSkill Definition;
        FText Error;
        if (!IsValid(Skill) || !Skill->ResolveRoundSkill(Definition, Error))
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
        if (Skill->AbilityClass && Skill->AbilityClass != DefaultAttackAbilityClass)
        {
            EquippedSkillAbilityClasses.AddUnique(Skill->AbilityClass);
        }
    }
    AbilitySystem->ClearAllAbilities();
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), MaxHP);
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), MaxHP);
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
    FCombatRoundSkill Definition;
    FText Error;
    if (!HasAuthority() || IsBusy() || !IsUnitAlive() || !IsValid(Skill) || !Skill->ResolveRoundSkill(Definition, Error) || EquippedSkillDataAssets.Num() >= 5)
    {
        return false;
    }
    for (const USkillDefinitionDataAsset* Existing : EquippedSkillDataAssets)
    {
        if (IsValid(Existing) && Existing->GetPrimaryAssetId() == Skill->GetPrimaryAssetId()) return false;
    }
    EquippedSkillDataAssets.Add(Skill);
    if (Skill->AbilityClass) EquippedSkillAbilityClasses.AddUnique(Skill->AbilityClass);
    ForceNetUpdate();
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
        FCombatRoundSkill Definition;
        FText Error;
        if (Entry.Weight > 0 && IsValid(Entry.Skill) && Entry.Skill->ResolveRoundSkill(Definition, Error))
        {
            const bool bOwned = EquippedSkillDataAssets.ContainsByPredicate([&Entry](const USkillDefinitionDataAsset* Existing) { return IsValid(Existing) && Existing->GetPrimaryAssetId() == Entry.Skill->GetPrimaryAssetId(); });
            if (bOwned) continue;
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
