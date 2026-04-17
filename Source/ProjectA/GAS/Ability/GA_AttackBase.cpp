#include "GAS/Ability/GA_AttackBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "Unit/UnitBase.h"

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

    // Commit cost, cooldown, and other requirements
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=CommitAbilityFailed | Ability=%s"), *GetNameSafe(GetClass()));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Get the unit using this Ability
    CachedOwnerUnit = Cast<AUnitBase>(GetAvatarActorFromActorInfo());

    if (!CachedOwnerUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=CachedOwnerUnitNull | Ability=%s"), *GetNameSafe(GetClass()));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

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

    // No-montage attacks release immediately without waiting for notify.
    // 노티 기반 타이밍을 기다리지 않고 즉시 릴리즈한다.
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
    // Fallback: if release event was not received from montage notify,
    // release once at montage completion.
    // 폴백: 몽타주 노티에서 릴리즈 이벤트가 오지 않으면 완료 시점에 1회 릴리즈한다.
    if (!bAttackReleasedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Release Event Missing | Applying fallback release on montage complete | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        ReleaseAttack();
    }

    FinishAttackAbility(false);
}

void UGA_AttackBase::OnAttackMontageBlendOut()
{
    // Some montages may only reach blend-out callback depending on task/event timing.
    // Ensure release is not lost when notify event is missing.
    // 일부 몽타주는 타이밍에 따라 블렌드아웃만 호출될 수 있어 릴리즈 누락을 방지한다.
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
    if (bAttackReleasedThisActivation)
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
    if (!CachedOwnerUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Failed | Reason=OwnerNull"));
        return;
    }

    if (!SpawnedAttackActorClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Failed | Reason=AttackActorClassNull | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Failed | Reason=WorldNull | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        return;
    }

    FTransform SpawnTransform = CachedOwnerUnit->GetActorTransform();

    if (USkeletalMeshComponent* MeshComp = CachedOwnerUnit->GetMesh())
    {
        if (!SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
        {
            SpawnTransform = MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = CachedOwnerUnit;
    SpawnParams.Instigator = CachedOwnerUnit;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedActor = World->SpawnActor<AActor>(SpawnedAttackActorClass, SpawnTransform, SpawnParams);

    if (!SpawnedActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] SpawnAttackActor Failed | Reason=SpawnFailed | Owner=%s | AttackActorClass=%s | SpawnSocket=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(SpawnedAttackActorClass), *SpawnSocketName.ToString());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[GA_AttackBase] SpawnAttackActor Success | Owner=%s | SpawnedActor=%s | SpawnSocket=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(SpawnedActor), *SpawnSocketName.ToString());
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

    // Notify UnitBase that the attack has finished and begin return flow
    if (CachedOwnerUnit)
    {
        CachedOwnerUnit->OnSkillFinished();
    }

    EndAbility(CachedHandle, CurrentActorInfo, CachedActivationInfo, false, bWasCancelled);
}