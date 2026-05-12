#include "DataAsset/SkillDatabaseDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

USkillDefinitionDataAsset* USkillDatabaseDataAsset::FindSkillById(FName SkillId) const
{
    if (SkillId.IsNone())
    {
        return nullptr;
    }

    for (USkillDefinitionDataAsset* Skill : AllSkills)
    {
        if (!Skill)
        {
            continue;
        }

        if (Skill->SkillId == SkillId)
        {
            return Skill;
        }
    }

    return nullptr;
}

bool USkillDatabaseDataAsset::ContainsSkill(USkillDefinitionDataAsset* Skill) const
{
    if (!Skill)
    {
        return false;
    }

    return AllSkills.Contains(Skill);
}

TArray<USkillDefinitionDataAsset*> USkillDatabaseDataAsset::GetAllValidSkills() const
{
    TArray<USkillDefinitionDataAsset*> Result;

    for (USkillDefinitionDataAsset* Skill : AllSkills)
    {
        if (!Skill)
        {
            continue;
        }

        Result.Add(Skill);
    }

    return Result;
}
