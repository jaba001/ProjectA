#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"

class USkillDefinitionDataAsset;

namespace RunItemPresentation
{
    struct FItemSkillDetails
    {
        FSoftObjectPath SkillAsset;
        FText Name;
        FText Description;
        FText Stats;
        FLinearColor Color = FLinearColor::White;
    };

    FText SlotName(FGameplayTag Slot);
    const FRunWeaponRarityRule* FindRarity(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText Name(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText SkillName(const USkillDefinitionDataAsset* Skill);
    FText SkillDescription(const USkillDefinitionDataAsset* Skill);
    PROJECTA_API FText GrantedSkills(const FRunItemDefinition& Item, bool bIncludeStats = true);
    FText Tooltip(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities);
    FText EquipmentDescription(const FRunItemDefinition& Item);
    // Build display values during a view refresh, never from a hover tick or a new random selection.
    // hover Tick이나 새 추첨 대신 화면 갱신 시 표시할 값을 작성합니다.
    PROJECTA_API TArray<FItemSkillDetails> SkillDetails(const FRunItemDefinition& Item);
}
