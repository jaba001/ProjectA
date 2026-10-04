#pragma once

#include "CoreMinimal.h"

class AUnitBase;
struct FCombatRoundSkill;

namespace CombatConsumableRules
{
    PROJECTA_API bool CanUse(const AUnitBase* Source, const AUnitBase* Target, const FCombatRoundSkill& Skill, bool bHumanControlled);
    PROJECTA_API bool Release(AUnitBase* Source, AUnitBase* Target, const FCombatRoundSkill& Skill, bool bHumanControlled);
}
