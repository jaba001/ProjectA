#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"

namespace RunItemPresentation
{
    const FRunWeaponRarityRule* FindRarity(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText Name(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText GrantedSkills(const FRunItemDefinition& Item);
    FText Tooltip(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
}
