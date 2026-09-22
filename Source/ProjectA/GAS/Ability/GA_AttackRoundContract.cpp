#include "GAS/Ability/GA_AttackBase.h"
#include "Combat/Round/CombatRoundTypes.h"

void UGA_AttackBase::ExportRoundEffectContract(FCombatRoundSkill& Skill) const
{
    if (!Skill.EffectClass) Skill.EffectClass = DamageEffectClass;
    Skill.EffectTags.AppendTags(GetAssetTags());
    Skill.SourceRequiredTags.AppendTags(ActivationRequiredTags);
    Skill.SourceRequiredTags.AppendTags(SourceRequiredTags);
    Skill.SourceBlockedTags.AppendTags(ActivationBlockedTags);
    Skill.SourceBlockedTags.AppendTags(SourceBlockedTags);
    Skill.TargetRequiredTags.AppendTags(TargetRequiredTags);
    Skill.TargetBlockedTags.AppendTags(TargetBlockedTags);
}
