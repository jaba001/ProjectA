#include "Combat/Library/CombatTargetingLibrary.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Kismet/GameplayStatics.h"
#include "GAS/CombatGameplayTags.h"

bool UCombatTargetingLibrary::IsValidSkillTarget(const AUnitBase* SourceUnit, const USkillDefinitionDataAsset* SkillData, const ACombatGridTile* TargetTile)
{
    if (!IsValid(SourceUnit) || !SourceUnit->IsUnitAlive() || !IsSupportedSkillArea(SkillData) || !IsValid(TargetTile) || SourceUnit->GetWorld() != TargetTile->GetWorld())
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

    // Only melee-tagged attacks restrict initial enemy selection; effect collision remains independent.
    // 근접 태그 공격만 최초 적 선택을 제한하며 실제 효과 충돌 판정은 별도로 유지합니다.
    FCombatRoundSkill RoundSkill;
    FText Error;
    const bool bProtectedEnemy = TargetUnit
        && TargetUnit->GetTeam() != SourceTeam
        && TargetTile->GetProtectedByFront();
    if (bProtectedEnemy
        && SkillData->ResolveRoundSkill(RoundSkill, Error)
        && RoundSkill.EffectTags.HasTag(ProjectACombatTags::Skill_Attack_Melee)) return false;
    switch (SkillData->TargetRule)
    {
    case ESkillTargetRule::EnemyUnit:
    {
        return TargetUnit && TargetUnit->GetTeam() != SourceTeam;
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

bool UCombatTargetingLibrary::IsSupportedSkillArea(const USkillDefinitionDataAsset* SkillData)
{
    return IsValid(SkillData) && SkillData->AreaRadius >= 0 && (SkillData->AreaType == ESkillAreaType::Single || SkillData->AreaType == ESkillAreaType::AroundTarget || SkillData->AreaType == ESkillAreaType::AroundSelf || SkillData->AreaType == ESkillAreaType::TargetAndSides);
}

ACombatGridTile* UCombatTargetingLibrary::ResolveSkillAreaCenter(const AUnitBase* SourceUnit, const USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile)
{
    if (!IsValid(SourceUnit) || !IsSupportedSkillArea(SkillData))
    {
        return nullptr;
    }
    ACombatGridTile* Center = SkillData->AreaType == ESkillAreaType::AroundSelf ? SourceUnit->GetCurrentTile() : TargetTile;
    return IsValid(Center) && Center->GetWorld() == SourceUnit->GetWorld() ? Center : nullptr;
}

bool UCombatTargetingLibrary::IsSkillEffectTarget(const AUnitBase* SourceUnit, const USkillDefinitionDataAsset* SkillData, const AUnitBase* TargetUnit)
{
    if (!IsValid(SourceUnit) || !IsSupportedSkillArea(SkillData) || !IsValid(TargetUnit) || !TargetUnit->IsUnitAlive() || TargetUnit == SourceUnit || TargetUnit->GetWorld() != SourceUnit->GetWorld())
    {
        return false;
    }
    // Effect coverage uses team rules; front protection only restricts initial selection.
    // 효과 범위에는 진영 규칙을 적용하며 전열 보호는 최초 선택만 제한합니다.
    switch (SkillData->TargetRule)
    {
    case ESkillTargetRule::EnemyUnit:
    case ESkillTargetRule::EnemyTile:
        return SourceUnit->GetTeam() != TargetUnit->GetTeam();
    case ESkillTargetRule::AllyUnit:
    case ESkillTargetRule::AllyTile:
        return SourceUnit->GetTeam() == TargetUnit->GetTeam();
    case ESkillTargetRule::AnyUnit:
    case ESkillTargetRule::AnyTile:
        return true;
    default:
        return false;
    }
}

TArray<AUnitBase*> UCombatTargetingLibrary::ResolveSkillAreaTargets(AUnitBase* SourceUnit, const USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile)
{
    TArray<AUnitBase*> Result;
    ACombatGridTile* Center = ResolveSkillAreaCenter(SourceUnit, SkillData, TargetTile);
    if (!Center)
    {
        return Result;
    }
    TArray<ACombatGridTile*> Tiles;
    if (SkillData->AreaType == ESkillAreaType::Single)
    {
        Tiles.Add(Center);
    }
    else
    {
        ACombatGridManager* Grid = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(SourceUnit->GetWorld(), ACombatGridManager::StaticClass()));
        if (!Grid)
        {
            return Result;
        }
        if (SkillData->AreaType == ESkillAreaType::TargetAndSides)
        {
            for (int32 Offset = -1; Offset <= 1; ++Offset) Tiles.Add(Grid->GetTileAtCoord(Center->GridCoord + FIntPoint(Offset, 0)));
        }
        else
        {
            Tiles = Grid->GetTilesInChebyshevRange(Center, SkillData->AreaRadius);
        }
    }
    for (ACombatGridTile* Tile : Tiles)
    {
        if (!IsValid(Tile) || Tile->GetWorld() != SourceUnit->GetWorld())
        {
            continue;
        }
        AUnitBase* Unit = Tile->GetOccupyingUnit();
        if (IsSkillEffectTarget(SourceUnit, SkillData, Unit) && Unit->GetCurrentTile() == Tile)
        {
            Result.AddUnique(Unit);
        }
    }
    return Result;
}
