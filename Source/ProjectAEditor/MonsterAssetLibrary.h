#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MonsterAssetLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequence;
class UAnimMontage;
class UBlendSpace;
class UBlendSpace1D;
class USkillDefinitionDataAsset;

// Author owned monster animation assets while preserving original sequences and skeletons.
// 원본 시퀀스와 스켈레톤을 보존하며 프로젝트 소유 몬스터 애니메이션 에셋을 작성합니다.
UCLASS()
class PROJECTAEDITOR_API UMonsterAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Read the runtime-resolved profile so derived monster skills retain GAS tags, effects and values.
    // 실제 실행 프로필을 읽어 파생 몬스터 스킬의 GAS 태그, 효과와 수치를 보존합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Monster Authoring")
    static FCombatRoundSkill GetResolvedMonsterSkill(USkillDefinitionDataAsset* SkillAsset);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Monster Authoring")
    static bool ValidateMonsterSkill(USkillDefinitionDataAsset* SkillAsset);

    // Inspect a single non-looping source attack without executing its animation or gameplay.
    // 애니메이션이나 게임을 실행하지 않고 반복 없는 단일 원본 공격을 검사합니다.
    UFUNCTION(BlueprintPure, Category = "ProjectA|Monster Authoring")
    static bool ValidateMonsterMontage(UAnimMontage* Montage, UAnimSequence* Source);

    // Configure an empty project BlendSpace once; a matching owned result is reused without mutation.
    // 빈 프로젝트 BlendSpace를 한 번 구성하며 일치하는 소유 결과는 변경 없이 재사용합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Monster Authoring")
    static bool ConfigureMonsterBlendSpace(UBlendSpace1D* BlendSpace, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, float WalkSpeed, float RunSpeed);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Monster Authoring")
    static bool ValidateMonsterBlendSpace(UBlendSpace1D* BlendSpace, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, float WalkSpeed, float RunSpeed);

    // Connect native GroundSpeed to locomotion and use an existing slot or the engine DefaultSlot fallback.
    // 네이티브 GroundSpeed를 이동에 연결하며 기존 슬롯 또는 엔진 DefaultSlot 기본 처리를 사용합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Monster Authoring")
    static bool ConfigureLocomotionAnimBlueprint(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace, FName SlotName);

    UFUNCTION(BlueprintPure, Category = "ProjectA|Monster Authoring")
    static bool ValidateLocomotionAnimBlueprint(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace, FName SlotName);
};
