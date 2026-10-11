#pragma once

#include "CoreMinimal.h"

class UPartyDefinitionDataAsset;

enum class EEncyclopediaCategory : uint8
{
    Equipment,
    Skills,
    Professions,
    Statuses,
    Formulas,
    Run,
    Monsters,
    Count
};

// Read-only presentation values never own or modify a running party or its saved state.
// 읽기 전용 표시 값은 진행 중 파티나 저장 상태를 소유하거나 변경하지 않습니다.
struct FEncyclopediaEntry
{
    EEncyclopediaCategory Category = EEncyclopediaCategory::Equipment;
    FText Title;
    FText Body;
};

namespace EncyclopediaCatalog
{
    FSoftObjectPath DefaultPartyPath();
    void GetSkillPaths(TArray<FSoftObjectPath>& OutPaths);
    FText CategoryName(EEncyclopediaCategory Category);
    void Build(const UPartyDefinitionDataAsset* Party, const TArray<FSoftObjectPath>& SkillPaths, TArray<FEncyclopediaEntry>& OutEntries);
}
