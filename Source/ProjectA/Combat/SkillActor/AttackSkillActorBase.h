#pragma once

#include "CoreMinimal.h"
#include "Combat/SkillActor/SkillActorBase.h"
#include "AttackSkillActorBase.generated.h"

class AUnitBase;
class UGameplayEffect;

// Base actor for spawned attack skill objects.
// 스폰형 공격 스킬 오브젝트의 공통 베이스 액터.
UCLASS()
class PROJECTA_API AAttackSkillActorBase : public ASkillActorBase
{
    GENERATED_BODY()

public:
    AAttackSkillActorBase();

public:
    // Initialize this attack skill actor with runtime attack context.
    // 런타임 공격 컨텍스트로 공격 스킬 액터를 초기화한다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    virtual void InitializeAttackSkillActor(const FSkillActorInitData& InitData, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageAmount);

    // Returns the damage effect class.
    // 데미지 이펙트 클래스를 반환한다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    TSubclassOf<UGameplayEffect> GetDamageEffectClass() const;

    // Returns the damage amount.
    // 데미지 수치를 반환한다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    float GetDamageAmount() const;

protected:
    // Execute actual attack impact logic.
    // 실제 공격 임팩트 로직을 실행한다.
    virtual void HandleImpact() override;

    // Apply impact damage by tile-based skill rules.
    // 타일 기반 스킬 규칙으로 임팩트 데미지를 적용한다.
    virtual void ApplyImpactEffect();

    // Resolve impact target units from target tile and skill data.
    // 타겟 타일과 스킬 데이터 기준으로 임팩트 대상 유닛을 계산한다.
    virtual TArray<AUnitBase*> ResolveImpactTargetUnits() const;

    // Check whether target unit is valid for this attack skill.
    // 이 공격 스킬의 유효 대상 유닛인지 검사한다.
    virtual bool IsValidImpactTargetUnit(AUnitBase* TargetUnit) const;

protected:
    // Damage effect class passed from ability.
    // 어빌리티로부터 전달된 데미지 이펙트 클래스.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // Damage amount passed from ability.
    // 어빌리티로부터 전달된 데미지 수치.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Attack")
    float DamageAmount = 0.0f;
};