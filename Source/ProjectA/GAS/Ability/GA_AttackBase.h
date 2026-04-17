// GAS PIPELINE SUMMARY 
// 1. Unit (Actor)
//    - GAS를 사용하는 주체 (AUnitBase)
//    - AbilitySystemComponent(ASC)를 소유한다
// 2. AbilitySystemComponent (ASC)  [Engine Provided] Ability / Effect / Attribute / Tag를 관리
// 3. GameplayAbility (Ability) 행동
// 4. GameplayEffect (Effect)   결과
// 5. AttributeSet (Attribute)  숫자상태
// 6. GameplayTag (Tag) 상태분류조건
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_AttackBase.generated.h"

class AUnitBase;
class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

// 공격 Ability 공통 부모
// - 공통 활성화 흐름
// - 히트 이벤트 대기
// - 몽타주 재생
// - 공통 종료 처리
// 를 담당한다.
//
// 실제 타겟 캐싱/검증/효과 적용은 자식 Ability 가 구현한다.

UCLASS(Abstract)
class PROJECTA_API UGA_AttackBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_AttackBase();

protected:
    // Common attack activation entry point
    // Handles commit, owner caching, context caching, and starting montage/hit event tasks
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Common attack end entry point
    // Handles task cleanup and cached state cleanup
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // Called when the attack montage is completed
    UFUNCTION()
    void OnAttackMontageCompleted();

    // Called when the attack montage begins blend-out
    UFUNCTION()
    void OnAttackMontageBlendOut();

    // Called when the attack montage is interrupted
    UFUNCTION()
    void OnAttackMontageInterrupted();

    // Called when the attack montage is cancelled
    UFUNCTION()
    void OnAttackMontageCancelled();

    // Called when the attack hit event is received
    // The default implementation prevents duplicate application and enters the actual attack effect function
    UFUNCTION()
    void OnHitEventReceived(FGameplayEventData Payload);

    // Child Ability caches its own attack context
    // Example:
    // - DefaultAttack: cache a single target unit
    virtual bool CacheAttackContext();

    // Child Ability validates whether the current context is valid
    virtual bool ValidateAttackContext() const;

    // Child Ability applies the actual attack effect
    virtual void ApplyAttackEffect();

    // Child Ability clears its own cached context
    virtual void ClearCachedAttackContext();

    // Common finish handler
    void FinishAttackAbility(bool bWasCancelled);

protected:
    // Unit currently performing the attack
    UPROPERTY()
    AUnitBase* CachedOwnerUnit = nullptr;

    // Whether damage has already been applied during this activation
    UPROPERTY()
    bool bDamageAppliedThisActivation = false;

    // Flag used to prevent duplicate finish handling
    UPROPERTY()
    bool bFinishRequested = false;

protected:
    // Attack montage
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    UAnimMontage* AttackMontage = nullptr;

    // Damage GE class applied to the actual target
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // Base attack damage value
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    float DamageAmount = 10.0f;

    // Event tag used for attack hit timing
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    FGameplayTag AttackHitEventTag;

protected:
    // Cached activation info
    FGameplayAbilitySpecHandle CachedHandle;
    FGameplayAbilityActivationInfo CachedActivationInfo;

    // Cached AbilityTasks
    UPROPERTY()
    UAbilityTask_PlayMontageAndWait* PlayMontageTask = nullptr;

    UPROPERTY()
    UAbilityTask_WaitGameplayEvent* WaitHitEventTask = nullptr;
};