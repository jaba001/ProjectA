#pragma once

#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

UENUM(BlueprintType)
enum class ESkillTargetRule : uint8
{
    EnemyUnit,
    AllyUnit,
    AnyUnit,
    EnemyTile,
    AllyTile,
    AnyTile
};

// Defines how the skill area is applied
UENUM(BlueprintType)
enum class ESkillAreaType : uint8
{
    Single,
    Row,
    Column,
    LeftAndTarget,
    RightAndTarget,
    DiagonalTarget,
    AroundTarget,
    AroundSelf,
    AllEnemies
};