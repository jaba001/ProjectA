#include "GAS/Ability/GA_AttackBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "Grid/Combat/CombatGridTile.h"
#include "Combat/SkillActor/AttackSkillActorBase.h"
#include "Combat/SkillActor/SkillActorBase.h"
#include "Unit/UnitBase.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/Library/CombatTargetingLibrary.h"

UGA_AttackBase::UGA_AttackBase()
{
    // Keep an ability instance per unit
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Default release event tag
    AttackReleaseEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Release"));
}

void UGA_AttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // Cache the current activation info
    CachedHandle = Handle;
    CachedActivationInfo = ActivationInfo;
    bAttackReleasedThisActivation = false;
    bFinishRequested = false;
    ActionResult = EUnitActionResult::Failed;

    // Get the unit using this Ability
    CachedOwnerUnit = Cast<AUnitBase>(GetAvatarActorFromActorInfo());

    if (!CachedOwnerUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=CachedOwnerUnitNull | Ability=%s"), *GetNameSafe(GetClass()));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Use the selected skill definition even when several definitions share an ability class.
    // 여러 스킬 정의가 같은 어빌리티 클래스를 사용해도 선택한 정의의 비용을 사용합니다.
    const USkillDefinitionDataAsset* SkillData = CachedOwnerUnit->PendingSkillData;
    if (!UCombatTargetingLibrary::IsSupportedSkillArea(SkillData) || SkillData->AbilityClass != GetClass() || !CachedOwnerUnit->HasEnoughActionPoint(SkillData->ActionPointCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=InvalidSkillOrAP | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    const int32 SkillActionPointCost = SkillData->ActionPointCost;

    // Cache attack context required by child Ability
    if (!CacheAttackContext())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=CacheAttackContextFailed | Ability=%s | Owner=%s"), *GetNameSafe(GetClass()), *GetNameSafe(CachedOwnerUnit));
        FinishAttackAbility(true);
        return;
    }

    // Validate the cached context
    if (!ValidateAttackContext())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=ValidateAttackContextFailed | Ability=%s | Owner=%s"), *GetNameSafe(GetClass()), *GetNameSafe(CachedOwnerUnit));
        FinishAttackAbility(true);
        return;
    }

    // Charge AP only after context validation and a successful GAS commit.
    // 컨텍스트 검증과 GAS 커밋이 성공한 뒤에만 AP를 차감합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=CommitAbilityFailed | Ability=%s"), *GetNameSafe(GetClass()));
        FinishAttackAbility(true);
        return;
    }

    if (!CachedOwnerUnit->ConsumeActionPoint(SkillActionPointCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=ConsumeActionPointFailed | Cost=%d | Owner=%s"), SkillActionPointCost, *GetNameSafe(CachedOwnerUnit));
        FinishAttackAbility(true);
        return;
    }

    ActionResult = EUnitActionResult::Succeeded;

    // No-montage attacks release immediately without waiting for notify.
    // 몽타주가 없는 공격은 노티파이를 기다리지 않고 즉시 발사합니다.
    if (!AttackMontage)
    {
        UE_LOG(LogTemp, Log, TEXT("[GA_AttackBase] ActivateAbility | NoMontageImmediateRelease | Ability=%s | Owner=%s"), *GetNameSafe(GetClass()), *GetNameSafe(CachedOwnerUnit));
        ReleaseAttack();
        FinishAttackAbility(false);
        return;
    }

    // Create the task that waits for the release event first
    WaitReleaseEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, AttackReleaseEventTag);

    if (WaitReleaseEventTask)
    {
        WaitReleaseEventTask->EventReceived.AddDynamic(this, &UGA_AttackBase::OnReleaseEventReceived);
        WaitReleaseEventTask->ReadyForActivation();
    }

    // Play the attack montage
    PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);

    if (!PlayMontageTask)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=PlayMontageTaskNull | Ability=%s"), *GetNameSafe(GetClass()));
        ActionResult = EUnitActionResult::Failed;
        FinishAttackAbility(true);
        return;
    }

    // Bind montage end-related callbacks
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_AttackBase::OnAttackMontageCompleted);
    PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_AttackBase::OnAttackMontageBlendOut);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_AttackBase::OnAttackMontageInterrupted);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_AttackBase::OnAttackMontageCancelled);
    PlayMontageTask->ReadyForActivation();
}

void UGA_AttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    bFinishRequested = true;
    if (bWasCancelled && ActionResult == EUnitActionResult::Succeeded)
    {
        ActionResult = EUnitActionResult::Cancelled;
    }

    // Clear cached task references
    PlayMontageTask = nullptr;
    WaitReleaseEventTask = nullptr;

    // Clear child Ability cache first
    ClearCachedAttackContext();

    // Clear shared cache
    CachedOwnerUnit = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AttackBase::OnAttackMontageCompleted()
{
    if (bFinishRequested)
    {
        return;
    }

    // Fallback: if release event was not received from montage notify,
    // release once at montage completion.
    if (!bAttackReleasedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Release Event Missing | Applying fallback release on montage complete | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        ReleaseAttack();
    }

    FinishAttackAbility(false);
}

void UGA_AttackBase::OnAttackMontageBlendOut()
{
    if (bFinishRequested)
    {
        return;
    }

    // Some montages may only reach blend-out callback depending on task/event timing.
    // Ensure release is not lost when notify event is missing.
    if (!bAttackReleasedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Release Event Missing | Applying fallback release on montage blend-out | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        ReleaseAttack();
    }
}

void UGA_AttackBase::OnAttackMontageInterrupted()
{
    FinishAttackAbility(true);
}

void UGA_AttackBase::OnAttackMontageCancelled()
{
    FinishAttackAbility(true);
}

void UGA_AttackBase::OnReleaseEventReceived(FGameplayEventData Payload)
{
    (void)Payload;
    ReleaseAttack();
}

bool UGA_AttackBase::CacheAttackContext()
{
    return true;
}

bool UGA_AttackBase::ValidateAttackContext() const
{
    return true;
}

void UGA_AttackBase::ReleaseAttack()
{
    // Apply attack release only once per activation
    if (bAttackReleasedThisActivation || bFinishRequested)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ReleaseAttack Ignored | Reason=AlreadyReleased"));
        return;
    }

    bAttackReleasedThisActivation = true;

    if (SpawnedAttackActorClass)
    {
        SpawnAttackActor();
        return;
    }

    ApplyAttackEffect();
}

void UGA_AttackBase::ApplyAttackEffect()
{
}

void UGA_AttackBase::SpawnAttackActor()
{
    if (!CachedOwnerUnit || !SpawnedAttackActorClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Failed | OwnerOrClassNull | Owner=%s | ActorClass=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(SpawnedAttackActorClass));
        return;
    }

    FVector SpawnLocation = CachedOwnerUnit->GetActorLocation();
    FRotator SpawnRotation = CachedOwnerUnit->GetActorRotation();

    USkeletalMeshComponent* OwnerMesh = CachedOwnerUnit->GetMesh();

    if (OwnerMesh && SpawnSocketName != NAME_None && OwnerMesh->DoesSocketExist(SpawnSocketName))
    {
        SpawnLocation = OwnerMesh->GetSocketLocation(SpawnSocketName);
        SpawnRotation = OwnerMesh->GetSocketRotation(SpawnSocketName);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = CachedOwnerUnit;
    SpawnParams.Instigator = CachedOwnerUnit;

    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(SpawnedAttackActorClass, SpawnLocation, SpawnRotation, SpawnParams);

    if (!SpawnedActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Failed | SpawnedActorNull | Owner=%s | ActorClass=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(SpawnedAttackActorClass));
        return;
    }

    FSkillActorInitData InitData;
    InitData.SourceUnit = CachedOwnerUnit;
    InitData.SkillData = CachedOwnerUnit->PendingSkillData;
    InitData.TargetTile = CachedOwnerUnit->PendingSkillTargetTile;
    if (InitData.TargetTile)
    {
        InitData.TargetWorldLocation = InitData.TargetTile->GetActorLocation();
    }
    else
    {
        InitData.TargetWorldLocation = SpawnLocation;
    }

    if (AAttackSkillActorBase* AttackSkillActor = Cast<AAttackSkillActorBase>(SpawnedActor))
    {
        AttackSkillActor->InitializeAttackSkillActor(InitData, DamageEffectClass, DamageAmount);
        UE_LOG(LogTemp, Log, TEXT("[GA_AttackBase] SpawnAttackActor InitializedAttackSkillActor | Actor=%s | Owner=%s"), *GetNameSafe(SpawnedActor), *GetNameSafe(CachedOwnerUnit));
        return;
    }

    if (ASkillActorBase* SkillActor = Cast<ASkillActorBase>(SpawnedActor))
    {
        SkillActor->InitializeSkillActor(InitData);
        UE_LOG(LogTemp, Log, TEXT("[GA_AttackBase] SpawnAttackActor InitializedSkillActor | Actor=%s | Owner=%s"), *GetNameSafe(SpawnedActor), *GetNameSafe(CachedOwnerUnit));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Warning | Spawned actor is not SkillActorBase | Actor=%s | ActorClass=%s"), *GetNameSafe(SpawnedActor), *GetNameSafe(SpawnedActor->GetClass()));
}

void UGA_AttackBase::ClearCachedAttackContext()
{
}

void UGA_AttackBase::FinishAttackAbility(bool bWasCancelled)
{
    // Prevent duplicate finish handling
    if (bFinishRequested)
    {
        return;
    }

    bFinishRequested = true;

    // Unit completion is observed through GAS after all ability cleanup.
    // 어빌리티 정리가 끝난 뒤 GAS를 통해 유닛 완료를 관찰합니다.
    EndAbility(CachedHandle, CurrentActorInfo, CachedActivationInfo, false, bWasCancelled);
}
