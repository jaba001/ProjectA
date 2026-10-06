#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunWeaponSkillTypes.generated.h"

// Freeze skill eligibility and weight data alongside the original GAS content tags.
// 기존 GAS 콘텐츠 태그와 함께 스킬 적합성 및 가중치 데이터를 고정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunWeaponSkillCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FSoftObjectPath Skill;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FGameplayTagContainer Tags;

    // Selection-only metadata augments queries without rewriting the skill's GAS execution tags.
    // 선택 전용 분류는 스킬의 GAS 실행 태그를 바꾸지 않고 쿼리 조건에 추가합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FGameplayTagContainer SelectionTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FGameplayTagQuery AllowedItemQuery;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills", meta = (ClampMin = "0"))
    float BaseWeight = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunWeaponRarityRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FGameplayTag RarityTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills", meta = (ClampMin = "0"))
    float BaseWeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FGameplayTagQuery SkillQuery;
};

// Store all selection inputs in the Run so restored copies never depend on a new random draw.
// 복원된 사본이 새 추첨에 의존하지 않도록 모든 선택 입력을 Run에 저장합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunWeaponSkillRulesState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    int32 SchemaVersion = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    int32 SkillCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    FGameplayTagQuery WeaponQuery;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    TArray<FRunWeaponSkillCandidate> Candidates;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Weapon Skills")
    TArray<FRunWeaponRarityRule> Rarities;
};
