#pragma once

#include "CoreMinimal.h"
#include "Combat/SkillActor/SkillActorBase.h"
#include "AttackSkillActorBase.generated.h"

class ACombatGridManager;
class ACombatGridTile;
class AUnitBase;
class UGameplayEffect;

UCLASS()
class PROJECTA_API AAttackSkillActorBase : public ASkillActorBase
{
    GENERATED_BODY()

public:
    AAttackSkillActorBase();

public:
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    virtual void InitializeAttackSkillActor(const FSkillActorInitData& InitData, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageAmount);

    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    TSubclassOf<UGameplayEffect> GetDamageEffectClass() const;

    UFUNCTION(BlueprintCallable, Category = "SkillActor|Attack")
    float GetDamageAmount() const;

protected:
    virtual void HandleImpact() override;
    virtual void ApplyImpactEffect();
    virtual TArray<AUnitBase*> ResolveImpactTargetUnits() const;
    virtual bool IsValidImpactTargetUnit(AUnitBase* TargetUnit) const;

    virtual bool HasValidImpactContext() const;
    virtual ACombatGridManager* ResolveCombatGridManager() const;
    virtual TArray<ACombatGridTile*> ResolveImpactAreaTiles(ACombatGridManager* CombatGridManager) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Attack")
    float DamageAmount = 0.0f;

    UPROPERTY(Transient)
    mutable TObjectPtr<ACombatGridManager> CachedCombatGridManager = nullptr;
};
