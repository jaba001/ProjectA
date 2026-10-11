#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

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
    FGameplayTagContainer Tags;
};

// Empty queries select all entries; specific queries inspect the actual content tags.
// 빈 쿼리는 전체 항목을 선택하며 개별 쿼리는 실제 콘텐츠 태그를 검사합니다.
struct FEncyclopediaSubcategory
{
    FText Label;
    FGameplayTagQuery Query;
};

namespace EncyclopediaCatalog
{
    FSoftObjectPath DefaultPartyPath();
    void GetSkillPaths(TArray<FSoftObjectPath>& OutPaths);
    FText CategoryName(EEncyclopediaCategory Category);
    void GetSubcategories(EEncyclopediaCategory Category, const TArray<FEncyclopediaEntry>& Entries, TArray<FEncyclopediaSubcategory>& OutSubcategories);
    void Build(const UPartyDefinitionDataAsset* Party, const TArray<FSoftObjectPath>& SkillPaths, TArray<FEncyclopediaEntry>& OutEntries);
}
