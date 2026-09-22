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
}
