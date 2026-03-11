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
#include "GA_DefaultAttack.generated.h"

class AUnitBase;
class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

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
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    // 공격 몽타주 완료 시 호출된다.
    UFUNCTION()
    void OnAttackMontageCompleted();

    // 공격 몽타주 블렌드아웃 시 호출된다.
    UFUNCTION()
    void OnAttackMontageBlendOut();

    // 공격 몽타주 인터럽트 시 호출된다.
    UFUNCTION()
    void OnAttackMontageInterrupted();

    // 공격 몽타주 취소 시 호출된다.
    UFUNCTION()
    void OnAttackMontageCancelled();

    // 공격 히트 이벤트를 수신했을 때 호출된다.
    UFUNCTION()
    void OnHitEventReceived(FGameplayEventData Payload);

    // 공통 종료 처리 함수
    void FinishAttackAbility(bool bWasCancelled);

    // 실제 타겟 ASC에 데미지 GE를 적용한다.
    void ApplyDamageEffectToTarget();

protected:
    // 현재 공격을 수행하는 유닛
    UPROPERTY()
    AUnitBase* CachedOwnerUnit = nullptr;

    // 현재 공격 대상 유닛
    UPROPERTY()
    AUnitBase* CachedTargetUnit = nullptr;

    // 이번 활성화에서 이미 데미지를 적용했는지 여부
    UPROPERTY()
    bool bDamageAppliedThisActivation = false;

    // 종료 처리를 중복 호출하지 않기 위한 플래그
    UPROPERTY()
    bool bFinishRequested = false;

protected:
    // 공격 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    UAnimMontage* AttackMontage = nullptr;

    // 실제 타겟에게 적용할 데미지 GE 클래스
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // 기본공격 데미지 수치
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    float DamageAmount = 10.0f;

    // 공격 히트 타이밍용 이벤트 태그
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    FGameplayTag AttackHitEventTag;

protected:
    // 현재 활성화 정보 캐싱
    FGameplayAbilitySpecHandle CachedHandle;
    FGameplayAbilityActivationInfo CachedActivationInfo;

    // AbilityTask 캐싱
    UPROPERTY()
    UAbilityTask_PlayMontageAndWait* PlayMontageTask = nullptr;

    UPROPERTY()
    UAbilityTask_WaitGameplayEvent* WaitHitEventTask = nullptr;
};