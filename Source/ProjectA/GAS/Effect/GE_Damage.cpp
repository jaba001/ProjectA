#include "GAS/Effect/GE_Damage.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayTagContainer.h"

UGE_Damage::UGE_Damage()
{
    // Preserve the instantaneous negative Data.Damage contract and expose its effect tag.
    // 즉시 적용하는 음수 Data.Damage 계약을 유지하며 효과 태그를 제공합니다.
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FInheritedTagContainer Tags;
    Tags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
    UAssetTagsGameplayEffectComponent* AssetTags = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("EffectAssetTags"));
    GEComponents.Add(AssetTags);
    AssetTags->SetAndApplyAssetTagChanges(Tags);

    // Shield absorption runs in the attribute set before this HP modifier executes.
    // 이 HP 수정자가 실행되기 전에 어트리뷰트 세트에서 보호막을 흡수합니다.
    FGameplayModifierInfo DamageModifier;
    DamageModifier.Attribute = UAS_Unit::GetHPAttribute();
    DamageModifier.ModifierOp = EGameplayModOp::Additive;

    FSetByCallerFloat SetByCallerDamage;
    SetByCallerDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

    DamageModifier.ModifierMagnitude = SetByCallerDamage;

    Modifiers.Add(DamageModifier);
}
