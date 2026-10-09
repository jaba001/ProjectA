#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"

namespace RunItemPresentation
{
    struct FItemSkillDetails
    {
        FText Name;
        FText Description;
        FText Stats;
        FLinearColor Color = FLinearColor::White;
    };

    const FRunWeaponRarityRule* FindRarity(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText Name(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText GrantedSkills(const FRunItemDefinition& Item, bool bIncludeStats = true);
    FText Tooltip(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText EquipmentDescription(const FRunItemDefinition& Item);
    // Build display values during a view refresh, never from a hover tick or a new random selection.
    // hover Tick이나 새 추첨 대신 화면 갱신 시 표시할 값을 작성합니다.
    TArray<FItemSkillDetails> SkillDetails(const FRunItemDefinition& Item);
}
