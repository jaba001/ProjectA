#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/SkillTypes.h"
#include "Combat/Round/CombatRoundTypes.h"
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
    // Opt into authored real-time execution values; older assets receive a documented migration profile.
    // 실시간 실행 수치를 직접 지정하며 기존 에셋은 명시된 이행 기본값을 사용합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Round")
    bool bUseRoundDefinition = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Round", meta = (EditCondition = "bUseRoundDefinition"))
    FCombatRoundSkill RoundDefinition;

    // Resolve and validate the same execution profile for runtime, catalogues and editor validation.
    // 실행 중 처리, 카탈로그, 에디터 검증에서 동일한 실행 프로필을 해석하고 검사합니다.
    bool ResolveRoundSkill(FCombatRoundSkill& OutSkill, FText& OutError) const;

#if WITH_EDITOR
    // Report invalid profiles and legacy semantics that require an explicit round definition.
    // 잘못된 프로필과 명시적 라운드 정의가 필요한 기존 의미를 오류로 보고합니다.
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

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

    // Legacy damage and montage source; an empty round montage may reuse this class without activating it.
    // 기존 피해와 몽타주 원본이며 라운드 몽타주가 비어 있으면 활성화 없이 이 클래스의 표현을 재사용합니다.
    // Authored round profiles remain valid without an ability class or a montage.
    // 직접 작성한 라운드 프로필은 어빌리티 클래스나 몽타주 없이도 유효합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

    // Action Point cost required to use this skill.
    // 이 스킬 사용에 필요한 행동력 비용입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cost", meta = (ClampMin = "1"))
    int32 ActionPointCost = 1;

    // Display the authoritative AP cost, or identify invalid skill data.
    // 기준 AP 비용을 표시하며 잘못된 스킬 데이터는 오류로 표시합니다.
    UFUNCTION(BlueprintPure, Category = "Skill|Cost")
    FText GetActionPointCostText() const;

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
