#include "Combat/Library/CombatTargetingLibrary.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

bool UCombatTargetingLibrary::IsValidSkillTarget(const AUnitBase* SourceUnit, const USkillDefinitionDataAsset* SkillData, const ACombatGridTile* TargetTile)
{
    if (!IsValid(SourceUnit) || !SourceUnit->IsUnitAlive() || !IsValid(SkillData) || !IsValid(TargetTile) || SourceUnit->GetWorld() != TargetTile->GetWorld())
    {
        return false;
    }

    const AUnitBase* TargetUnit = TargetTile->GetOccupyingUnit();
    if (TargetUnit && (!IsValid(TargetUnit) || !TargetUnit->IsUnitAlive() || TargetUnit->GetWorld() != SourceUnit->GetWorld() || TargetUnit->GetCurrentTile() != TargetTile))
    {
        return false;
    }

    // Approach skills need another living unit; stationary ally skills may target the caster.
    // 접근 스킬에는 다른 생존 유닛이 필요하며 제자리 아군 스킬은 시전자도 선택할 수 있습니다.
    if (SkillData->bMoveToTarget && (!TargetUnit || TargetUnit == SourceUnit))
    {
        return false;
    }

    const ETeam SourceTeam = SourceUnit->GetTeam();
    const ETileTerritory Territory = TargetTile->GetTerritory();
    const bool bEnemyTerritory = (SourceTeam == ETeam::Player && Territory == ETileTerritory::Enemy) || (SourceTeam == ETeam::Enemy && Territory == ETileTerritory::Player);
    const bool bAllyTerritory = (SourceTeam == ETeam::Player && Territory == ETileTerritory::Player) || (SourceTeam == ETeam::Enemy && Territory == ETileTerritory::Enemy);

    // Preserve the existing player rule: front protection applies to EnemyUnit selection only.
    // 기존 플레이어 규칙대로 전열 보호는 EnemyUnit 대상 선택에만 적용합니다.
    switch (SkillData->TargetRule)
    {
    case ESkillTargetRule::EnemyUnit:
    {
        return TargetUnit && TargetUnit->GetTeam() != SourceTeam && (SkillData->bIgnoreFront || !TargetTile->GetProtectedByFront());
    }
    case ESkillTargetRule::AllyUnit:
    {
        return TargetUnit && TargetUnit->GetTeam() == SourceTeam;
    }
    case ESkillTargetRule::AnyUnit:
    {
        return TargetUnit != nullptr;
    }
    case ESkillTargetRule::EnemyTile:
    {
        return bEnemyTerritory;
    }
    case ESkillTargetRule::AllyTile:
    {
        return bAllyTerritory;
    }
    case ESkillTargetRule::AnyTile:
    {
        return Territory == ETileTerritory::Player || Territory == ETileTerritory::Enemy;
    }
    default:
    {
        return false;
    }
    }
}

TArray<AUnitBase*> UCombatTargetingLibrary::CollectUniqueAliveUnitsFromTiles(const TArray<ACombatGridTile*>& TargetTiles, AUnitBase* SourceUnit)
{
    TArray<AUnitBase*> ResultUnits;
    TSet<AUnitBase*> UniqueUnits;

    for (ACombatGridTile* TargetTile : TargetTiles)
    {
        if (!TargetTile)
        {
            continue;
        }

        AUnitBase* OccupyingUnit = TargetTile->GetOccupyingUnit();

        if (!OccupyingUnit)
        {
            continue;
        }

        if (OccupyingUnit == SourceUnit)
        {
            continue;
        }

        if (!OccupyingUnit->IsUnitAlive())
        {
            continue;
        }

        if (UniqueUnits.Contains(OccupyingUnit))
        {
            continue;
        }

        UniqueUnits.Add(OccupyingUnit);
        ResultUnits.Add(OccupyingUnit);
    }

    return ResultUnits;
}
