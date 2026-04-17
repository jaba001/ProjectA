#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GA_AttackBase.h"
#include "GA_AreaAttack.generated.h"

class ACombatGridTile;
class ACombatGridManager;
class AUnitBase;
class USkillDefinitionDataAsset;

// Tile-centered area attack Ability
// - The common attack lifecycle is handled by UGA_AttackBase
// - This class is responsible for caching center tile and skill data
// - Calculating area tiles
// - Collecting target units within the area
// - Applying damage to multiple targets
UCLASS()
class PROJECTA_API UGA_AreaAttack : public UGA_AttackBase
{
    GENERATED_BODY()

public:
    UGA_AreaAttack();

protected:
    // Cache the current area attack context
    virtual bool CacheAttackContext() override;

    // Validate the cached context
    virtual bool ValidateAttackContext() const override;

    // Apply attack effect to area targets
    virtual void ApplyAttackEffect() override;

    // Clear cached area attack context
    virtual void ClearCachedAttackContext() override;

protected:
    // Resolve the actual center tile for the area
    ACombatGridTile* ResolveCenterTile() const;

    // Resolve the list of target units within the area
    TArray<AUnitBase*> ResolveAreaTargetUnits() const;

    // Validate whether the unit is a valid target based on team rules
    bool IsValidAreaTargetUnit(AUnitBase* TargetUnit) const;

protected:
    // Currently selected target tile
    UPROPERTY()
    ACombatGridTile* CachedTargetTile = nullptr;

    // Skill definition data currently being executed
    UPROPERTY()
    USkillDefinitionDataAsset* CachedSkillData = nullptr;
};