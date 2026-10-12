#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/Round/CombatStatusTypes.h"
#include "RunSkillBalanceTypes.generated.h"

// Freeze numeric tuning separately from the original GAS, targeting and collision contracts.
// 원래 GAS·대상·충돌 계약과 별개로 수치 조정값을 고정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunSkillBalance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    FGameplayTag RarityTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    float Power = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    int32 ActionPointCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    int32 SubActionPointCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    float WindupSeconds = 0.f;

    // Frozen with the Run like the other numbers; Runs saved before this field keep no statuses.
    // 다른 수치처럼 Run에 고정되며 이 필드 이전에 저장된 Run은 상태이상 없이 유지됩니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    TArray<FCombatStatusApplication> StatusApplications;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunSkillRarityWeight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    FGameplayTag EquipmentRarityTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    FGameplayTag SkillRarityTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Skill Balance")
    float Weight = 0.f;
};
