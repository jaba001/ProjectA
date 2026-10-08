#pragma once

#include "CoreMinimal.h"
#include "Game/Run/TargetRunTypes.h"

namespace RunEncounterPool
{
    PROJECTA_API bool Load(FRunTargetState& State, FText& OutError);
    PROJECTA_API bool LoadFromString(FString CsvText, int32 Seed, FRunTargetState& State, FText& OutError);
    PROJECTA_API bool Validate(const FRunTargetState& State, FText& OutError);
    PROJECTA_API bool Select(const FRunTargetState& State, int32 CombatIndex, int32 VisitIndex, TArray<FRunEncounterOffer>& OutOffers, FText& OutError);
    PROJECTA_API bool IsSameOffer(const FRunEncounterOffer& Left, const FRunEncounterOffer& Right);
}
