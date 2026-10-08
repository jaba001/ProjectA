#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunGoldRewardTypes.h"

struct FRunWeaponSkillRulesState;

namespace RunCombatRewards
{
    PROJECTA_API bool Build(FName NodeId, const FRunItemShopState& Shop, const FRunWeaponSkillRulesState& Rules, const TArray<int32>& GoldChoices, FRandomStream& Random, FRunGoldRewardState& OutState, FText& OutError);
    PROJECTA_API bool Validate(const FRunGoldRewardState& State, const FRunItemShopState& Shop, const FRunWeaponSkillRulesState& Rules, const TArray<int32>& GoldChoices, FText& OutError);
}
