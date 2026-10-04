#include "Combat/Round/CombatConsumableRules.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "Game/Run/RunRecoveryTypes.h"
#include "Unit/UnitBase.h"

namespace
{
    int32 FindStack(const AUnitBase* Source, const FCombatRoundSkill& Skill)
    {
        if (!IsValid(Source)) return INDEX_NONE;
        return Source->Consumables.IndexOfByPredicate([&Skill](const FRunConsumableStack& Stack)
        {
            FCombatRoundSkill Resolved;
            FText Error;
            return Skill.EffectTags.HasTagExact(Stack.ItemTag) && RunRecoveryRules::ResolveStack(Stack, Resolved, Error) && FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Resolved, &Skill, 0);
        });
    }
}

bool CombatConsumableRules::CanUse(const AUnitBase* Source, const AUnitBase* Target, const FCombatRoundSkill& Skill, bool bHumanControlled)
{
    if (!bHumanControlled || Source != Target || !RunRecoveryRules::IsConsumable(Skill) || !CombatSkillExecution::CanUseSkill(Source, Skill) || !CombatSkillExecution::IsValidEffectTarget(Source, Target, Skill) || !Source->GetAttributeSet()) return false;
    const float HP = Source->GetAttributeSet()->GetHP();
    const float MaxHP = Source->GetAttributeSet()->GetMaxHP();
    if (!FMath::IsFinite(HP) || !FMath::IsFinite(MaxHP) || HP <= 0.f || HP >= MaxHP) return false;
    const int32 Index = FindStack(Source, Skill);
    return Source->Consumables.IsValidIndex(Index) && Source->Consumables[Index].Quantity > 0;
}

bool CombatConsumableRules::Release(AUnitBase* Source, AUnitBase* Target, const FCombatRoundSkill& Skill, bool bHumanControlled)
{
    if (!IsValid(Source) || !Source->HasAuthority() || !CanUse(Source, Target, Skill, bHumanControlled)) return false;
    const int32 Index = FindStack(Source, Skill);
    // The round marks release once; quantity changes only after the official Instant GAS effect succeeds.
    // 라운드는 발동을 한 번만 기록하며 수량은 공식 Instant GAS 효과가 성공한 뒤에만 차감합니다.
    if (!CombatSkillExecution::ApplyEffect(Source, Target, Skill)) return false;
    --Source->Consumables[Index].Quantity;
    Source->ForceNetUpdate();
    return true;
}
