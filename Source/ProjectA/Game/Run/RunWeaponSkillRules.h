#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunWeaponSkillTypes.h"

struct FRunItemDefinition;

namespace RunWeaponSkillRules
{
    PROJECTA_API bool Validate(const FRunWeaponSkillRulesState& State, FText& OutError);
    // Query eligibility only after validating the owning frozen rule state.
    // 소유하는 고정 규칙 상태를 검증한 뒤 후보 적합성을 조회합니다.
    PROJECTA_API bool CanGenerate(const FRunItemDefinition& BaseItem, const FRunWeaponSkillRulesState& State);
    PROJECTA_API bool Generate(const FRunItemDefinition& BaseItem, const FRunWeaponSkillRulesState& State, FRandomStream& Random, FRunItemDefinition& OutCopy, FText& OutError);
    // Validate the stored result without rerolling; validate the owning Run's frozen rule state separately.
    // 재추첨 없이 저장 결과를 검사하며 소유 Run의 고정 규칙 상태는 별도로 검증합니다.
    PROJECTA_API bool ValidateGeneratedCopy(const FRunItemDefinition& Item, const FRunWeaponSkillRulesState& State, FText& OutError);
}
