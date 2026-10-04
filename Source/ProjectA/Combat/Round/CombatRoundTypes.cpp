#include "Combat/Round/CombatRoundTypes.h"
#include "GAS/CombatGameplayTags.h"
#include "GameplayEffect.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

namespace
{
    bool IsValidChainSettings(const FCombatChainSettings& Chain)
    {
        if (Chain.MaxTargets < 1 || Chain.MaxTargets > 32) return false;
        if (!FMath::IsFinite(Chain.JumpDistance) || Chain.JumpDistance < 0.f || Chain.JumpDistance > 100000.f || (Chain.MaxTargets > 1 && Chain.JumpDistance <= 0.f)) return false;
        if (!FMath::IsFinite(Chain.JumpIntervalSeconds) || Chain.JumpIntervalSeconds < 0.f || Chain.JumpIntervalSeconds > 10.f) return false;
        return FMath::IsFinite(Chain.DamageMultiplierPerJump) && Chain.DamageMultiplierPerJump >= 0.f && Chain.DamageMultiplierPerJump <= 1.f;
    }

    template<typename TValue>
    void SerializeVisualParameters(FArchive& Ar, TMap<FName, TValue>& Parameters)
    {
        if (Ar.IsSaving() && Parameters.Num() > 64)
        {
            Ar.SetError();
            return;
        }
        uint8 Count = static_cast<uint8>(Parameters.Num());
        Ar << Count;
        if (Count > 64)
        {
            Ar.SetError();
            return;
        }
        TArray<FName> Names;
        if (Ar.IsSaving())
        {
            Parameters.GetKeys(Names);
            Names.Sort(FNameLexicalLess());
        }
        else Parameters.Reset();
        for (uint8 Index = 0; Index < Count && !Ar.IsError(); ++Index)
        {
            FName Name = Ar.IsSaving() ? Names[Index] : NAME_None;
            TValue Value = Ar.IsSaving() ? Parameters.FindChecked(Name) : TValue{};
            Ar << Name;
            Ar << Value;
            if (Name.IsNone() || (Ar.IsLoading() && Parameters.Contains(Name))) Ar.SetError();
            else if (Ar.IsLoading()) Parameters.Add(Name, Value);
        }
    }
}

bool FCombatSkillVfx::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    Ar << Niagara;
    Ar << Cascade;
    Ar << RelativeTransform;
    Ar << Sound;
    Ar << SoundVolume;
    Ar << SoundPitch;
    Ar << SoundMaxDuration;
    Ar << StartPositionParameter;
    Ar << StartPositionSpace;
    Ar << StartPositionOffset;
    Ar << EndPositionParameter;
    Ar << EndPositionSpace;
    SerializeVisualParameters(Ar, BoolParameters);
    SerializeVisualParameters(Ar, FloatParameters);
    if (StartPositionSpace > ECombatVfxEndpointSpace::World || EndPositionSpace > ECombatVfxEndpointSpace::World) Ar.SetError();
    bOutSuccess = !Ar.IsError();
    return true;
}

bool FCombatChainSettings::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    Ar << MaxTargets;
    Ar << JumpDistance;
    Ar << JumpIntervalSeconds;
    Ar << DamageMultiplierPerJump;
    if (!IsValidChainSettings(*this)) Ar.SetError();
    bOutSuccess = !Ar.IsError();
    return true;
}

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

bool CombatRoundRules::IsSupportedEffectDuration(const FCombatRoundSkill& Skill)
{
    if (!Skill.EffectClass) return true;
    const UGameplayEffect* Effect = Skill.EffectClass->GetDefaultObject<UGameplayEffect>();
    return Effect && Effect->DurationPolicy == EGameplayEffectDurationType::Instant;
}

bool CombatRoundRules::UsesUnitTarget(const FCombatRoundSkill& Skill)
{
    if (Skill.Kind == ECombatRoundSkillKind::Wait) return false;
    if (Skill.Kind != ECombatRoundSkillKind::GroundAttack) return true;
    return Skill.bUseEffectCollision && Skill.TargetRule <= ESkillTargetRule::AnyUnit;
}

bool CombatRoundRules::MatchesTargetTeam(const FCombatRoundSkill& Skill, bool bSourceEnemy, bool bTargetEnemy)
{
    if (Skill.TargetRule == ESkillTargetRule::AllyUnit || Skill.TargetRule == ESkillTargetRule::AllyTile) return bSourceEnemy == bTargetEnemy;
    if (Skill.TargetRule == ESkillTargetRule::AnyUnit || Skill.TargetRule == ESkillTargetRule::AnyTile) return true;
    return bSourceEnemy != bTargetEnemy;
}

bool CombatRoundRules::UsesChain(const FCombatRoundSkill& Skill)
{
    static const FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchTag(ProjectACombatTags::Skill_Shape_Chain);
    return Query.Matches(Skill.EffectTags);
}

bool CombatRoundRules::IsValidSkill(const FCombatRoundSkill& Skill)
{
    if (Skill.EffectClass && Skill.EffectClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) return false;
    if (!IsSupportedEffectDuration(Skill)) return false;
    if (Skill.SkillId.IsNone() || Skill.Kind > ECombatRoundSkillKind::Wait || Skill.Approach > ECombatRoundApproach::Tile || Skill.TargetLoss > ECombatRoundTargetLoss::NearestEnemy) return false;
    if (static_cast<uint8>(Skill.Kind) == 3) return false;
    if (Skill.TargetRule > ESkillTargetRule::AnyTile) return false;
    if (!IsValidChainSettings(Skill.Chain)) return false;
    if (Skill.Chain.MaxTargets > 1)
    {
        // Tags select chain execution; the fixed kind only restricts supported collision geometry.
        // 태그가 체인 실행을 선택하며 고정 종류는 지원하는 충돌 지오메트리만 제한합니다.
        if (!UsesChain(Skill) || !Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Damage) || Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal) || Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield)) return false;
        if (Skill.TargetRule != ESkillTargetRule::EnemyUnit || Skill.Kind != ECombatRoundSkillKind::Melee || !Skill.bUseEffectCollision || !Skill.bTargetOnly || !Skill.EffectTravel.IsZero()) return false;
        if (Skill.bUseWeaponTrace || Skill.bUseMeleeAreaCollision || Skill.MeleeArea != ESkillAreaType::Single) return false;
    }
    const auto IsValidVfx = [](const FCombatSkillVfx& Vfx)
    {
        if ((!Vfx.Niagara.IsNull() && !Vfx.Cascade.IsNull()) || Vfx.RelativeTransform.ContainsNaN() || !Vfx.RelativeTransform.GetRotation().IsNormalized() || Vfx.RelativeTransform.GetScale3D().GetMin() <= 0.0) return false;
        if (!FMath::IsFinite(Vfx.SoundVolume) || Vfx.SoundVolume < 0.f || Vfx.SoundVolume > 10.f || !FMath::IsFinite(Vfx.SoundPitch) || Vfx.SoundPitch < 0.125f || Vfx.SoundPitch > 4.f || !FMath::IsFinite(Vfx.SoundMaxDuration) || Vfx.SoundMaxDuration < 0.01f || Vfx.SoundMaxDuration > 60.f) return false;
        if (Vfx.StartPositionOffset.ContainsNaN() || Vfx.StartPositionOffset.GetAbsMax() > 100000.f || Vfx.BoolParameters.Num() > 64 || Vfx.FloatParameters.Num() > 64) return false;
        if (Vfx.StartPositionSpace > ECombatVfxEndpointSpace::World || Vfx.EndPositionSpace > ECombatVfxEndpointSpace::World) return false;
        if (Vfx.Niagara.IsNull() && (!Vfx.StartPositionParameter.IsNone() || !Vfx.EndPositionParameter.IsNone() || !Vfx.BoolParameters.IsEmpty() || !Vfx.FloatParameters.IsEmpty())) return false;
        if (!Vfx.StartPositionParameter.IsNone() && Vfx.StartPositionParameter == Vfx.EndPositionParameter) return false;
        for (const TPair<FName, bool>& Parameter : Vfx.BoolParameters)
        {
            if (Parameter.Key.IsNone() || Vfx.FloatParameters.Contains(Parameter.Key) || Parameter.Key == Vfx.StartPositionParameter || Parameter.Key == Vfx.EndPositionParameter) return false;
        }
        for (const TPair<FName, float>& Parameter : Vfx.FloatParameters)
        {
            if (Parameter.Key.IsNone() || !FMath::IsFinite(Parameter.Value) || Parameter.Key == Vfx.StartPositionParameter || Parameter.Key == Vfx.EndPositionParameter) return false;
        }
        return true;
    };
    if (!IsValidVfx(Skill.Vfx) || !IsValidVfx(Skill.ImpactVfx)) return false;
    if (!FMath::IsFinite(Skill.EffectHitDelaySeconds) || Skill.EffectHitDelaySeconds < 0.f || Skill.EffectHitDelaySeconds > 10.f) return false;
    if (Skill.bUseEffectCollision)
    {
        if (Skill.Kind != ECombatRoundSkillKind::Melee && Skill.Kind != ECombatRoundSkillKind::GroundAttack) return false;
        if (Skill.bUseWeaponTrace || Skill.bUseMeleeAreaCollision || Skill.MeleeArea != ESkillAreaType::Single) return false;
        if (Skill.EffectHalfExtent.ContainsNaN() || Skill.EffectHalfExtent.GetMin() <= 0.0 || Skill.EffectHalfExtent.GetMax() > 10000.0) return false;
        if (Skill.EffectOffset.ContainsNaN() || Skill.EffectTravel.ContainsNaN() || Skill.EffectOffset.GetAbsMax() > 10000.0 || Skill.EffectTravel.GetAbsMax() > 10000.0) return false;
        if (!FMath::IsFinite(Skill.EffectDuration) || Skill.EffectDuration <= 0.f || Skill.EffectDuration > 10.f) return false;
    }
    if (Skill.MeleeArea != ESkillAreaType::Single && Skill.MeleeArea != ESkillAreaType::TargetAndSides) return false;
    if (Skill.MeleeArea == ESkillAreaType::TargetAndSides && (Skill.Kind != ECombatRoundSkillKind::Melee || Skill.Approach != ECombatRoundApproach::Unit)) return false;
    if (Skill.bUseMeleeAreaCollision && (Skill.Kind != ECombatRoundSkillKind::Melee || Skill.MeleeArea != ESkillAreaType::Single)) return false;
    if (Skill.MeleeAreaHalfExtent.ContainsNaN() || Skill.MeleeAreaHalfExtent.GetMin() <= 0.0 || Skill.MeleeAreaHalfExtent.GetMax() > 1000.0) return false;
    if (!FMath::IsFinite(Skill.WindupSeconds) || Skill.WindupSeconds < 0.f || Skill.WindupSeconds > 60.f) return false;
    if (Skill.bUseWeaponTrace)
    {
        if (Skill.Kind != ECombatRoundSkillKind::Melee || Skill.MeleeArea != ESkillAreaType::Single || Skill.bUseMeleeAreaCollision || !Skill.CastMontage) return false;
        if (Skill.WeaponComponentName.IsNone() || Skill.WeaponBaseSocket.IsNone() || Skill.WeaponTipSocket.IsNone() || Skill.WeaponBaseSocket == Skill.WeaponTipSocket || Skill.WeaponMontageSlot.IsNone()) return false;
        if (!FMath::IsFinite(Skill.WeaponTraceDuration) || Skill.WeaponTraceDuration <= 0.f || Skill.WeaponTraceDuration > 5.f || Skill.WindupSeconds + Skill.WeaponTraceDuration > 60.f) return false;
        if (!FMath::IsFinite(Skill.WeaponTraceRadius) || Skill.WeaponTraceRadius <= 0.f || Skill.WeaponTraceRadius > 100.f) return false;
    }
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
