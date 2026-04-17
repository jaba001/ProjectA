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
    // 공통 공격 활성화 진입점
    // Commit, 소유자 캐싱, 컨텍스트 캐싱, 몽타주/히트 이벤트 태스크 시작을 담당한다.
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // 공통 공격 종료 진입점
    // 태스크와 캐시 정리를 담당한다.
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

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
    // 기본 구현은 중복 적용을 막고 실제 공격 효과 적용 함수로 진입한다.
    UFUNCTION()
    void OnHitEventReceived(FGameplayEventData Payload);

    // 자식 Ability 가 자기 공격 컨텍스트를 캐싱한다.
    // 예:
    // - DefaultAttack: 단일 타겟 유닛 캐싱
    virtual bool CacheAttackContext();

    // 자식 Ability 가 현재 컨텍스트가 유효한지 검사한다.
    virtual bool ValidateAttackContext() const;

    // 자식 Ability 가 실제 공격 효과를 적용한다.
    virtual void ApplyAttackEffect();

    // 자식 Ability 가 자기 캐시를 정리한다.
    virtual void ClearCachedAttackContext();

    // 공통 종료 처리 함수
    void FinishAttackAbility(bool bWasCancelled);

protected:
    // 현재 공격을 수행하는 유닛
    UPROPERTY()
    AUnitBase* CachedOwnerUnit = nullptr;

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