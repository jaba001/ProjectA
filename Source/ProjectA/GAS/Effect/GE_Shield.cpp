#include "GAS/Effect/GE_Shield.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UGE_Shield::UGE_Shield()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FInheritedTagContainer Tags;
    Tags.AddTag(ProjectACombatTags::Skill_Effect_Shield);
    UAssetTagsGameplayEffectComponent* AssetTags = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("EffectAssetTags"));
    GEComponents.Add(AssetTags);
    AssetTags->SetAndApplyAssetTagChanges(Tags);
    FSetByCallerFloat Magnitude;
    Magnitude.DataTag = ProjectACombatTags::Data_Shield;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UAS_Unit::GetShieldAttribute();
    // Each cast adds its power to the remaining shield, including after damage absorption.
    // 매 시전은 피해 흡수 후의 잔여 보호막에도 새 보호막 수치를 합산합니다.
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = Magnitude;
    Modifiers.Add(Modifier);
}
