#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"

namespace RunItemShopCatalog
{
    PROJECTA_API FGameplayTag GetWeaponTag();
    PROJECTA_API bool Load(TArray<FRunItemDefinition>& OutCatalog, FText& OutError);
    PROJECTA_API bool Roll(FRunItemShopState& State, bool bAllowDuplicates, const FGameplayTagQuery& Query, FText& OutError);
    PROJECTA_API bool Validate(const FRunItemShopState& State, FText& OutError);
    PROJECTA_API bool ValidateItem(const FRunItemDefinition& Item);
}
