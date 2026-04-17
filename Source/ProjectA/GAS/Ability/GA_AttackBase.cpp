#include "GAS/Ability/GA_AttackBase.h"
#include "Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

UGA_AttackBase::UGA_AttackBase()
{
    // 유닛 단위로 인스턴스를 유지한다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 히트 이벤트 태그 기본값
    AttackHitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"));
}

void UGA_AttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 현재 활성화 정보를 캐싱한다.
    CachedHandle = Handle;
    CachedActivationInfo = ActivationInfo;
    bDamageAppliedThisActivation = false;
    bFinishRequested = false;

    // 비용/쿨다운 등을 커밋한다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 현재 Ability 를 사용하는 유닛을 가져온다.
    CachedOwnerUnit = Cast<AUnitBase>(GetAvatarActorFromActorInfo());

    if (!CachedOwnerUnit)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 몽타주와 데미지 GE 클래스가 없으면 공격을 진행할 수 없다.
    if (!AttackMontage || !DamageEffectClass)
    {
        FinishAttackAbility(true);
        return;
    }

    // 자식 Ability 가 필요한 공격 컨텍스트를 캐싱한다.
    if (!CacheAttackContext())
    {
        FinishAttackAbility(true);
        return;
    }

    // 캐싱된 컨텍스트가 실제로 유효한지 검사한다.
    if (!ValidateAttackContext())
    {
        FinishAttackAbility(true);
        return;
    }

    // 히트 이벤트를 기다리는 태스크를 먼저 생성한다.
    WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, AttackHitEventTag);

    if (WaitHitEventTask)
    {
        WaitHitEventTask->EventReceived.AddDynamic(this, &UGA_AttackBase::OnHitEventReceived);
        WaitHitEventTask->ReadyForActivation();
    }

    // 공격 몽타주를 재생한다.
    PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);

    if (!PlayMontageTask)
    {
        FinishAttackAbility(true);
        return;
    }

    // 몽타주 종료 관련 콜백을 연결한다.
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_AttackBase::OnAttackMontageCompleted);
    PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_AttackBase::OnAttackMontageBlendOut);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_AttackBase::OnAttackMontageInterrupted);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_AttackBase::OnAttackMontageCancelled);
    PlayMontageTask->ReadyForActivation();
}

void UGA_AttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 태스크 캐시를 정리한다.
    PlayMontageTask = nullptr;
    WaitHitEventTask = nullptr;

    // 자식 Ability 캐시를 먼저 정리한다.
    ClearCachedAttackContext();

    // 공통 캐시를 정리한다.
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
    // 한 번의 공격 활성화에서 데미지는 한 번만 적용한다.
    if (bDamageAppliedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Hit Event Ignored | Reason=AlreadyApplied"));
        return;
    }

    bDamageAppliedThisActivation = true;

    // 실제 효과 적용은 자식 Ability 가 담당한다.
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
    // 중복 종료 방지
    if (bFinishRequested)
    {
        return;
    }

    bFinishRequested = true;

    // 공격이 끝났음을 UnitBase 에 알리고 복귀를 시작한다.
    if (CachedOwnerUnit)
    {
        CachedOwnerUnit->OnSkillFinished();
    }

    EndAbility(CachedHandle, CurrentActorInfo, CachedActivationInfo, false, bWasCancelled);
}