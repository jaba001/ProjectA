#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/SkillTypes.h"
#include "SkillDefinitionDataAsset.generated.h"

class UGameplayAbility;
class UTexture2D;

// DataAsset that stores static skill definition data.
// 입력 규칙, UI 데이터, 연결된 어빌리티 클래스를 저장하는 스킬 정의 데이터 에셋입니다.
UCLASS(BlueprintType)
class PROJECTA_API USkillDefinitionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Skill identifier name.
    // 스킬 식별자 이름입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FName SkillId = NAME_None;

    // Skill name for UI display.
    // UI에 표시할 스킬 이름입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FText SkillName;

    // Skill description for UI display.
    // UI에 표시할 스킬 설명입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FText SkillDescription;

    // Skill icon for UI display.
    // UI에 표시할 스킬 아이콘입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TObjectPtr<UTexture2D> SkillIcon = nullptr;

    // GAS Ability class executed by this skill.
    // 이 스킬이 실행하는 GAS 어빌리티 클래스입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

    // Action Point cost required to use this skill.
    // 이 스킬 사용에 필요한 행동력 비용입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cost")
    int32 ActionPointCost = 1;

    // Whether this skill ignores front-line protection and can target back-line units directly.
    // 전열 보호를 무시하고 후열 유닛을 직접 대상으로 삼을 수 있는지 여부입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    bool bIgnoreFront = false;

    // Whether movement to the target position is required before executing the skill.
    // 스킬 실행 전에 대상 위치로 이동해야 하는지 여부입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    bool bMoveToTarget = false;

    // Target team rule for the skill.
    // 스킬의 대상 팀 규칙입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    ESkillTargetRule TargetRule = ESkillTargetRule::EnemyUnit;

    // Area application type of the skill.
    // 스킬의 범위 적용 타입입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Area")
    ESkillAreaType AreaType = ESkillAreaType::Single;

    // Grid radius used for AroundTarget and AroundSelf types.
    // AroundTarget과 AroundSelf 타입에서 사용하는 그리드 반경입니다.
    // Range calculation is based on Chebyshev distance.
    // 범위 계산은 체비셰프 거리를 기준으로 합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Area", meta = (ClampMin = "0"))
    int32 AreaRadius = 0;

};
