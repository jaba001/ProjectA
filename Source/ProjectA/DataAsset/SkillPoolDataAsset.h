#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SkillPoolDataAsset.generated.h"

class USkillDefinitionDataAsset;

USTRUCT(BlueprintType)
struct FSkillPoolEntry
{
    GENERATED_BODY()

public:
    // Skill candidate entry
    // 스킬 후보 엔트리
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SkillPool")
    TObjectPtr<USkillDefinitionDataAsset> Skill = nullptr;

    // Weight used for weighted random selection
    // 가중치 랜덤 선택에 사용되는 가중치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SkillPool", meta = (ClampMin = "0"))
    int32 Weight = 1;
};

// DataAsset that stores a candidate pool of skills
// 스킬 후보 풀을 저장하는 데이터 에셋
UCLASS(BlueprintType)
class PROJECTA_API USkillPoolDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Candidate entries for this pool
    // 이 풀의 후보 엔트리 목록
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SkillPool")
    TArray<FSkillPoolEntry> Entries;

public:
    // Returns all valid skill assets from entries
    // 엔트리에서 유효한 스킬 에셋만 반환
    UFUNCTION(BlueprintCallable, Category = "SkillPool")
    TArray<USkillDefinitionDataAsset*> GetValidSkills() const;

    // Returns one weighted-random valid skill
    // 가중치 기반으로 유효 스킬 하나를 랜덤 반환
    UFUNCTION(BlueprintCallable, Category = "SkillPool")
    USkillDefinitionDataAsset* GetRandomSkill() const;
};
