#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EncounterDefinitionDataAsset.generated.h"

class AEnemyUnit;

UCLASS(BlueprintType)
class PROJECTA_API UEncounterDefinitionDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter")
    TArray<TSubclassOf<AEnemyUnit>> EnemyUnitClasses;
};
