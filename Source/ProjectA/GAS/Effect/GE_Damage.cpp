#include "GAS/Effect/GE_Damage.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

UGE_Damage::UGE_Damage()
{
    // 데미지 적용은 즉시 1회 처리한다.
    DurationPolicy = EGameplayEffectDurationType::Instant;

    // HP를 직접 감소시키는 Modifier를 추가한다.
    FGameplayModifierInfo DamageModifier;
    DamageModifier.Attribute = UAS_Unit::GetHPAttribute();
    DamageModifier.ModifierOp = EGameplayModOp::Additive;

    // SetByCaller 구조체를 만든 뒤 DataTag를 지정한다.
    FSetByCallerFloat SetByCallerDamage;
    SetByCallerDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

    // ModifierMagnitude에 SetByCaller 정보를 넣는다.
    DamageModifier.ModifierMagnitude = SetByCallerDamage;

    Modifiers.Add(DamageModifier);
}