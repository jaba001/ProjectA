#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"

struct FRunWeaponSkillRulesState;

namespace RunItemShopCatalog
{
    PROJECTA_API FGameplayTag GetWeaponTag();
    PROJECTA_API bool Load(TArray<FRunItemDefinition>& OutCatalog, FText& OutError);
    PROJECTA_API bool LoadFromString(FString CsvText, TArray<FRunItemDefinition>& OutCatalog, FText& OutError);
    PROJECTA_API bool Roll(FRunItemShopState& State, bool bAllowDuplicates, const FGameplayTagQuery& Query, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules = nullptr);
    PROJECTA_API bool Validate(const FRunItemShopState& State, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules = nullptr);
    PROJECTA_API bool ValidateItem(const FRunItemDefinition& Item);
    PROJECTA_API bool IsSameDefinition(const FRunItemDefinition& Left, const FRunItemDefinition& Right);
    PROJECTA_API bool IsSameBaseDefinition(const FRunItemDefinition& Left, const FRunItemDefinition& Right);
}
