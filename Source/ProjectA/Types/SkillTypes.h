#pragma once

#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

// 스킬이 최종적으로 무엇을 선택 대상으로 삼는지 정의
UENUM(BlueprintType)
enum class ESkillTargetingType : uint8
{
    Unit,
    Tile
};

// 스킬이 선택 가능한 대상의 팀 규칙을 정의
UENUM(BlueprintType)
enum class ESkillTargetTeamRule : uint8
{
    EnemyOnly,
    AllyOnly,
    AnyUnit,
    EmptyTileOnly
};

// 스킬의 범위 적용 방식
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