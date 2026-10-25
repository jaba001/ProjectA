#include "GAS/CombatGameplayTags.h"

namespace ProjectACombatTags
{
    UE_DEFINE_GAMEPLAY_TAG(Skill_Effect_Damage, "Skill.Effect.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Effect_Heal, "Skill.Effect.Heal");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Effect_Shield, "Skill.Effect.Shield");
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Data_Heal, "Data.Heal");
    UE_DEFINE_GAMEPLAY_TAG(Data_Shield, "Data.Shield");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Element_Physical, "Skill.Element.Physical");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Element_Fire, "Skill.Element.Fire");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Element_Cold, "Skill.Element.Cold");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Element_Lightning, "Skill.Element.Lightning");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Element_Chaos, "Skill.Element.Chaos");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Shape_Slash, "Skill.Shape.Slash");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Shape_Projectile, "Skill.Shape.Projectile");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Shape_Area, "Skill.Shape.Area");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Shape_Beam, "Skill.Shape.Beam");
    UE_DEFINE_GAMEPLAY_TAG(Skill_Shape_Chain, "Skill.Shape.Chain");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attack_Melee, "Skill.Attack.Melee", "Direct damage adds the caster's Strength");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Attack_Magic, "Skill.Attack.Magic", "Direct damage adds the caster's Intelligence");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_AI_Fallback, "Skill.AI.Fallback", "AI uses this skill only when no other equipped skill has a legal command / 다른 장착 스킬의 유효한 행동이 없을 때만 AI가 사용");
    UE_DEFINE_GAMEPLAY_TAG(Event_Attack_Release, "Event.Attack.Release");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Burning, "Status.Burning", "Damage equal to the value at round end, then decays");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Poisoned, "Status.Poisoned", "Damage equal to the value at round end, then minus one");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Bleeding, "Status.Bleeding", "Damage equal to the value per tactical tile moved");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Chilled, "Status.Chilled", "Lowers combat speed from the next planning");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Frozen, "Status.Frozen", "Blocks the current waiting action or the next round");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Stunned, "Status.Stunned", "Cannot act");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Confused, "Status.Confused", "Replaces the next round plan with a random legal action");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Shocked, "Status.Shocked", "Discharges to orthogonal allies on the next direct hit");
}
