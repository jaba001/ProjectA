#include "Combat/Round/CombatRoundTypes.h"

float CombatRoundRules::StartDelay(float HighestSpeed, float UnitSpeed)
{
    if (!FMath::IsFinite(HighestSpeed) || !FMath::IsFinite(UnitSpeed)) return 0.0f;
    return static_cast<float>(FMath::Max(static_cast<double>(HighestSpeed) - UnitSpeed, 0.0) * 0.1);
}

float CombatRoundRules::AttackMoveSpeed(const FCombatRoundSkill& Skill, float RoundSpeed)
{
    if (Skill.Kind != ECombatRoundSkillKind::Melee) return Skill.MoveSpeed;
    // Initial tuning halves the default rate at speed 10 and preserves movement at zero speed.
    // 초기 조정값은 속도 10에서 기본 이동을 절반으로 낮추고 속도 0에서도 이동을 유지합니다.
    const double Speed = FMath::IsFinite(RoundSpeed) ? FMath::Max(0.0, static_cast<double>(RoundSpeed)) : 0.0;
    return static_cast<float>(FMath::Min(static_cast<double>(Skill.MoveSpeed) * (0.25 + Speed / 40.0), 100000.0));
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
    if (static_cast<uint8>(Skill.Kind) == 3) return false;
    if (Skill.MeleeArea != ESkillAreaType::Single && Skill.MeleeArea != ESkillAreaType::TargetAndSides) return false;
    if (Skill.MeleeArea == ESkillAreaType::TargetAndSides && (Skill.Kind != ECombatRoundSkillKind::Melee || Skill.Approach != ECombatRoundApproach::Unit)) return false;
    if (Skill.bUseMeleeAreaCollision && (Skill.Kind != ECombatRoundSkillKind::Melee || Skill.MeleeArea != ESkillAreaType::Single)) return false;
    if (Skill.MeleeAreaHalfExtent.ContainsNaN() || Skill.MeleeAreaHalfExtent.GetMin() <= 0.0 || Skill.MeleeAreaHalfExtent.GetMax() > 1000.0) return false;
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
    if (Skill.Kind == ECombatRoundSkillKind::Wait && Skill.Approach != ECombatRoundApproach::None) return false;
    if (Skill.Kind == ECombatRoundSkillKind::GroundAttack && Skill.Approach == ECombatRoundApproach::Unit) return false;
    return true;
}
