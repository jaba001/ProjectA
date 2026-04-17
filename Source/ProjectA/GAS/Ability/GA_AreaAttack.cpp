#include "GAS/Ability/GA_AreaAttack.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "GAS/Library/CombatEffectLibrary.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"

UGA_AreaAttack::UGA_AreaAttack()
{
}

bool UGA_AreaAttack::CacheAttackContext()
{
    if (!CachedOwnerUnit)
    {
        return false;
    }

    CachedTargetTile = CachedOwnerUnit->PendingSkillTargetTile;
    CachedSkillData = CachedOwnerUnit->PendingSkillData;

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
    if (!CachedOwnerUnit || !CachedSkillData)
    {
        return nullptr;
    }

    if (CachedSkillData->AreaType == ESkillAreaType::AroundSelf)
    {
        return CachedOwnerUnit->GetCurrentTile();
    }

    if (CachedSkillData->AreaType == ESkillAreaType::AroundTarget)
    {
        return CachedTargetTile;
    }

    if (CachedSkillData->AreaType == ESkillAreaType::Single)
    {
        return CachedTargetTile;
    }

    return nullptr;
}

TArray<AUnitBase*> UGA_AreaAttack::ResolveAreaTargetUnits() const
{
    TArray<AUnitBase*> Result;

    if (!CachedOwnerUnit || !CachedSkillData)
    {
        return Result;
    }

    ACombatGridTile* CenterTile = ResolveCenterTile();

    if (!CenterTile)
    {
        return Result;
    }

    ACombatGridManager* CombatGridManager = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatGridManager::StaticClass()));

    if (!CombatGridManager)
    {
        return Result;
    }

    TArray<ACombatGridTile*> AreaTiles;

    if (CachedSkillData->AreaType == ESkillAreaType::Single)
    {
        AreaTiles.Add(CenterTile);
    }
    else
    {
        AreaTiles = CombatGridManager->GetTilesInChebyshevRange(CenterTile, CachedSkillData->AreaRadius);
    }

    TArray<AUnitBase*> RawUnits = UCombatTargetingLibrary::CollectUniqueAliveUnitsFromTiles(AreaTiles, CachedOwnerUnit);

    for (AUnitBase* TargetUnit : RawUnits)
    {
        if (!IsValidAreaTargetUnit(TargetUnit))
        {
            continue;
        }

        Result.Add(TargetUnit);
    }

    return Result;
}

bool UGA_AreaAttack::IsValidAreaTargetUnit(AUnitBase* TargetUnit) const
{
    if (!CachedOwnerUnit || !CachedSkillData || !TargetUnit)
    {
        return false;
    }

    if (!TargetUnit->IsUnitAlive())
    {
        return false;
    }

    if (CachedSkillData->TargetTeamRule == ESkillTargetTeamRule::EnemyOnly)
    {
        return CachedOwnerUnit->GetTeam() != TargetUnit->GetTeam();
    }

    if (CachedSkillData->TargetTeamRule == ESkillTargetTeamRule::AllyOnly)
    {
        return CachedOwnerUnit->GetTeam() == TargetUnit->GetTeam();
    }

    if (CachedSkillData->TargetTeamRule == ESkillTargetTeamRule::AnyUnit)
    {
        return true;
    }

    return false;
}