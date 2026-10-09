#pragma once

#include "CoreMinimal.h"

namespace EncounterDungeonLayout
{
    constexpr int32 VariantCount = 8;
    constexpr float CellSize = 400.f;
    constexpr float WallHeight = 420.f;
    constexpr float EyeHeight = 170.f;

    // Legacy APIs preserve variant zero; invalid variants return empty geometry or identity.
    // 기존 인자 형식은 변형 0을 유지하며 잘못된 변형은 빈 지형 또는 단위 변환을 반환합니다.
    PROJECTA_API TArray<FVector> GetPath(int32 Direction);
    PROJECTA_API TArray<FVector> GetPath(int32 Direction, int32 LayoutVariant);
    PROJECTA_API TSet<FIntPoint> GetFloorCells();
    PROJECTA_API TSet<FIntPoint> GetFloorCells(int32 LayoutVariant);
    PROJECTA_API FTransform GetStageTransform(int32 Direction);
    PROJECTA_API FTransform GetStageTransform(int32 Direction, int32 LayoutVariant);
    PROJECTA_API FText GetDirectionLabel(int32 Index);
}
