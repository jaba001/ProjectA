#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunSkillBalanceTypes.h"

struct FRunWeaponSkillRulesState;
struct FCombatRoundSkill;

namespace RunSkillBalance
{
    PROJECTA_API bool Load(FRunWeaponSkillRulesState& State, FText& OutError);
    PROJECTA_API bool LoadFromStrings(FString BalanceCsv, FString ProbabilityCsv, FRunWeaponSkillRulesState& State, FText& OutError, FString StatusCsv = FString());
    // Parse SKILL_STATUS_EFFECTS.csv into status applications keyed by the skill's primary asset ID.
    // SKILL_STATUS_EFFECTS.csv를 스킬 기본 에셋 ID별 상태 부여 목록으로 읽습니다.
    PROJECTA_API bool ParseStatusEffects(FString StatusCsv, TMap<FString, TArray<FCombatStatusApplication>>& OutStatuses, FText& OutError);
    PROJECTA_API bool LoadStatusEffects(TMap<FString, TArray<FCombatStatusApplication>>& OutStatuses, FText& OutError);
    // Battles without a Run (DebugCombat) apply the current table to a resolved skill.
    // Run이 없는 전투(DebugCombat)는 해석한 스킬에 현재 표를 적용합니다.
    PROJECTA_API bool ApplyCurrentStatusEffects(const FPrimaryAssetId& SkillAssetId, FCombatRoundSkill& Skill, FText& OutError);
    PROJECTA_API bool Validate(const FRunWeaponSkillRulesState& State, FText& OutError);
    PROJECTA_API bool IsValid(const FRunSkillBalance& Balance);
    PROJECTA_API bool IsEmpty(const FRunSkillBalance& Balance);
    PROJECTA_API bool Apply(const FRunSkillBalance& Balance, FCombatRoundSkill& Skill, FText& OutError);
    PROJECTA_API FGameplayTag ResolveRarityTag(const FString& Name);
    PROJECTA_API FText RarityName(FGameplayTag Tag);
}
