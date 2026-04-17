#pragma once

#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

// Defines what the skill ultimately targets
UENUM(BlueprintType)
enum class ESkillTargetingType : uint8
{
    Unit,
    Tile
};

// Defines the team filtering rule for valid targets
UENUM(BlueprintType)
enum class ESkillTargetTeamRule : uint8
{
    EnemyOnly,
    AllyOnly,
    AnyUnit,
    EmptyTileOnly
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