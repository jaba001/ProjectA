#include "GAS/Ability/GA_AttackBase.h"
#include "Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

UGA_AttackBase::UGA_AttackBase()
{
    // Keep an ability instance per unit
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Default hit event tag
    AttackHitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"));
}

void UGA_AttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // Cache the current activation info
    CachedHandle = Handle;
    CachedActivationInfo = ActivationInfo;
    bDamageAppliedThisActivation = false;
    bFinishRequested = false;

    // Commit cost, cooldown, and other requirements
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Get the unit using this Ability
    CachedOwnerUnit = Cast<AUnitBase>(GetAvatarActorFromActorInfo());

    if (!CachedOwnerUnit)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Attack cannot proceed without a montage and damage GE class
    if (!AttackMontage || !DamageEffectClass)
    {
        FinishAttackAbility(true);
        return;
    }

    // Cache attack context required by child Ability
    if (!CacheAttackContext())
    {
        FinishAttackAbility(true);
        return;
    }

    // Validate the cached context
    if (!ValidateAttackContext())
    {
        FinishAttackAbility(true);
        return;
    }

    // Create the task that waits for the hit event first
    WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, AttackHitEventTag);

    if (WaitHitEventTask)
    {
        WaitHitEventTask->EventReceived.AddDynamic(this, &UGA_AttackBase::OnHitEventReceived);
        WaitHitEventTask->ReadyForActivation();
    }

    // Play the attack montage
    PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);

    if (!PlayMontageTask)
    {
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
    WaitHitEventTask = nullptr;

    // Clear child Ability cache first
    ClearCachedAttackContext();

    // Clear shared cache
    CachedOwnerUnit = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AttackBase::OnAttackMontageCompleted()
{
    FinishAttackAbility(false);
}

void UGA_AttackBase::OnAttackMontageBlendOut()
{
}

void UGA_AttackBase::OnAttackMontageInterrupted()
{
    FinishAttackAbility(true);
}

void UGA_AttackBase::OnAttackMontageCancelled()
{
    FinishAttackAbility(true);
}

void UGA_AttackBase::OnHitEventReceived(FGameplayEventData Payload)
{
    // Apply damage only once per attack activation
    if (bDamageAppliedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Hit Event Ignored | Reason=AlreadyApplied"));
        return;
    }

    bDamageAppliedThisActivation = true;

    // Child Ability is responsible for applying the actual effect
    ApplyAttackEffect();
}

bool UGA_AttackBase::CacheAttackContext()
{
    return true;
}

bool UGA_AttackBase::ValidateAttackContext() const
{
    return true;
}

void UGA_AttackBase::ApplyAttackEffect()
{
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