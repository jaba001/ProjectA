#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTargetingLibrary.generated.h"

class ACombatGridTile;
class AUnitBase;

UCLASS()
class PROJECTA_API UCombatTargetingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
    static TArray<AUnitBase*> CollectUniqueAliveUnitsFromTiles(const TArray<ACombatGridTile*>& TargetTiles, AUnitBase* SourceUnit);

};