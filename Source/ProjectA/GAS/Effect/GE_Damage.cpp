#include "GAS/Effect/GE_Damage.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

UGE_Damage::UGE_Damage()
{
    // Apply damage immediately as a one-time effect
    DurationPolicy = EGameplayEffectDurationType::Instant;

    // Add a modifier that directly reduces HP
    FGameplayModifierInfo DamageModifier;
    DamageModifier.Attribute = UAS_Unit::GetHPAttribute();
    DamageModifier.ModifierOp = EGameplayModOp::Additive;

    // Create a SetByCaller struct and assign the data tag
    FSetByCallerFloat SetByCallerDamage;
    SetByCallerDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

    // Assign the SetByCaller data to ModifierMagnitude
    DamageModifier.ModifierMagnitude = SetByCallerDamage;

    Modifiers.Add(DamageModifier);
}