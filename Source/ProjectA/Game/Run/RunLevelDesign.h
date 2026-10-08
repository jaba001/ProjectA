#pragma once

#include "CoreMinimal.h"

struct FRunTargetState;

namespace RunLevelDesign
{
    PROJECTA_API bool Load(FRunTargetState& State, int32 PartySize, FText& OutError);
    PROJECTA_API bool LoadFromStrings(FString Stats, FString Weights, FString Levels, int32 Seed, int32 PartySize, FRunTargetState& State, FText& OutError);

    // Build and validate frozen values without reading CSV files or loading assets.
    // CSV 파일을 읽거나 에셋을 로드하지 않고 고정 값을 구성하고 검증합니다.
    PROJECTA_API bool Build(FRunTargetState& State, FText& OutError);
    PROJECTA_API bool Validate(const FRunTargetState& State, FText& OutError);
}
