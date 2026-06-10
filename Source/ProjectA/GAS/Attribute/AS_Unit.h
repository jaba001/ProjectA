#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AS_Unit.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// Attribute set containing combat stats for units.
// 유닛의 전투 스탯을 담는 어트리뷰트 세트입니다.
UCLASS()
class PROJECTA_API UAS_Unit : public UAttributeSet
{
    GENERATED_BODY()

public:
    // Current hit points.
    // 현재 체력입니다.
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData HP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, HP)

    // Maximum hit points.
    // 최대 체력입니다.
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData MaxHP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, MaxHP)

public:
    // Reacts after gameplay effects modify attributes.
    // 게임플레이 이펙트가 어트리뷰트를 변경한 뒤 후처리합니다.
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
