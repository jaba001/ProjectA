#include "DataAsset/SkillDefinitionDataAsset.h"

FText USkillDefinitionDataAsset::GetActionPointCostText() const
{
    if (ActionPointCost <= 0)
    {
        return NSLOCTEXT("SkillCost", "Invalid", "Invalid AP / AP 오류");
    }

    return FText::Format(NSLOCTEXT("SkillCost", "Amount", "AP {0}"), FText::AsNumber(ActionPointCost));
}

#if WITH_EDITOR
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Misc/DataValidation.h"

EDataValidationResult USkillDefinitionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    if (!UCombatTargetingLibrary::IsSupportedSkillArea(this))
    {
        Context.AddError(NSLOCTEXT("SkillArea", "Unsupported", "Only Single, AroundTarget and AroundSelf with non-negative radius are supported. / 범위는 Single, AroundTarget, AroundSelf와 0 이상의 반경만 지원합니다."));
        return EDataValidationResult::Invalid;
    }
    return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
