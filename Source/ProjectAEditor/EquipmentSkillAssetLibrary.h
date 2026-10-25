#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EquipmentSkillAssetLibrary.generated.h"

class UAnimMontage;
class USkillDefinitionDataAsset;

// Bake and inspect equipment skills without creating a game world or executing combat.
// 게임 월드 생성이나 전투 실행 없이 장비 스킬을 작성하고 검사합니다.
UCLASS()
class PROJECTAEDITOR_API UEquipmentSkillAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ConfigureDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, UAnimMontage* Montage, float Windup, float StrikeInterval);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, FString& OutReport);
};
