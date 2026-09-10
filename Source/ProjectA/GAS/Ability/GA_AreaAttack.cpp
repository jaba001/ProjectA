#include "GAS/Ability/GA_AreaAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"

UGA_AreaAttack::UGA_AreaAttack()
{
    DamageEffectClass = UGE_Damage::StaticClass();
}

bool UGA_AreaAttack::CacheAttackContext()
{
    if (!CachedOwnerUnit)
    {
        return false;
    }

    CachedTargetTile = CachedOwnerUnit->PendingSkillTargetTile;
    CachedSkillData = CachedOwnerUnit->PendingSkillData;

    if (!CachedSkillData)
    {
        return false;
    }

    return true;
}

bool UGA_AreaAttack::ValidateAttackContext() const
{
    if (!CachedSkillData)
    {
        return false;
    }

    if (!CachedTargetTile && CachedSkillData->AreaType != ESkillAreaType::AroundSelf)
    {
        return false;
    }

    if (!ResolveCenterTile())
    {
        return false;
    }

    return true;
}

void UGA_AreaAttack::ApplyAttackEffect()
{
    if (!CachedOwnerUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AreaAttack] ApplyAttackEffect Failed | CachedOwnerUnit is null"));
        return;
    }

    TArray<AUnitBase*> TargetUnits = ResolveAreaTargetUnits();

    if (TargetUnits.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AreaAttack] ApplyAttackEffect Skipped | No Targets | Owner=%s"), *GetNameSafe(CachedOwnerUnit));
        return;
    }

    UCombatEffectLibrary::ApplyDamageToUnits(CachedOwnerUnit, TargetUnits, DamageEffectClass, DamageAmount);
}

void UGA_AreaAttack::ClearCachedAttackContext()
{
    CachedTargetTile = nullptr;
    CachedSkillData = nullptr;
}

ACombatGridTile* UGA_AreaAttack::ResolveCenterTile() const
{
    return UCombatTargetingLibrary::ResolveSkillAreaCenter(CachedOwnerUnit, CachedSkillData, CachedTargetTile);
}

TArray<AUnitBase*> UGA_AreaAttack::ResolveAreaTargetUnits() const
{
    return UCombatTargetingLibrary::ResolveSkillAreaTargets(CachedOwnerUnit, CachedSkillData, CachedTargetTile);
}

bool UGA_AreaAttack::IsValidAreaTargetUnit(AUnitBase* TargetUnit) const
{
    return UCombatTargetingLibrary::IsSkillEffectTarget(CachedOwnerUnit, CachedSkillData, TargetUnit);
}
