#include "GAS/Ability/GA_DefaultAttack.h"
#include "Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

UGA_DefaultAttack::UGA_DefaultAttack()
{
    // 유닛 단위로 인스턴스를 유지한다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 히트 이벤트 태그 기본값
    AttackHitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"));
}

void UGA_DefaultAttack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ActivateAbility | Owner=%s | Target=%s | HitTag=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(CachedTargetUnit), *AttackHitEventTag.ToString());

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

    // 현재 Ability를 사용하는 유닛을 가져온다.
    CachedOwnerUnit = Cast<AUnitBase>(GetAvatarActorFromActorInfo());

    if (!CachedOwnerUnit)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 현재 공격 대상 유닛을 가져온다.
    CachedTargetUnit = CachedOwnerUnit->PendingTargetUnit;

    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ActivateAbility | Owner=%s | Target=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(CachedTargetUnit));

    if (!CachedTargetUnit || !CachedTargetUnit->IsUnitAlive())
    {
        FinishAttackAbility(true);
        return;
    }

    // 몽타주와 데미지 GE 클래스가 없으면 공격을 진행할 수 없다.
    if (!AttackMontage || !DamageEffectClass)
    {
        FinishAttackAbility(true);
        return;
    }

    // 히트 이벤트를 기다리는 태스크를 먼저 생성한다.
    WaitHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, AttackHitEventTag);

    if (WaitHitEventTask)
    {
        WaitHitEventTask->EventReceived.AddDynamic(this, &UGA_DefaultAttack::OnHitEventReceived);
        WaitHitEventTask->ReadyForActivation();
    }

    // 공격 몽타주를 재생한다.
    PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        AttackMontage
    );

    if (!PlayMontageTask)
    {
        FinishAttackAbility(true);
        return;
    }

    // 몽타주 종료 관련 콜백을 연결한다.
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_DefaultAttack::OnAttackMontageCompleted);
    PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_DefaultAttack::OnAttackMontageBlendOut);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_DefaultAttack::OnAttackMontageInterrupted);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_DefaultAttack::OnAttackMontageCancelled);
    PlayMontageTask->ReadyForActivation();
}

void UGA_DefaultAttack::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // 태스크 캐시를 정리한다.
    PlayMontageTask = nullptr;
    WaitHitEventTask = nullptr;
    CachedOwnerUnit = nullptr;
    CachedTargetUnit = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_DefaultAttack::OnAttackMontageCompleted()
{
    FinishAttackAbility(false);
}

void UGA_DefaultAttack::OnAttackMontageBlendOut()
{
}

void UGA_DefaultAttack::OnAttackMontageInterrupted()
{
    FinishAttackAbility(true);
}

void UGA_DefaultAttack::OnAttackMontageCancelled()
{
    FinishAttackAbility(true);
}

void UGA_DefaultAttack::OnHitEventReceived(FGameplayEventData Payload)
{
    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] Hit Event Received | PayloadTag=%s | Owner=%s | Target=%s"), *Payload.EventTag.ToString(), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(CachedTargetUnit));

    // 한 번의 기본공격에서 데미지는 한 번만 적용한다.
    if (bDamageAppliedThisActivation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] Hit Event Ignored | Reason=AlreadyApplied"));
        return;
    }

    bDamageAppliedThisActivation = true;

    ApplyDamageEffectToTarget();
}

void UGA_DefaultAttack::ApplyDamageEffectToTarget()
{
    // 유효한 공격자/타겟이 없으면 종료
    if (!CachedOwnerUnit || !CachedTargetUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid Owner or Target"));
        return;
    }

    if (!CachedTargetUnit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Target Dead | Target=%s"),
            *GetNameSafe(CachedTargetUnit));
        return;
    }

    UAbilitySystemComponent* SourceASC = CachedOwnerUnit->GetAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = CachedTargetUnit->GetAbilitySystemComponent();

    if (!SourceASC || !TargetASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid ASC | Source=%s | Target=%s"),
            *GetNameSafe(CachedOwnerUnit),
            *GetNameSafe(CachedTargetUnit));
        return;
    }

    if (!DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | DamageEffectClass is null"));
        return;
    }

    const float BeforeHP = TargetASC->GetNumericAttribute(UAS_Unit::GetHPAttribute());

    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] Before Apply | Target=%s | HP=%.1f"), *GetNameSafe(CachedTargetUnit), BeforeHP);

    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddSourceObject(CachedOwnerUnit);

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid SpecHandle"));
        return;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

    SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, -DamageAmount);

    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget | Source=%s | Target=%s | Damage=%.1f | Tag=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(CachedTargetUnit), DamageAmount, *DamageTag.ToString());

    const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] Apply Result | HandleValid=%d"), AppliedHandle.IsValid() ? 1 : 0);

    const float AfterHP = TargetASC->GetNumericAttribute(UAS_Unit::GetHPAttribute());

    //UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] After Apply | Target=%s | HP=%.1f"), *GetNameSafe(CachedTargetUnit), AfterHP);
}

void UGA_DefaultAttack::FinishAttackAbility(bool bWasCancelled)
{
    // 중복 종료 방지
    if (bFinishRequested)
    {
        return;
    }

    bFinishRequested = true;

    // 공격이 끝났음을 UnitBase에 알리고 복귀를 시작한다.
    if (CachedOwnerUnit)
    {
        CachedOwnerUnit->OnSkillFinished();
    }

    EndAbility(CachedHandle, CurrentActorInfo, CachedActivationInfo, false, bWasCancelled);
}