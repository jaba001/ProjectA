#pragma once

#include "CoreMinimal.h"
#include "UnitActionTypes.generated.h"

// Every accepted action completes once with an explicit outcome.
// 수락된 행동은 명시적인 결과로 한 번만 완료됩니다.
UENUM(BlueprintType)
enum class EUnitActionResult : uint8
{
    Succeeded,
    Failed,
    Cancelled
};
