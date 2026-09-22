#include "UnitBase.h"
#include "Unit/UnitCharacterMovementComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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

AUnitBase::AUnitBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UUnitCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
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
    WeaponPresentationSkills.Add(TSoftObjectPtr<USkillDefinitionDataAsset>(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack"))));
}

UAbilitySystemComponent* AUnitBase::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

float AUnitBase::GetCombatSpeed() const
{
    const float Dexterity = AttributeSet ? AttributeSet->GetDexterity() : 0.0f;
    return FMath::IsFinite(Dexterity) ? FMath::Max(0.0f, Dexterity) : 0.0f;
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
    RefreshSkillPresentation();
}

void AUnitBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopRoundCastMontage();
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

void AUnitBase::SetRoundMovementVelocity(const FVector& InVelocity)
{
    if (UUnitCharacterMovementComponent* Movement = Cast<UUnitCharacterMovementComponent>(GetCharacterMovement())) Movement->SetRoundMovementVelocity(IsUnitAlive() ? InVelocity : FVector::ZeroVector);
}

void AUnitBase::OnRep_EquippedSkills()
{
    RefreshSkillPresentation();
}

void AUnitBase::RefreshSkillPresentation()
{
    TMap<FName, bool> Visibility;
    for (const TSoftObjectPtr<USkillDefinitionDataAsset>& Reference : WeaponPresentationSkills)
    {
        USkillDefinitionDataAsset* Skill = Reference.LoadSynchronous();
        FCombatRoundSkill Definition;
        FText Error;
        if (!Skill || !Skill->ResolveRoundSkill(Definition, Error) || !Definition.bUseWeaponTrace || Definition.WeaponComponentName.IsNone()) continue;
        bool& bVisible = Visibility.FindOrAdd(Definition.WeaponComponentName);
        bVisible |= EquippedSkillDataAssets.ContainsByPredicate([Skill](const USkillDefinitionDataAsset* Equipped) { return Equipped && Equipped->GetPrimaryAssetId() == Skill->GetPrimaryAssetId(); });
    }
    for (const TPair<FName, bool>& Entry : Visibility)
    {
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(GetClass(), Entry.Key);
        UStaticMeshComponent* Weapon = Property ? Cast<UStaticMeshComponent>(Property->GetObjectPropertyValue_InContainer(this)) : nullptr;
        if (Weapon && Weapon->GetOwner() == this) Weapon->SetVisibility(Entry.Value);
    }
}

void AUnitBase::SetRoundCastMontage(UAnimMontage* Montage, bool bImmediateStop)
{
    // Actor destruction is replicated separately; do not send cosmetic RPCs from a closing actor.
    // 액터 파괴는 별도로 복제되므로 종료 중인 액터에서 표현 RPC를 보내지 않습니다.
    if (IsActorBeingDestroyed())
    {
        StopRoundCastMontage(0.f);
        return;
    }
    if (HasAuthority()) MulticastSetRoundCastMontage(Montage, bImmediateStop);
}

UAnimMontage* AUnitBase::ResolveRoundCastMontage(UAnimMontage* Montage) const
{
    if (!Montage) return nullptr;
    const TObjectPtr<UAnimMontage>* Override = RoundMontageOverrides.Find(Montage);
    return Override && IsValid(Override->Get()) ? Override->Get() : Montage;
}

bool AUnitBase::HasRoundCastMontageInstance() const
{
    UAnimInstance* AnimInstance = RoundMontageAnimInstance.Get();
    const FAnimMontageInstance* Instance = AnimInstance ? AnimInstance->GetMontageInstanceForID(RoundMontageInstanceId) : nullptr;
    return Instance && Instance->IsValid();
}

void AUnitBase::MulticastSetRoundCastMontage_Implementation(UAnimMontage* Montage, bool bImmediateStop)
{
    StopRoundCastMontage(bImmediateStop ? 0.f : 0.1f);
    if (!Montage || bIsDead || GetNetMode() == NM_DedicatedServer) return;
    if (!FMath::IsFinite(Montage->GetPlayLength()) || Montage->GetPlayLength() <= 0.f || !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RoundAnimation] Invalid montage duration or rate Unit=%s Montage=%s / 몽타주 길이 또는 속도가 유효하지 않아 시간 제한 대기를 사용합니다"), *GetPathName(), *GetPathNameSafe(Montage));
        return;
    }
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance || AnimInstance->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, false) <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RoundAnimation] Montage playback failed Unit=%s Montage=%s AnimInstance=%s / 몽타주 재생 실패: AnimBP, Skeleton, Slot 확인"), *GetPathName(), *GetPathNameSafe(Montage), *GetPathNameSafe(AnimInstance));
        return;
    }
    if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage))
    {
        // Keep movement and damage under the round simulation for the entire cosmetic instance, including blend-out.
        // 블렌드 아웃을 포함한 표현 인스턴스 전체 수명 동안 이동과 피해는 라운드 시뮬레이션이 결정합니다.
        Instance->PushDisableRootMotion();
        RoundMontageAnimInstance = AnimInstance;
        RoundMontageInstanceId = Instance->GetInstanceID();
        UE_LOG(LogTemp, Log, TEXT("[RoundAnimation] Started Unit=%s Montage=%s / 시전 몽타주 시작"), *GetPathName(), *GetPathNameSafe(Montage));
    }
}

void AUnitBase::StopRoundCastMontage(float BlendOutSeconds)
{
    if (UAnimInstance* AnimInstance = RoundMontageAnimInstance.Get())
    {
        if (FAnimMontageInstance* Instance = AnimInstance->GetMontageInstanceForID(RoundMontageInstanceId))
        {
            Instance->Stop(FAlphaBlend(BlendOutSeconds));
        }
    }
    RoundMontageAnimInstance.Reset();
    RoundMontageInstanceId = INDEX_NONE;
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
    StopRoundCastMontage();
    GetCharacterMovement()->DisableMovement();
    SetRoundMovementVelocity(FVector::ZeroVector);
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
    SetRoundCastMontage(nullptr);
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
    }
    GetCharacterMovement()->StopMovementImmediately();
    SetRoundMovementVelocity(FVector::ZeroVector);
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

bool AUnitBase::ConfigureProfession(float MaxHP, int32 AP, int32 SubAP, const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills, float Strength, float Dexterity, float Intelligence)
{
    if (!HasAuthority() || IsBusy() || IsActiveTurn() || !AbilitySystem || !AttributeSet || !FMath::IsFinite(MaxHP) || MaxHP <= 0.0f || AP <= 0 || SubAP < 0)
    {
        return false;
    }
    if (!FMath::IsFinite(Strength) || Strength < 0.0f || Strength > 1000000.0f || !FMath::IsFinite(Dexterity) || Dexterity < 0.0f || Dexterity > 1000000.0f || !FMath::IsFinite(Intelligence) || Intelligence < 0.0f || Intelligence > 1000000.0f)
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
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetStrengthAttribute(), Strength);
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetDexterityAttribute(), Dexterity);
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetIntelligenceAttribute(), Intelligence);
    RefreshSkillPresentation();
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
    RefreshSkillPresentation();
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
