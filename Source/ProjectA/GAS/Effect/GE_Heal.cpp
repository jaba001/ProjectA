#include "GAS/Effect/GE_Heal.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

UGE_Heal::UGE_Heal()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FInheritedTagContainer Tags;
    Tags.AddTag(ProjectACombatTags::Skill_Effect_Heal);
    UAssetTagsGameplayEffectComponent* AssetTags = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("EffectAssetTags"));
    GEComponents.Add(AssetTags);
    AssetTags->SetAndApplyAssetTagChanges(Tags);
    FSetByCallerFloat Magnitude;
    Magnitude.DataTag = ProjectACombatTags::Data_Heal;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UAS_Unit::GetHPAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = Magnitude;
    Modifiers.Add(Modifier);
}
