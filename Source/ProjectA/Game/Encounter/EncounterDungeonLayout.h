#pragma once

#include "CoreMinimal.h"

namespace EncounterDungeonLayout
{
    constexpr float CellSize = 400.f;
    constexpr float WallHeight = 420.f;
    constexpr float EyeHeight = 170.f;

    PROJECTA_API TArray<FVector> GetPath(int32 Direction);
    PROJECTA_API TSet<FIntPoint> GetFloorCells();
    PROJECTA_API FTransform GetStageTransform(int32 Direction);
    PROJECTA_API FText GetDirectionLabel(int32 Index);
}
