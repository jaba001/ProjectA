#include "DataAsset/SkillPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

TArray<USkillDefinitionDataAsset*> USkillPoolDataAsset::GetValidSkills() const
{
    TArray<USkillDefinitionDataAsset*> Result;

    for (const FSkillPoolEntry& Entry : Entries)
    {
        if (!Entry.Skill)
        {
            continue;
        }

        Result.Add(Entry.Skill);
    }

    return Result;
}

USkillDefinitionDataAsset* USkillPoolDataAsset::GetRandomSkill() const
{
    int32 TotalWeight = 0;

    for (const FSkillPoolEntry& Entry : Entries)
    {
        if (!Entry.Skill || Entry.Weight <= 0)
        {
            continue;
        }

        TotalWeight += Entry.Weight;
    }

    if (TotalWeight <= 0)
    {
        return nullptr;
    }

    int32 RandomValue = FMath::RandRange(1, TotalWeight);

    for (const FSkillPoolEntry& Entry : Entries)
    {
        if (!Entry.Skill || Entry.Weight <= 0)
        {
            continue;
        }

        RandomValue -= Entry.Weight;

        if (RandomValue <= 0)
        {
            return Entry.Skill;
        }
    }

    return nullptr;
}
