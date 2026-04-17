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

    // Attack cannot proceed without a montage and damage GE class
    if (!AttackMontage || !DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] ActivateAbility Failed | Reason=MissingMontageOrDamageEffect | Montage=%d | DamageEffect=%d"),
            AttackMontage ? 1 : 0,
            DamageEffectClass ? 1 : 0);
        FinishAttackAbility(true);
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

    // No-montage attacks (e.g., tile AoE) apply immediately without waiting hit event.
    if (!AttackMontage)
    {
        UE_LOG(LogTemp, Log, TEXT("[GA_AttackBase] ActivateAbility | NoMontageImmediateApply | Ability=%s | Owner=%s"), *GetNameSafe(GetClass()), *GetNameSafe(CachedOwnerUnit));
        bDamageAppliedThisActivation = true;
        ApplyAttackEffect();
        FinishAttackAbility(false);
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
    WaitHitEventTask = nullptr;

    // Clear child Ability cache first
    ClearCachedAttackContext();

    // Clear shared cache
    CachedOwnerUnit = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AttackBase::OnAttackMontageCompleted()
{
    // Fallback: if hit event was not received from montage notify,
    // apply attack effect once at montage completion.
    // 폴백: 몽타주 노티파이에서 히트 이벤트를 못 받으면
    // 몽타주 완료 시점에 공격 효과를 1회 적용한다.
    if (!bDamageAppliedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Hit Event Missing | Applying fallback damage on montage complete | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        bDamageAppliedThisActivation = true;
        ApplyAttackEffect();
    }

    FinishAttackAbility(false);
}

void UGA_AttackBase::OnAttackMontageBlendOut()
{
    // Some montages may only reach blend-out callback depending on task/event timing.
    // Ensure attack effect is not lost when hit notify event is missing.
    // 태스크/이벤트 타이밍에 따라 일부 몽타주는 블렌드아웃 콜백만 호출될 수 있다.
    // 히트 노티파이 이벤트가 없을 때도 공격 효과가 누락되지 않도록 보장한다.
    if (!bDamageAppliedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Hit Event Missing | Applying fallback damage on montage blend-out | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        bDamageAppliedThisActivation = true;
        ApplyAttackEffect();
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