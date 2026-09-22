#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatEffectLibrary.generated.h"

class AUnitBase;
class UGameplayEffect;
struct FGameplayTagContainer;

// Blueprint utility functions for applying combat effects.
// 전투 효과 적용을 위한 블루프린트 유틸리티 함수 모음입니다.
UCLASS()
class PROJECTA_API UCombatEffectLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Applies a damage gameplay effect to a single target unit.
    // 단일 대상 유닛에게 피해 게임플레이 이펙트를 적용합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat|Effect")
    static bool ApplyDamageToUnit(AUnitBase* SourceUnit, AUnitBase* TargetUnit, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount);

    // Applies a damage gameplay effect to multiple target units.
    // 여러 대상 유닛에게 피해 게임플레이 이펙트를 적용합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat|Effect")
    static int32 ApplyDamageToUnits(AUnitBase* SourceUnit, const TArray<AUnitBase*>& TargetUnits, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount);

    // Use the same GAS application path for legacy callers and authored round effect tags.
    // 기존 호출과 제작된 라운드 효과 태그가 동일한 GAS 적용 경로를 사용합니다.
    static bool ApplyTaggedEffectToUnit(AUnitBase* SourceUnit, AUnitBase* TargetUnit, TSubclassOf<UGameplayEffect> EffectClass, float DamageAmount, const FGameplayTagContainer& AssetTags);
};
