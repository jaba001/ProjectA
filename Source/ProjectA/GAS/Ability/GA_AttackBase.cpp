#include "GAS/Ability/GA_AttackBase.h"

UGA_AttackBase::UGA_AttackBase()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    AttackReleaseEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Release"));
}

bool UGA_AttackBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    return false;
}

void UGA_AttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // Direct legacy activation fails without spending AP, spawning actors, or applying damage.
    // 직접 호출한 기존 활성화도 AP 소비, 액터 생성, 피해 적용 없이 실패합니다.
    UE_LOG(LogTemp, Warning, TEXT("[GA_AttackBase] Legacy attack execution is unsupported; use the round coordinator."));
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}
