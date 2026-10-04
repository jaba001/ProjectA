#include "Game/Run/RunRecoveryTypes.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "GAS/CombatGameplayTags.h"
#include "GAS/Effect/GE_Heal.h"
#include "NativeGameplayTags.h"
#include "Unit/UnitDataRules.h"

namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(ConsumableTag, "Item.Consumable");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(HealingItemTag, "Item.Consumable.Healing");
}

FGameplayTag RunRecoveryRules::GetConsumableTag() { return ConsumableTag; }
FGameplayTag RunRecoveryRules::GetHealingItemTag() { return HealingItemTag; }
FSoftObjectPath RunRecoveryRules::GetHealingSkillPath() { return FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Consumables/DA_HealthPotion.DA_HealthPotion")); }
bool RunRecoveryRules::IsConsumable(const FCombatRoundSkill& Skill) { return Skill.EffectTags.HasTag(GetConsumableTag()); }

bool RunRecoveryRules::ResolveStack(const FRunConsumableStack& Stack, FCombatRoundSkill& OutSkill, FText& OutError)
{
    OutError = FText::FromString(TEXT("소모품 태그, 수량 또는 회복 스킬이 올바르지 않습니다."));
    if (!Stack.ItemTag.IsValid() || !Stack.ItemTag.MatchesTag(GetConsumableTag()) || Stack.Quantity < 0 || Stack.Quantity > MaximumQuantity || !Stack.Skill.IsValid() || !Stack.Skill.GetAssetPathString().StartsWith(TEXT("/Game/User_JeHoon/"))) return false;
    const USkillDefinitionDataAsset* Definition = Cast<USkillDefinitionDataAsset>(Stack.Skill.TryLoad());
    if (!Definition || !Definition->ResolveRoundSkill(OutSkill, OutError)) return false;
    return IsConsumable(OutSkill) && OutSkill.EffectTags.HasTagExact(Stack.ItemTag) && OutSkill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal) && !OutSkill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Damage) && !OutSkill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield) && OutSkill.TargetRule == ESkillTargetRule::AllyUnit && OutSkill.Kind == ECombatRoundSkillKind::Melee && OutSkill.Approach == ECombatRoundApproach::None && !OutSkill.bUseEffectCollision && !OutSkill.bUseWeaponTrace && !OutSkill.bUseMeleeAreaCollision && OutSkill.Chain.MaxTargets == 1 && OutSkill.Power > 0.f && OutSkill.ActionPointCost > 0 && OutSkill.EffectClass == UGE_Heal::StaticClass();
}

bool RunRecoveryRules::ValidateStacks(const TArray<FRunConsumableStack>& Stacks, FText& OutError)
{
    if (Stacks.Num() > UnitDataRules::MaxSkills) return false;
    TSet<FGameplayTag> Tags;
    TSet<FName> Skills;
    for (const FRunConsumableStack& Stack : Stacks)
    {
        FCombatRoundSkill Skill;
        if (!ResolveStack(Stack, Skill, OutError) || Tags.Contains(Stack.ItemTag) || Skills.Contains(Skill.SkillId)) return false;
        Tags.Add(Stack.ItemTag);
        Skills.Add(Skill.SkillId);
    }
    return true;
}

bool RunRecoveryRules::ValidateState(const FRunRecoveryState& State, FText& OutError)
{
    if (State.SchemaVersion != 1 || State.Revision < 0 || State.StartingQuantity < 0 || State.StartingQuantity > MaximumQuantity || State.ConsumablePrice <= 0 || State.RecoveryPrice <= 0 || State.RevivalPrice <= 0 || !FMath::IsFinite(State.RecoveryHP) || State.RecoveryHP <= 0.f || State.RecoveryHP > UnitDataRules::MaxStatValue || !FMath::IsFinite(State.RevivalFraction) || State.RevivalFraction <= 0.f || State.RevivalFraction > 1.f) return false;
    FRunConsumableStack Stack;
    Stack.ItemTag = State.ConsumableTag;
    Stack.Skill = State.HealingSkill;
    Stack.Quantity = State.StartingQuantity;
    FCombatRoundSkill Skill;
    return ResolveStack(Stack, Skill, OutError);
}
