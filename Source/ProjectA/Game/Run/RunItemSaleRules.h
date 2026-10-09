#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemSaleTypes.h"

struct FRunPartyMember;
struct FRunItemDefinition;
struct FRunItemShopState;
struct FRunEncounterProgress;

namespace RunItemSaleRules
{
    PROJECTA_API int32 GetPrice(const FRunItemDefinition& Item);
    PROJECTA_API bool Validate(const FRunPartyMember& Member, const FRunItemShopState& Shop, const FRunEncounterProgress& Encounter, const FRunItemSaleCommand& Command, FText& OutError);
    PROJECTA_API bool Apply(FRunPartyMember& Member, FRunItemShopState& Shop, const FRunEncounterProgress& Encounter, const FRunItemSaleCommand& Command, FText& OutError);
}
