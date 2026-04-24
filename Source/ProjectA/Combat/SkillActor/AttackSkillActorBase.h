#pragma once

#include "CoreMinimal.h"
#include "Combat/SkillActor/SkillActorBase.h"
#include "AttackSkillActorBase.generated.h"

class AUnitBase;
class UGameplayEffect;

// Base actor for spawned attack skill objects.
// 공격 스킬 액터 기본 클래스
UCLASS()
class PROJECTA_API AAttackSkillActorBase : public ASkillActorBase
{
    GENERATED_BODY()

public:
    AAttackSkillActorBase();

public:
    // Initialize this attack skill actor with runtime attack context.
    // 공격 스킬 액터를 런타임 공격 컨텍스트로 초기화합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    virtual void InitializeAttackSkillActor(const FSkillActorInitData& InitData, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageAmount);

    // Returns the damage effect class.
    // 피해 효과 클래스를 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    TSubclassOf<UGameplayEffect> GetDamageEffectClass() const;

    // Returns the damage amount.
    // 피해량을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    float GetDamageAmount() const;

protected:
    // Execute actual attack impact logic.
    // 실제 공격 임팩트 로직을 실행합니다.
    virtual void HandleImpact() override;

    // Apply impact damage by tile-based skill rules.
    // 타일 기반 스킬 규칙에 따라 임팩트 피해를 적용합니다.
    virtual void ApplyImpactEffect();

    // Resolve impact target units from target tile and skill data.
    // 대상 타일과 스킬 데이터를 기반으로 임팩트 대상 유닛을 해결합니다.
    virtual TArray<AUnitBase*> ResolveImpactTargetUnits() const;

    // Check whether target unit is valid for this attack skill.
    // 대상 유닛이 이 공격 스킬에 유효한지 확인합니다.
    virtual bool IsValidImpactTargetUnit(AUnitBase* TargetUnit) const;

protected:
    // Damage effect class passed from ability.
    // 능력에서 전달된 피해 효과 클래스.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    // Damage amount passed from ability.
    // 능력에서 전달된 피해량.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Attack")
    float DamageAmount = 0.0f;
};