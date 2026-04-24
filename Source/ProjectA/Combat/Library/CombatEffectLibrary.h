#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatEffectLibrary.generated.h"

class AUnitBase;
class UGameplayEffect;

UCLASS()
class PROJECTA_API UCombatEffectLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Combat|Effect")
    static bool ApplyDamageToUnit(
        AUnitBase* SourceUnit,
        AUnitBase* TargetUnit,
        TSubclassOf<UGameplayEffect> DamageEffectClass,
        float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Combat|Effect")
    static int32 ApplyDamageToUnits(
        AUnitBase* SourceUnit,
        const TArray<AUnitBase*>& TargetUnits,
        TSubclassOf<UGameplayEffect> DamageEffectClass,
        float DamageAmount);
};