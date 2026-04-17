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

UCLASS()
class PROJECTA_API UAS_Unit : public UAttributeSet
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData HP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, HP)

        UPROPERTY(BlueprintReadOnly, Category = "Attributes")
    FGameplayAttributeData MaxHP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, MaxHP)

public:
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};