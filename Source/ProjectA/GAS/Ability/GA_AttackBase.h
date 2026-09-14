#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Types/UnitActionTypes.h"
#include "GA_AttackBase.generated.h"

class AActor;
class UAnimMontage;
class UGameplayEffect;

// Existing Blueprint classes retain authored values but cannot execute the retired attack lifecycle.
// 기존 블루프린트 클래스는 제작 수치를 유지하지만 사용 중단된 공격 생명주기를 실행하지 않습니다.
UCLASS(Abstract)
class PROJECTA_API UGA_AttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_AttackBase();

    // Read authored attack power without executing a legacy ability.
    // 기존 어빌리티를 실행하지 않고 제작된 공격 수치를 읽습니다.
    float GetAuthoredDamageAmount() const { return DamageAmount; }
    EUnitActionResult GetActionResult() const { return EUnitActionResult::Failed; }
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Compatibility properties preserve existing assets while round definitions own execution.
    // 라운드 정의가 실행을 소유하며 호환 속성은 기존 에셋을 보존합니다.
    UPROPERTY(BlueprintReadOnly, Category = "Attack|Cost", meta = (DeprecatedProperty, DeprecationMessage = "Use SkillDefinitionDataAsset.ActionPointCost instead."))
    int32 ActionPointCost = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    UAnimMontage* AttackMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    float DamageAmount = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    FGameplayTag AttackReleaseEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TSubclassOf<AActor> SpawnedAttackActorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    FName SpawnSocketName = NAME_None;

    UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    float SpawnedActorTimeout = 10.0f;
};
