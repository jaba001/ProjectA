#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTargetingLibrary.generated.h"

class ACombatGridTile;
class AUnitBase;
class USkillDefinitionDataAsset;

// Blueprint utility functions for combat target collection.
// 전투 대상 수집을 위한 블루프린트 유틸리티 함수 모음입니다.
UCLASS()
class PROJECTA_API UCombatTargetingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Share target eligibility between player selection, enemy decisions and skill execution.
    // 플레이어 선택, 적 판단, 스킬 실행에서 같은 대상 허용 규칙을 사용합니다.
    UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
    static bool IsValidSkillTarget(const AUnitBase* SourceUnit, const USkillDefinitionDataAsset* SkillData, const ACombatGridTile* TargetTile);

    // Collects unique alive units from the given target tiles.
    // 지정된 대상 타일들에서 살아있는 유닛을 중복 없이 수집합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
    static TArray<AUnitBase*> CollectUniqueAliveUnitsFromTiles(const TArray<ACombatGridTile*>& TargetTiles, AUnitBase* SourceUnit);

};
