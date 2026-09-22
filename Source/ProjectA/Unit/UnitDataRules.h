#pragma once

#include "CoreMinimal.h"

class USkillDefinitionDataAsset;

// Share content limits across authoring, live units and serialized snapshots without actor dependencies.
// 액터 의존성 없이 제작 데이터, 실제 유닛과 저장 Snapshot의 콘텐츠 제한을 공유합니다.
namespace UnitDataRules
{
    inline constexpr float MaxStatValue = 1000000.0f;
    inline constexpr int32 MaxActionPoints = 100;
    inline constexpr int32 MaxMoveRange = 32;
    inline constexpr int32 MaxSkills = 5;

    PROJECTA_API bool IsValidMaxHP(float MaxHP);
    PROJECTA_API bool IsValidHealth(float MaxHP, float CurrentHP);
    PROJECTA_API bool IsValidAttribute(float Value);
    PROJECTA_API bool IsValidAttributes(float Strength, float Dexterity, float Intelligence);
    PROJECTA_API bool IsValidActionPoints(int32 AP, int32 SubAP);
    PROJECTA_API bool IsValidMoveRange(int32 MoveRange);
    PROJECTA_API bool IsValidSkillCount(int32 Count, bool bRequireSkill);
    PROJECTA_API bool ValidateSkills(const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills, bool bRequireSkill, FText& OutError);
}
