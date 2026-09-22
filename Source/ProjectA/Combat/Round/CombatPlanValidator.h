#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"

struct FCombatCheckpointData;

// Actor-free planning inputs are shared by live commands and serialized checkpoints.
// 액터 없는 계획 입력을 실시간 명령과 직렬화된 체크포인트에서 공유합니다.
namespace CombatPlanValidation
{
    struct PROJECTA_API FUnit
    {
        int32 UnitId = INDEX_NONE;
        bool bAlive = false;
        bool bEnemy = false;
        int32 AP = 0;
        int32 SAP = 0;
        FIntPoint HomeCoord = FIntPoint::ZeroValue;
        TArray<FName> SkillIds;
        FCombatRoundCommand Command;
        bool bHasMovePlan = false;
        FIntPoint MoveDestinationCoord = FIntPoint::ZeroValue;
    };

    struct PROJECTA_API FState
    {
        TArray<FUnit> Units;
        TArray<FCombatRoundSkill> Skills;
        // INDEX_NONE denotes an empty tile; unknown external occupants use a distinct blocked value.
        // INDEX_NONE은 빈칸이며 전투 외부 점유자는 별도 차단 값으로 표시합니다.
        TMap<FIntPoint, int32> Tiles;
    };

    PROJECTA_API const FCombatRoundSkill* FindSkill(const FState& State, FName SkillId);
    PROJECTA_API bool IsReservedByOther(const FState& State, int32 UnitId, FIntPoint Coord);
    PROJECTA_API bool ValidateCommand(const FState& State, const FCombatRoundCommand& Command, FText& OutError);
    PROJECTA_API bool ValidateMoveDestination(const FState& State, const FUnit& Unit, FIntPoint Destination, FText& OutError);
    PROJECTA_API bool ValidateDestinations(const FState& State, FText& OutError);
    PROJECTA_API bool ValidateCheckpointPlans(const FCombatCheckpointData& Checkpoint, FText& OutError);
}
