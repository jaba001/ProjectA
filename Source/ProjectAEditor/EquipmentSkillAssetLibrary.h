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
    // Bake mirrored montage motion with optional trimming and a smooth transition into the next sequence.
    // 몽타주 미러링과 선택적인 구간 단축 및 다음 시퀀스로의 부드러운 연결을 굽습니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool BakeMeleeMontageMotion(UAnimSequence* Destination, UAnimMontage* Source, bool bMirror, float EndTime = 0.f, UAnimSequence* TransitionTarget = nullptr, float TransitionSeconds = 0.f);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateMeleeMontageMotion(UAnimSequence* Sequence, UAnimMontage* Source, bool bMirror, float EndTime = 0.f, UAnimSequence* TransitionTarget = nullptr, float TransitionSeconds = 0.f);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ConfigureDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, UAnimMontage* Montage, float Windup, float StrikeInterval);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Asset Authoring")
    static bool ValidateDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, FString& OutReport);
};
