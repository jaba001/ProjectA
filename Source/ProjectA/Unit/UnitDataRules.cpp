#include "Unit/UnitDataRules.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

bool UnitDataRules::IsValidMaxHP(float MaxHP)
{
    return FMath::IsFinite(MaxHP) && MaxHP > 0.0f && MaxHP <= MaxStatValue;
}

bool UnitDataRules::IsValidHealth(float MaxHP, float CurrentHP)
{
    return IsValidMaxHP(MaxHP) && FMath::IsFinite(CurrentHP) && CurrentHP >= 0.0f && CurrentHP <= MaxHP;
}

bool UnitDataRules::IsValidAttribute(float Value)
{
    return FMath::IsFinite(Value) && Value >= 0.0f && Value <= MaxStatValue;
}

bool UnitDataRules::IsValidAttributes(float Strength, float Dexterity, float Intelligence)
{
    return IsValidAttribute(Strength) && IsValidAttribute(Dexterity) && IsValidAttribute(Intelligence);
}

bool UnitDataRules::IsValidActionPoints(int32 AP, int32 SubAP)
{
    return AP >= 1 && AP <= MaxActionPoints && SubAP >= 0 && SubAP <= MaxActionPoints;
}

bool UnitDataRules::IsValidMoveRange(int32 MoveRange)
{
    return MoveRange >= 0 && MoveRange <= MaxMoveRange;
}

bool UnitDataRules::IsValidSkillCount(int32 Count, bool bRequireSkill)
{
    return Count >= (bRequireSkill ? 1 : 0) && Count <= MaxSkills;
}

bool UnitDataRules::ValidateSkills(const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills, bool bRequireSkill, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (!IsValidSkillCount(Skills.Num(), bRequireSkill))
    {
        OutError = NSLOCTEXT("UnitDataRules", "SkillCount", "The skill loadout is empty or exceeds its capacity. / 스킬 장착이 비어 있거나 허용 개수를 초과했습니다.");
        return false;
    }
    TSet<FName> SkillIds;
    for (int32 Index = 0; Index < Skills.Num(); ++Index)
    {
        USkillDefinitionDataAsset* Skill = Skills[Index];
        if (!IsValid(Skill))
        {
            OutError = FText::Format(NSLOCTEXT("UnitDataRules", "MissingSkill", "StartingSkills[{0}] is missing or invalid. / StartingSkills[{0}] 참조가 없거나 유효하지 않습니다."), FText::AsNumber(Index));
            return false;
        }
        FCombatRoundSkill Definition;
        if (!Skill->ResolveRoundSkill(Definition, OutError)) return false;
        if (SkillIds.Contains(Definition.SkillId))
        {
            OutError = FText::Format(NSLOCTEXT("UnitDataRules", "DuplicateSkill", "Skill asset ID is duplicated: {0}. / 스킬 에셋 ID가 중복됩니다: {0}."), FText::FromName(Definition.SkillId));
            return false;
        }
        SkillIds.Add(Definition.SkillId);
    }
    return true;
}
