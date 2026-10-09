#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunSkillBalanceTypes.h"

struct FRunWeaponSkillRulesState;
struct FCombatRoundSkill;

namespace RunSkillBalance
{
    PROJECTA_API bool Load(FRunWeaponSkillRulesState& State, FText& OutError);
    PROJECTA_API bool LoadFromStrings(FString BalanceCsv, FString ProbabilityCsv, FRunWeaponSkillRulesState& State, FText& OutError);
    PROJECTA_API bool Validate(const FRunWeaponSkillRulesState& State, FText& OutError);
    PROJECTA_API bool IsValid(const FRunSkillBalance& Balance);
    PROJECTA_API bool IsEmpty(const FRunSkillBalance& Balance);
    PROJECTA_API bool Apply(const FRunSkillBalance& Balance, FCombatRoundSkill& Skill, FText& OutError);
    PROJECTA_API FGameplayTag ResolveRarityTag(const FString& Name);
    PROJECTA_API FText RarityName(FGameplayTag Tag);
}
