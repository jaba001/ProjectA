#include "GAS/Effect/GE_Damage.h"
#include "GAS/Attribute/AS_Unit.h"

UGE_Damage::UGE_Damage()
{
    // 즉시 적용되는 Effect
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo DamageModifier;
    DamageModifier.Attribute = UAS_Unit::GetHPAttribute(); // 깎을 대상
    DamageModifier.ModifierOp = EGameplayModOp::Additive;  // 더하기 연산
    DamageModifier.ModifierMagnitude = FScalableFloat(-10.f); // HP -10

    Modifiers.Add(DamageModifier);
}
