#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunLevelDesignTypes.h"
#include "Game/Run/RunPveDifficultyTypes.h"

struct FRunTargetState;
struct FRunProgressView;

namespace RunPveDifficulty
{
    PROJECTA_API FGameplayTag GetLowTag();
    PROJECTA_API FGameplayTag GetMediumTag();
    PROJECTA_API FGameplayTag GetHighTag();
    PROJECTA_API bool Load(FRunPveDifficultyState& State, FText& OutError);
    PROJECTA_API bool LoadFromString(FString Csv, FRunPveDifficultyState& State, FText& OutError);
    PROJECTA_API bool Validate(const FRunTargetState& State, const FRunProgressView& Progress, FText& OutError);
    PROJECTA_API bool Resolve(const FRunTargetState& State, int32 CombatIndex, TArray<FRunMonsterDefinition>& OutRoster, TArray<int32>& OutGoldChoices, FText& OutError);
    PROJECTA_API bool ResolveArena(const FRunTargetState& State, int32 CombatIndex, FName& OutArenaId, FText& OutError);
    PROJECTA_API bool BuildOffers(const FRunTargetState& State, int32 CombatIndex, TArray<FRunPveDifficultyOffer>& OutOffers, FText& OutError);
}
