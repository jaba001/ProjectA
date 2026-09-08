#pragma once

#include "CoreMinimal.h"
#include "CombatResult.generated.h"

UENUM(BlueprintType)
enum class ECombatResult : uint8
{
    None,
    Victory,
    Defeat
};
