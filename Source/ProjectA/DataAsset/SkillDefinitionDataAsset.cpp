#include "DataAsset/SkillDefinitionDataAsset.h"

FText USkillDefinitionDataAsset::GetActionPointCostText() const
{
    if (ActionPointCost <= 0)
    {
        return NSLOCTEXT("SkillCost", "Invalid", "Invalid AP / AP 오류");
    }

    return FText::Format(NSLOCTEXT("SkillCost", "Amount", "AP {0}"), FText::AsNumber(ActionPointCost));
}

