#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTargetingLibrary.generated.h"

class ACombatGridTile;
class AUnitBase;

// Blueprint utility functions for combat target collection.
// 전투 대상 수집을 위한 블루프린트 유틸리티 함수 모음입니다.
UCLASS()
class PROJECTA_API UCombatTargetingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Collects unique alive units from the given target tiles.
    // 지정된 대상 타일들에서 살아있는 유닛을 중복 없이 수집합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
    static TArray<AUnitBase*> CollectUniqueAliveUnitsFromTiles(const TArray<ACombatGridTile*>& TargetTiles, AUnitBase* SourceUnit);

};
