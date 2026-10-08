#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"

struct FRunWeaponSkillRulesState;

namespace RunItemRarityProbabilities
{
    PROJECTA_API bool Load(FRunItemRarityProbabilityState& OutState, FText& OutError);
    PROJECTA_API bool LoadFromString(FString CsvText, FRunItemRarityProbabilityState& OutState, FText& OutError);
    PROJECTA_API bool Validate(const FRunItemRarityProbabilityState& State, FText& OutError);
    PROJECTA_API bool GetEligibleIndices(const TArray<FRunItemDefinition>& Catalog, const FRunItemRarityProbabilityState& State, const FGameplayTagQuery& Query, TArray<int32>& OutIndices, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules = nullptr);
    PROJECTA_API bool Select(const TArray<FRunItemDefinition>& Catalog, const FRunItemRarityProbabilityState& State, const FGameplayTagQuery& Query, int32 Count, bool bAllowDuplicates, FRandomStream& Random, TArray<int32>& OutIndices, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules = nullptr);
}
