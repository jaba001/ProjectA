#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EquipmentSkillAssetLibrary.generated.h"

class UAnimMontage;
class UAnimSequence;
class USkillDefinitionDataAsset;

// Bake and inspect equipment skills without creating a game world or executing combat.
// 게임 월드 생성이나 전투 실행 없이 장비 스킬을 작성하고 검사합니다.
UCLASS()
class PROJECTAEDITOR_API UEquipmentSkillAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Bake montage trims and engine bone mirroring into an editable sequence without changing the source.
    // 원본을 변경하지 않고 몽타주 구간과 엔진 본 미러링을 편집 가능한 시퀀스로 굽습니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool BakeMeleeMontageMotion(UAnimSequence* Destination, UAnimMontage* Source, bool bMirror);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateMeleeMontageMotion(UAnimSequence* Sequence, UAnimMontage* Source, bool bMirror);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ConfigureDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, UAnimMontage* Montage, float Windup, float StrikeInterval);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, FString& OutReport);
};
