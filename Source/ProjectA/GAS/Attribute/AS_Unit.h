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
    UAS_Unit();

    // Current hit points.
    // 현재 체력입니다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HP, Category = "Attributes")
    FGameplayAttributeData HP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, HP)

    // Maximum hit points.
    // 최대 체력입니다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHP, Category = "Attributes")
    FGameplayAttributeData MaxHP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, MaxHP)

    // Remaining damage absorption is authoritative and expires at the round boundary.
    // 남은 피해 흡수량은 서버 권위로 관리하며 라운드 경계에서 만료됩니다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Attributes")
    FGameplayAttributeData Shield;
    ATTRIBUTE_ACCESSORS(UAS_Unit, Shield)

    // Base strength is stored without defining a damage formula.
    // 피해 공식을 정의하지 않고 기본 힘을 저장합니다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "Attributes")
    FGameplayAttributeData Strength;
    ATTRIBUTE_ACCESSORS(UAS_Unit, Strength)

    // Current dexterity determines round initiative at one speed point per point without changing action points.
    // 현재 민첩 1당 라운드 시작 속도 1을 사용하며 행동력은 변경하지 않습니다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Dexterity, Category = "Attributes")
    FGameplayAttributeData Dexterity;
    ATTRIBUTE_ACCESSORS(UAS_Unit, Dexterity)

    // Base intelligence is stored without defining a skill scaling formula.
    // 스킬 계수 공식을 정의하지 않고 기본 지능을 저장합니다.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intelligence, Category = "Attributes")
    FGameplayAttributeData Intelligence;
    ATTRIBUTE_ACCESSORS(UAS_Unit, Intelligence)

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Absorb incoming HP damage before GAS applies it and reject revival effects.
    // GAS의 HP 피해 적용 전에 흡수량을 차감하고 부활 효과를 거부합니다.
    virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    // Reacts after gameplay effects modify attributes.
    // 게임플레이 이펙트가 어트리뷰트를 변경한 뒤 후처리합니다.
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    // Notify GAS observers when authoritative attributes arrive.
    // 서버 어트리뷰트가 도착하면 GAS 관찰자에게 알립니다.
    UFUNCTION()
    void OnRep_HP(const FGameplayAttributeData& PreviousHP);

    UFUNCTION()
    void OnRep_MaxHP(const FGameplayAttributeData& PreviousMaxHP);

    UFUNCTION()
    void OnRep_Shield(const FGameplayAttributeData& PreviousShield);

    UFUNCTION()
    void OnRep_Strength(const FGameplayAttributeData& PreviousStrength);

    UFUNCTION()
    void OnRep_Dexterity(const FGameplayAttributeData& PreviousDexterity);

    UFUNCTION()
    void OnRep_Intelligence(const FGameplayAttributeData& PreviousIntelligence);
};
