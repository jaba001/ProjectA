#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AS_Unit.generated.h"

// ============================================================================
// ATTRIBUTE_ACCESSORS (GAS 핵심 개념)
//
// GAS에서 Attribute(FGameplayAttributeData)는 일반 변수(float)가 아니다.
//  - GameplayEffect 적용
//  - 네트워크 동기화
//  - 변경 이벤트 브로드캐스트
// 를 모두 처리하는 특수 데이터다.
//
// Attribute는 직접 값에 접근하지 않고,
// GAS가 제공하는 전용 Getter/Setter/Attribute 참조 함수를 통해 다뤄야 한다.
//
// ATTRIBUTE_ACCESSORS(ClassName, PropertyName) 매크로는
// 아래 4개의 함수를 자동으로 생성해준다
//
// 1. Get<Property>Attribute()
//    - 이 Attribute를 가리키는 FGameplayAttribute 반환
//    - GameplayEffect에서 "어떤 Attribute를 변경할지" 지정할 때 사용
//
// 2. Get<Property>()
//    - 현재 Attribute 값 조회 (UI 표시 등)
//
// 3. Set<Property>(float NewValue)
//    - 코드에서 Attribute 값을 직접 세팅할 때 사용 (권장 빈도 낮음)
//
// 4. Init<Property>(float NewValue)
//    - 초기값 설정용 (스폰 시, 초기화 단계)
//
// 이 매크로를 사용하지 않고 Attribute에 직접 값을 대입하면:
//  - GAS 내부 변경 이벤트가 동작하지 않고
//  - Effect / 네트워크 동기화가 깨질 수 있다.
//
// Attribute는 반드시 ATTRIBUTE_ACCESSORS로 생성된 함수들을 통해서만 다룬다.
// ============================================================================

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
    UAS_Unit();

public:
    // 현재 체력
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    FGameplayAttributeData HP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, HP)

    // 최대 체력
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    FGameplayAttributeData MaxHP;
    ATTRIBUTE_ACCESSORS(UAS_Unit, MaxHP)

public:
    void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data);
};
