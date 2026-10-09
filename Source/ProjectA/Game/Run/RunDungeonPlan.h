#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunDungeonTypes.h"

class URunSaveGame;
struct FRunEncounterProgress;
struct FRunEncounterOffer;

namespace RunDungeonPlan
{
    PROJECTA_API bool Build(const URunSaveGame& Save, FRunDungeonState& OutState, FText& OutError);
    PROJECTA_API bool Validate(const URunSaveGame& Save, FText& OutError);
    PROJECTA_API int32 FindVisit(const FRunDungeonState& State, const FRunEncounterProgress& Progress);
    PROJECTA_API bool ResolveOffers(const URunSaveGame& Save, int32 CompletedCount, int32 VisitIndex, TArray<FRunEncounterOffer>& OutOffers, FText& OutError);
}
