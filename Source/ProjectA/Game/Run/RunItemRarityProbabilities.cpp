#include "Game/Run/RunItemRarityProbabilities.h"

#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "Types/GameplayTagCandidateSelection.h"

namespace
{
    constexpr int32 SupportedRarityCount = 5;
    constexpr int32 TotalBasisPoints = 10000;

    // Parse two decimal places as integer basis points, without floating-point rounding or exponent syntax.
    // 부동소수점 반올림이나 지수 표기 없이 소수 둘째 자리까지 정수 기준점으로 해석합니다.
    bool ParseProbability(const TCHAR* Text, int32& OutBasisPoints)
    {
        const FString Value = FString(Text).TrimStartAndEnd();
        if (Value.IsEmpty()) return false;
        int32 Whole = 0;
        int32 Fraction = 0;
        int32 FractionDigits = 0;
        bool bDecimal = false;
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            const TCHAR Character = Value[Index];
            if (Character == TEXT('.'))
            {
                if (bDecimal || Index == 0 || Index == Value.Len() - 1) return false;
                bDecimal = true;
                continue;
            }
            if (Character < TEXT('0') || Character > TEXT('9')) return false;
            const int32 Digit = Character - TEXT('0');
            if (bDecimal)
            {
                if (++FractionDigits > 2) return false;
                Fraction = Fraction * 10 + Digit;
            }
            else
            {
                if (Whole > 10 || (Whole == 10 && Digit > 0)) return false;
                Whole = Whole * 10 + Digit;
            }
        }
        if (FractionDigits == 1) Fraction *= 10;
        const int32 BasisPoints = Whole * 100 + Fraction;
        if (BasisPoints > TotalBasisPoints) return false;
        OutBasisPoints = BasisPoints;
        return true;
    }

    bool IsValidNote(const TCHAR* Text)
    {
        const FString Note(Text);
        if (Note.Len() > 256 || Note.TrimStartAndEnd().IsEmpty()) return false;
        for (const TCHAR Character : Note) if (Character < TEXT(' ') || Character == 0x7f) return false;
        return true;
    }

    // Share eligibility between selection, specialized stock size and frozen-stock validation.
    // 추첨·전문 상점 상품 수·저장된 진열 검증에서 같은 적격 조건을 사용합니다.
    bool BuildSelectionData(const TArray<FRunItemDefinition>& Catalog, const FRunItemRarityProbabilityState& State, const FGameplayTagQuery& Query, TArray<FGameplayTagWeightedCandidate>& OutCandidates, TArray<int32>& OutEligibleIndices, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules)
    {
        if (!RunItemRarityProbabilities::Validate(State, OutError) || (WeaponSkillRules && !RunWeaponSkillRules::Validate(*WeaponSkillRules, OutError))) return false;
        OutError = NSLOCTEXT("ItemRarityProbabilities", "InvalidCatalog", "등급별 추첨에는 유효한 원본 아이템 카탈로그와 양수 선택 개수가 필요합니다.");
        if (Catalog.IsEmpty()) return false;
        TSet<FSoftObjectPath> Assets;
        OutCandidates.Reserve(Catalog.Num());
        for (int32 Index = 0; Index < Catalog.Num(); ++Index)
        {
            const FRunItemDefinition& Item = Catalog[Index];
            if (!RunItemShopCatalog::ValidateItem(Item) || Item.GenerationVersion != 0 || Assets.Contains(Item.Asset)) return false;
            const FRunItemRarityProbability* Rarity = State.Entries.FindByPredicate([&Item](const FRunItemRarityProbability& Entry) { return Entry.RarityTag == Item.CatalogRarityTag; });
            if (State.SchemaVersion == 1 && !Rarity) return false;
            Assets.Add(Item.Asset);
            FGameplayTagWeightedCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
            Candidate.Tags = Item.Tags;
            if (State.SchemaVersion == 1) Candidate.Tags.AddTag(Item.CatalogRarityTag);
            Candidate.BaseWeight = WeaponSkillRules && WeaponSkillRules->SchemaVersion == 1 && !RunWeaponSkillRules::CanGenerate(Item, *WeaponSkillRules) ? 0.0f : 1.0f;
            if (Candidate.BaseWeight > 0.0f && (Query.IsEmpty() || Query.Matches(Candidate.Tags)) && (!Rarity || Rarity->ProbabilityBasisPoints > 0)) OutEligibleIndices.Add(Index);
        }
        OutError = FText::GetEmpty();
        return true;
    }
}

bool RunItemRarityProbabilities::Load(FRunItemRarityProbabilityState& OutState, FText& OutError)
{
    FString CsvText;
    if (!FFileHelper::LoadFileToString(CsvText, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/ITEM_RARITY_PROBABILITIES.csv"))))
    {
        OutError = NSLOCTEXT("ItemRarityProbabilities", "MissingCsv", "아이템 등급 확률 CSV를 읽을 수 없습니다.");
        return false;
    }
    return LoadFromString(MoveTemp(CsvText), OutState, OutError);
}

bool RunItemRarityProbabilities::LoadFromString(FString CsvText, FRunItemRarityProbabilityState& OutState, FText& OutError)
{
    OutError = NSLOCTEXT("ItemRarityProbabilities", "InvalidCsv", "아이템 등급 확률 CSV의 열·5개 등급·태그·적용 범위·비고 또는 확률이 올바르지 않습니다.");
    const FCsvParser Parser(MoveTemp(CsvText));
    const FCsvParser::FRows& Rows = Parser.GetRows();
    if (Rows.Num() != SupportedRarityCount + 1 || Rows[0].Num() != 5) return false;
    const TCHAR* Headers[] = {TEXT("등급"), TEXT("등급 태그"), TEXT("확률(%)"), TEXT("적용 범위"), TEXT("비고")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;

    FRunItemRarityProbabilityState State;
    State.SchemaVersion = 1;
    for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
    {
        const TArray<const TCHAR*>& Row = Rows[RowIndex];
        if (Row.Num() != UE_ARRAY_COUNT(Headers)) return false;
        const FGameplayTag Tag = RunItemShopCatalog::ResolveRarityTag(Row[0]);
        const FString Scope(Row[3]);
        if (!Tag.IsValid() || Tag.ToString() != Row[1] || (Scope != TEXT("아이템상점") && Scope != TEXT("아이템상점·전투보상")) || !IsValidNote(Row[4])) return false;
        FRunItemRarityProbability Entry;
        Entry.RarityTag = Tag;
        if (!ParseProbability(Row[2], Entry.ProbabilityBasisPoints)) return false;
        State.Entries.Add(Entry);
    }
    if (!Validate(State, OutError)) return false;
    OutState = MoveTemp(State);
    OutError = FText::GetEmpty();
    return true;
}

bool RunItemRarityProbabilities::Validate(const FRunItemRarityProbabilityState& State, FText& OutError)
{
    OutError = NSLOCTEXT("ItemRarityProbabilities", "InvalidState", "저장된 아이템 등급 확률의 버전·5개 등급 또는 100% 합계가 올바르지 않습니다.");
    if (State.SchemaVersion == 0)
    {
        if (!State.Entries.IsEmpty()) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (State.SchemaVersion != 1 || State.Entries.Num() != SupportedRarityCount) return false;
    TSet<FGameplayTag> Tags;
    int32 Total = 0;
    for (const FRunItemRarityProbability& Entry : State.Entries)
    {
        if (!RunItemShopCatalog::IsSupportedRarityTag(Entry.RarityTag) || Tags.Contains(Entry.RarityTag) || Entry.ProbabilityBasisPoints < 0 || Entry.ProbabilityBasisPoints > TotalBasisPoints) return false;
        Tags.Add(Entry.RarityTag);
        Total += Entry.ProbabilityBasisPoints;
    }
    if (Total != TotalBasisPoints) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool RunItemRarityProbabilities::GetEligibleIndices(const TArray<FRunItemDefinition>& Catalog, const FRunItemRarityProbabilityState& State, const FGameplayTagQuery& Query, TArray<int32>& OutIndices, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules)
{
    TArray<FGameplayTagWeightedCandidate> Candidates;
    TArray<int32> EligibleIndices;
    if (!BuildSelectionData(Catalog, State, Query, Candidates, EligibleIndices, OutError, WeaponSkillRules)) return false;
    OutIndices = MoveTemp(EligibleIndices);
    return true;
}

bool RunItemRarityProbabilities::Select(const TArray<FRunItemDefinition>& Catalog, const FRunItemRarityProbabilityState& State, const FGameplayTagQuery& Query, int32 Count, bool bAllowDuplicates, FRandomStream& Random, TArray<int32>& OutIndices, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules)
{
    TArray<FGameplayTagWeightedCandidate> Candidates;
    TArray<int32> EligibleIndices;
    if (!BuildSelectionData(Catalog, State, Query, Candidates, EligibleIndices, OutError, WeaponSkillRules)) return false;
    OutError = NSLOCTEXT("ItemRarityProbabilities", "InsufficientCandidates", "태그·등급 확률·스킬 조건을 만족하는 아이템 후보가 부족합니다.");
    if (Count <= 0 || EligibleIndices.IsEmpty() || (!bAllowDuplicates && EligibleIndices.Num() < Count)) return false;
    FRandomStream SelectionRandom = Random;
    TArray<int32> SelectedIndices;
    if (State.SchemaVersion == 0)
    {
        if (!GameplayTagCandidateSelection::Select(Candidates, Query, Count, bAllowDuplicates, SelectionRandom, SelectedIndices)) return false;
    }
    else
    {
        TArray<TArray<int32>> RarityItems;
        TArray<FGameplayTagWeightedCandidate> RarityCandidates;
        RarityItems.SetNum(State.Entries.Num());
        RarityCandidates.SetNum(State.Entries.Num());
        for (const int32 Index : EligibleIndices)
        {
            const int32 RarityIndex = State.Entries.IndexOfByPredicate([&Catalog, Index](const FRunItemRarityProbability& Entry) { return Entry.RarityTag == Catalog[Index].CatalogRarityTag; });
            RarityItems[RarityIndex].Add(Index);
        }
        for (int32 RarityIndex = 0; RarityIndex < State.Entries.Num(); ++RarityIndex)
        {
            RarityCandidates[RarityIndex].Tags.AddTag(State.Entries[RarityIndex].RarityTag);
            RarityCandidates[RarityIndex].BaseWeight = RarityItems[RarityIndex].IsEmpty() ? 0.0f : State.Entries[RarityIndex].ProbabilityBasisPoints;
        }
        for (int32 Selection = 0; Selection < Count; ++Selection)
        {
            TArray<int32> SelectedRarity;
            if (!GameplayTagCandidateSelection::Select(RarityCandidates, FGameplayTagQuery(), 1, false, SelectionRandom, SelectedRarity)) return false;
            const int32 RarityIndex = SelectedRarity[0];
            TArray<int32>& EligibleItems = RarityItems[RarityIndex];
            TArray<FGameplayTagWeightedCandidate> ItemCandidates;
            ItemCandidates.Reserve(EligibleItems.Num());
            for (const int32 ItemIndex : EligibleItems) ItemCandidates.Add(Candidates[ItemIndex]);
            TArray<int32> SelectedItem;
            if (!GameplayTagCandidateSelection::Select(ItemCandidates, Query, 1, false, SelectionRandom, SelectedItem)) return false;
            SelectedIndices.Add(EligibleItems[SelectedItem[0]]);
            if (!bAllowDuplicates)
            {
                EligibleItems.RemoveAt(SelectedItem[0]);
                if (EligibleItems.IsEmpty()) RarityCandidates[RarityIndex].BaseWeight = 0.0f;
            }
        }
    }
    OutIndices = MoveTemp(SelectedIndices);
    Random = SelectionRandom;
    OutError = FText::GetEmpty();
    return true;
}
