#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SkillDatabaseDataAsset.generated.h"

class USkillDefinitionDataAsset;

// DataAsset-based registry for all skills in the project
// 프로젝트 전체 스킬을 관리하는 데이터에셋 기반 레지스트리
UCLASS(BlueprintType)
class PROJECTA_API USkillDatabaseDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Full skill list in this project
    // 프로젝트 전체 스킬 목록
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SkillDatabase")
    TArray<TObjectPtr<USkillDefinitionDataAsset>> AllSkills;

public:
    // Finds a skill by SkillId
    // SkillId로 스킬을 검색
    UFUNCTION(BlueprintCallable, Category = "SkillDatabase")
    USkillDefinitionDataAsset* FindSkillById(FName SkillId) const;

    // Returns whether the skill exists in this database
    // 이 데이터베이스에 스킬이 포함되어 있는지 반환
    UFUNCTION(BlueprintCallable, Category = "SkillDatabase")
    bool ContainsSkill(USkillDefinitionDataAsset* Skill) const;

    // Returns all non-null skills
    // null이 아닌 전체 스킬 반환
    UFUNCTION(BlueprintCallable, Category = "SkillDatabase")
    TArray<USkillDefinitionDataAsset*> GetAllValidSkills() const;
};
