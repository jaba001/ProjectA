#include "GA_DefaultAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "GAS/Attribute/AS_Unit.h"

// GAS PIPELINE SUMMARY 
// 1. Unit (Actor)
//    - GAS를 사용하는 주체 (AUnitBase)
//    - AbilitySystemComponent(ASC)를 소유한다
// 2. AbilitySystemComponent (ASC)  [Engine Provided] Ability / Effect / Attribute / Tag를 관리
// 3. GameplayAbility (Ability) 행동
// 4. GameplayEffect (Effect)   결과
// 5. AttributeSet (Attribute)  숫자상태
// 6. GameplayTag (Tag) 상태분류조건


UGA_DefaultAttack::UGA_DefaultAttack()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_DefaultAttack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    if (ASC)
    {
        ASC->ApplyGameplayEffectToSelf(
            UGE_Damage::StaticClass()->GetDefaultObject<UGameplayEffect>(),
            1.f,
            ASC->MakeEffectContext()
        );

        //테스트코드
        //float CurrentHP = ASC->GetNumericAttribute(UAS_Unit::GetHPAttribute());
        //UE_LOG(LogTemp, Warning, TEXT("HP after damage: %f"), CurrentHP);
    }


    

    EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}