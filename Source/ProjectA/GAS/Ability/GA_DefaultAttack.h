#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GA_AttackBase.h"
#include "GA_DefaultAttack.generated.h"

class AUnitBase;

UCLASS()
class PROJECTA_API UGA_DefaultAttack : public UGA_AttackBase
{
    GENERATED_BODY()

public:
    UGA_DefaultAttack();

protected:
    // Cache the current default attack target unit
    virtual bool CacheAttackContext() override;

    // Validate whether the cached target unit is valid
    virtual bool ValidateAttackContext() const override;

    // Apply actual damage to the single target
    virtual void ApplyAttackEffect() override;

    // Clear the single-target cache
    virtual void ClearCachedAttackContext() override;

    // Apply the damage GE to the target ASC
    void ApplyDamageEffectToTarget();

protected:
    // Current attack target unit
    UPROPERTY()
    AUnitBase* CachedTargetUnit = nullptr;
};