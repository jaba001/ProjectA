#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

#include "GA_DefaultAttack.generated.h"

UCLASS()
class PROJECTA_API UGA_DefaultAttack : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_DefaultAttack();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;
};
