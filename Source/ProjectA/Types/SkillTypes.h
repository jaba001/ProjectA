#pragma once

#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

// Rule that describes which target kind a skill can select.
// 스킬이 선택할 수 있는 대상 종류를 정의하는 규칙입니다.
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

// Defines how the skill area is applied.
// 스킬 범위가 적용되는 방식을 정의합니다.
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
