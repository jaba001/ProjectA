#include "UI/Gameplay/RunItemPresentation.h"

#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunSkillBalance.h"

const FRunWeaponRarityRule* RunItemPresentation::FindRarity(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    if (Item.GenerationVersion == 0) return nullptr;
    return Rarities.FindByPredicate([&Item](const FRunWeaponRarityRule& Rule) { return Rule.RarityTag == Item.RarityTag; });
}

FText RunItemPresentation::Name(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    const FText Name = Item.DisplayName.IsEmpty() ? FText::FromString(Item.Asset.GetAssetName()) : Item.DisplayName;
    const FRunWeaponRarityRule* Rarity = FindRarity(Item, Rarities);
    return Rarity ? FText::Format(NSLOCTEXT("RunItem", "RarityName", "[{0}] {1}"), Rarity->DisplayName, Name) : Name;
}

FText RunItemPresentation::GrantedSkills(const FRunItemDefinition& Item)
{
    if (Item.GenerationVersion == 0 || Item.GrantedSkills.IsEmpty()) return FText::GetEmpty();
    TArray<FText> Names;
    for (int32 Index = 0; Index < Item.GrantedSkills.Num(); ++Index)
    {
        const FSoftObjectPath& Path = Item.GrantedSkills[Index];
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
        FText Name = Skill ? Skill->SkillName.IsEmpty() ? FText::FromName(Skill->SkillId) : Skill->SkillName : NSLOCTEXT("RunItem", "MissingSkill", "스킬 정보를 불러올 수 없습니다.");
        if (Item.SkillBalanceVersion == 1 && Item.GrantedSkillBalances.IsValidIndex(Index))
        {
            const FRunSkillBalance& Balance = Item.GrantedSkillBalances[Index];
            Name = FText::Format(NSLOCTEXT("RunItem", "BalancedSkill", "[{0}] {1} · 위력 {2} · AP {3} / SAP {4}"), RunSkillBalance::RarityName(Balance.RarityTag), Name, FText::AsNumber(Balance.Power), FText::AsNumber(Balance.ActionPointCost), FText::AsNumber(Balance.SubActionPointCost));
        }
        Names.Add(Name);
    }
    return FText::Format(NSLOCTEXT("RunItem", "GrantedSkills", "장착 스킬: {0}"), FText::Join(FText::FromString(TEXT(" · ")), Names));
}

FText RunItemPresentation::Tooltip(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    const FText Skills = GrantedSkills(Item);
    return Skills.IsEmpty() ? Name(Item, Rarities) : FText::Format(NSLOCTEXT("RunItem", "SkillTooltip", "{0}\n{1}"), Name(Item, Rarities), Skills);
}
