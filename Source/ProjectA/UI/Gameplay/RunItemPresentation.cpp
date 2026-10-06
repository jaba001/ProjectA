#include "UI/Gameplay/RunItemPresentation.h"

#include "DataAsset/SkillDefinitionDataAsset.h"

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
    for (const FSoftObjectPath& Path : Item.GrantedSkills)
    {
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
        Names.Add(Skill ? Skill->SkillName.IsEmpty() ? FText::FromName(Skill->SkillId) : Skill->SkillName : NSLOCTEXT("RunItem", "MissingSkill", "스킬 정보를 불러올 수 없습니다."));
    }
    return FText::Format(NSLOCTEXT("RunItem", "GrantedSkills", "장착 스킬: {0}"), FText::Join(FText::FromString(TEXT(" · ")), Names));
}

FText RunItemPresentation::Tooltip(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    const FText Skills = GrantedSkills(Item);
    return Skills.IsEmpty() ? Name(Item, Rarities) : FText::Format(NSLOCTEXT("RunItem", "SkillTooltip", "{0}\n{1}"), Name(Item, Rarities), Skills);
}
