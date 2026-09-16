#include "Combat/Round/CombatRoundTypes.h"

float CombatRoundRules::StartDelay(int32 HighestSpeed, int32 UnitSpeed)
{
    return static_cast<float>(FMath::Max(static_cast<int64>(HighestSpeed) - UnitSpeed, static_cast<int64>(0))) * 0.1f;
}

bool CombatRoundRules::IsTerminal(ECombatRoundActionPhase Phase)
{
    return Phase == ECombatRoundActionPhase::Complete || Phase == ECombatRoundActionPhase::Cancelled;
}

bool CombatRoundRules::IsOwnTerritory(bool bEnemy, FIntPoint Coord)
{
    if (Coord.X < 0 || Coord.X >= 4 || Coord.Y < 0 || Coord.Y >= 4) return false;
    if (bEnemy) return Coord.Y >= 2;
    return Coord.Y < 2;
}

bool CombatRoundRules::IsValidSkill(const FCombatRoundSkill& Skill)
{
    if (Skill.SkillId.IsNone() || Skill.Kind > ECombatRoundSkillKind::Wait || Skill.Approach > ECombatRoundApproach::Tile || Skill.TargetLoss > ECombatRoundTargetLoss::NearestEnemy) return false;
    if (!FMath::IsFinite(Skill.WindupSeconds) || Skill.WindupSeconds < 0.f || Skill.WindupSeconds > 60.f) return false;
    if (!FMath::IsFinite(Skill.Power) || Skill.Power < 0.f || Skill.Power > 1000000.f) return false;
    if (!FMath::IsFinite(Skill.HitRange) || Skill.HitRange <= 0.f || Skill.HitRange > 100000.f) return false;
    if (!FMath::IsFinite(Skill.MeleeRadius) || Skill.MeleeRadius <= 0.f || Skill.MeleeRadius > 1000.f) return false;
    if (!FMath::IsFinite(Skill.MoveSpeed) || Skill.MoveSpeed <= 0.f || Skill.MoveSpeed > 100000.f) return false;
    if (!FMath::IsFinite(Skill.ProjectileSpeed) || Skill.ProjectileSpeed <= 0.f || Skill.ProjectileSpeed > 100000.f) return false;
    if (!FMath::IsFinite(Skill.ProjectileRadius) || Skill.ProjectileRadius <= 0.f || Skill.ProjectileRadius > 1000.f) return false;
    if (!FMath::IsFinite(Skill.ProjectileLifetime) || Skill.ProjectileLifetime <= 0.f || Skill.ProjectileLifetime > 60.f) return false;
    if (Skill.ActionPointCost < 0 || Skill.ActionPointCost > 100 || Skill.SubActionPointCost < 0 || Skill.SubActionPointCost > 100) return false;
    if (Skill.bRemainAtDestination && Skill.Approach != ECombatRoundApproach::Tile) return false;
    if (Skill.Kind == ECombatRoundSkillKind::Guard && Skill.Approach != ECombatRoundApproach::None) return false;
    if (Skill.Kind == ECombatRoundSkillKind::Wait && Skill.Approach != ECombatRoundApproach::None) return false;
    if (Skill.Kind == ECombatRoundSkillKind::GroundAttack && Skill.Approach == ECombatRoundApproach::Unit) return false;
    return true;
}
