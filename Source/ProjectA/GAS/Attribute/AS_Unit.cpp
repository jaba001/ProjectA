#include "GAS/Attribute/AS_Unit.h"
#include "Unit/UnitBase.h"

UAS_Unit::UAS_Unit()
{
    // 초기값은 나중에 GameplayEffect로 설정
}

void UAS_Unit::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    if (HP.GetCurrentValue() <= 0.f)
    {
        AUnitBase* Unit = Cast<AUnitBase>(GetOwningActor());

        if (Unit)
        {
            Unit->Die();
        }
    }
}