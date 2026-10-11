#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatRoundTypes.h"

namespace CombatPlanValidation
{
    struct FState;
}

namespace CombatAIPlanning
{
    PROJECTA_API int32 FindNearestEnemy(const FCombatRoundView& View, int32 SourceIndex, TFunctionRef<bool(const FCombatRoundUnitView&)> IsAllowed);
    PROJECTA_API FCombatRoundCommand ChooseCommand(const FCombatRoundView& View, const CombatPlanValidation::FState& State, int32 UnitIndex, TFunctionRef<bool(const FCombatRoundCommand&)> IsAllowed);
    // Every allowed skill and target pair, using the same destination rule as the regular AI.
    // 일반 AI와 같은 목적지 규칙으로 허용되는 모든 스킬·대상 조합을 만듭니다.
    PROJECTA_API TArray<FCombatRoundCommand> CollectCommands(const FCombatRoundView& View, const CombatPlanValidation::FState& State, int32 UnitIndex, TFunctionRef<bool(const FCombatRoundCommand&)> IsAllowed);
}
