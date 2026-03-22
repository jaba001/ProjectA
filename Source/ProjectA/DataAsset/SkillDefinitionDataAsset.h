#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/SkillTypes.h"
#include "SkillDefinitionDataAsset.generated.h"

class UGameplayAbility;
class UTexture2D;

// 스킬의 정적 정의 데이터를 보관하는 DataAsset
// 입력 규칙, UI 정보, 연결할 AbilityClass를 정의한다
UCLASS(BlueprintType)
class PROJECTA_API USkillDefinitionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 스킬 식별용 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FName SkillId = NAME_None;

    // UI 표시용 스킬 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FText SkillName;

    // UI 표시용 스킬 설명
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FText SkillDescription;

    // UI 표시용 스킬 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TObjectPtr<UTexture2D> SkillIcon = nullptr;

    // 이 스킬이 실행할 GAS Ability 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

    // 스킬 사용에 필요한 Action Point 비용
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cost")
    int32 ActionPointCost = 1;

    // 전열 보호를 무시하고 후열을 직접 타겟팅할 수 있는지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    bool bIgnoreFront = false;

    // 스킬 실행 전에 대상 위치까지 이동이 필요한지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    bool bMoveToTarget = false;

    // 스킬의 대상 선택 방식
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    ESkillTargetingType TargetingType = ESkillTargetingType::Unit;

    // 스킬의 대상 팀 판정 규칙
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    ESkillTargetTeamRule TargetTeamRule = ESkillTargetTeamRule::EnemyOnly;

    // 스킬의 범위 적용 방식
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Area")
    ESkillAreaType AreaType = ESkillAreaType::Single;

    // AroundTarget / AroundSelf 계열에서 사용할 그리드 반경 값
    // 범위 판정은 체비쇼프 거리 기준으로 계산한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Area", meta = (ClampMin = "0"))
    int32 AreaRadius = 0;

};