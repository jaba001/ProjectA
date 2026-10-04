#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunRecoveryTypes.generated.h"

struct FCombatRoundSkill;

// Keep consumable quantities separate from the five acquired skill slots.
// 소모품 수량은 습득 스킬 다섯 칸과 분리하여 보존합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunConsumableStack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FGameplayTag ItemTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FSoftObjectPath Skill;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunRecoveryState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 SchemaVersion = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 Revision = 0;
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag ConsumableTag;
    UPROPERTY(BlueprintReadOnly)
    FSoftObjectPath HealingSkill;
    UPROPERTY(BlueprintReadOnly)
    int32 StartingQuantity = 1;
    UPROPERTY(BlueprintReadOnly)
    int32 ConsumablePrice = 1;
    UPROPERTY(BlueprintReadOnly)
    float RecoveryHP = 25.0f;
    UPROPERTY(BlueprintReadOnly)
    int32 RecoveryPrice = 1;
    UPROPERTY(BlueprintReadOnly)
    float RevivalFraction = 0.25f;
    UPROPERTY(BlueprintReadOnly)
    int32 RevivalPrice = 1;
};

namespace RunRecoveryRules
{
    inline constexpr int32 MaximumQuantity = 1000;
    PROJECTA_API FGameplayTag GetConsumableTag();
    PROJECTA_API FGameplayTag GetHealingItemTag();
    PROJECTA_API FSoftObjectPath GetHealingSkillPath();
    PROJECTA_API bool IsConsumable(const FCombatRoundSkill& Skill);
    PROJECTA_API bool ResolveStack(const FRunConsumableStack& Stack, FCombatRoundSkill& OutSkill, FText& OutError);
    PROJECTA_API bool ValidateStacks(const TArray<FRunConsumableStack>& Stacks, FText& OutError);
    PROJECTA_API bool ValidateState(const FRunRecoveryState& State, FText& OutError);
}
