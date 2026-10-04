#pragma once

#include "Combat/Round/CombatRoundTypes.h"
#include "GAS/CombatGameplayTags.h"

namespace CombatChainTests
{
    // Explicit disposable inputs exercise contracts without selecting production skill balance.
    // 명시적인 일회성 입력으로 실전 스킬의 밸런스를 결정하지 않고 계약을 검증합니다.
    inline FCombatRoundSkill Skill()
    {
        FCombatRoundSkill Result;
        Result.SkillId = TEXT("ChainContractFixture");
        Result.Kind = ECombatRoundSkillKind::Melee;
        Result.Approach = ECombatRoundApproach::None;
        Result.bUseEffectCollision = true;
        Result.bTargetOnly = true;
        Result.EffectOffset = FVector(300.f, 0.f, 0.f);
        Result.EffectHalfExtent = FVector(400.f, 150.f, 100.f);
        Result.EffectHitDelaySeconds = 0.125f;
        Result.EffectDuration = 0.375f;
        Result.WindupSeconds = 0.f;
        Result.HitRange = 1000.f;
        Result.Power = 20.f;
        Result.Chain.MaxTargets = 3;
        Result.Chain.JumpDistance = 400.f;
        Result.Chain.JumpIntervalSeconds = 0.125f;
        Result.Chain.DamageMultiplierPerJump = 0.5f;
        Result.EffectTags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
        Result.EffectTags.AddTag(ProjectACombatTags::Skill_Shape_Chain);
        return Result;
    }
}
