#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/SkillTypes.h"
#include "SkillDefinitionDataAsset.generated.h"

class UGameplayAbility;
class UTexture2D;

// DataAsset that stores static skill definition data
// Defines input rules, UI data, and the associated AbilityClass
UCLASS(BlueprintType)
class PROJECTA_API USkillDefinitionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Skill identifier name
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FName SkillId = NAME_None;

    // Skill name for UI display
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FText SkillName;

    // Skill description for UI display
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FText SkillDescription;

    // Skill icon for UI display
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TObjectPtr<UTexture2D> SkillIcon = nullptr;

    // GAS Ability class executed by this skill
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

    // Action Point cost required to use this skill
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cost")
    int32 ActionPointCost = 1;

    // Whether this skill ignores front-line protection and can target back-line units directly
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    bool bIgnoreFront = false;

    // Whether movement to the target position is required before executing the skill
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    bool bMoveToTarget = false;

    // Target team rule for the skill
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
    ESkillTargetRule TargetRule = ESkillTargetRule::EnemyUnit;

    // Area application type of the skill
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Area")
    ESkillAreaType AreaType = ESkillAreaType::Single;

    // Grid radius used for AroundTarget / AroundSelf types
    // Range calculation is based on Chebyshev distance
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Area", meta = (ClampMin = "0"))
    int32 AreaRadius = 0;

};