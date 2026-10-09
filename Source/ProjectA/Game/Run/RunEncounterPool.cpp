#include "Game/Run/RunEncounterPool.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "Types/GameplayTagCandidateSelection.h"
#include "UObject/TextProperty.h"

namespace
{
    constexpr int32 EncounterOfferCount = 3;
    constexpr int32 MaximumPoolSize = 32;

    bool IsValidText(const FString& Value, int32 MaximumLength, bool bAllowEmpty = false)
    {
        if (Value.Len() > MaximumLength || (!bAllowEmpty && Value.TrimStartAndEnd().IsEmpty())) return false;
        for (const TCHAR Character : Value) if (Character < TEXT(' ') || Character == 0x7f) return false;
        return true;
    }

    bool IsValidId(const FString& Value)
    {
        if (Value.IsEmpty() || Value.Len() > 64 || FName(Value).IsNone()) return false;
        for (const TCHAR Character : Value)
        {
            if ((Character < TEXT('A') || Character > TEXT('Z')) && (Character < TEXT('a') || Character > TEXT('z')) && (Character < TEXT('0') || Character > TEXT('9')) && Character != TEXT('_')) return false;
        }
        return true;
    }

    bool ParseWeight(const TCHAR* Text, float& OutWeight)
    {
        const FString Value = FString(Text).TrimStartAndEnd();
        if (Value.IsEmpty() || Value.Len() > 64) return false;
        bool bDecimal = false;
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            const TCHAR Character = Value[Index];
            if (Character == TEXT('.'))
            {
                if (bDecimal || Index == 0 || Index == Value.Len() - 1) return false;
                bDecimal = true;
            }
            else if (Character < TEXT('0') || Character > TEXT('9')) return false;
        }
        const double Weight = FCString::Atod(*Value);
        if (!FMath::IsFinite(Weight) || Weight > MAX_flt) return false;
        OutWeight = static_cast<float>(Weight);
        return FMath::IsFinite(OutWeight);
    }

    bool ParseEncounterTags(const TCHAR* Text, FGameplayTagContainer& OutTags)
    {
        const FString Value(Text);
        if (Value.IsEmpty()) return true;
        if (Value.Len() > 1024) return false;
        TArray<FString> Names;
        Value.ParseIntoArray(Names, TEXT("|"), false);
        for (const FString& Name : Names)
        {
            const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), false);
            if (!Tag.IsValid() || Tag.ToString() != Name || OutTags.HasTagExact(Tag)) return false;
            OutTags.AddTag(Tag);
        }
        return true;
    }

    bool IsSupportedGroup(FGameplayTag Tag)
    {
        return Tag == FRunEncounterOffer::GetBasicItemShopTag() || Tag == FRunEncounterOffer::GetRarityItemShopTag() || Tag == FRunEncounterOffer::GetTagItemShopTag() || Tag == FRunEncounterOffer::GetRecoveryTag() || Tag == FRunEncounterOffer::GetConsumableShopTag() || Tag == FRunEncounterOffer::GetRevivalTag();
    }

    FGameplayTagQuery BuildItemQuery(const FGameplayTagContainer& Required, const FGameplayTagContainer& Excluded)
    {
        FGameplayTagQueryExpression Root;
        Root.AllExprMatch();
        FGameplayTagQueryExpression All;
        All.AllTagsMatch();
        for (const FGameplayTag Tag : Required) All.AddTag(Tag);
        Root.AddExpr(All);
        if (!Excluded.IsEmpty())
        {
            FGameplayTagQueryExpression None;
            None.NoTagsMatch();
            for (const FGameplayTag Tag : Excluded) None.AddTag(Tag);
            Root.AddExpr(None);
        }
        return FGameplayTagQuery::BuildQuery(Root);
    }

    bool MatchesEncounterQuery(const FRunEncounterOffer& Offer, const FGameplayTagQuery& Query)
    {
        FGameplayTagContainer Tags(Offer.GetResolvedTag());
        if (Offer.SelectionGroupTag.IsValid()) Tags.AddTag(Offer.SelectionGroupTag);
        return Query.IsEmpty() || Query.Matches(Tags);
    }
}

bool RunEncounterPool::Load(FRunTargetState& State, FText& OutError)
{
    FString CsvText;
    if (!FFileHelper::LoadFileToString(CsvText, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/ENCOUNTER_POOL.csv"))))
    {
        OutError = NSLOCTEXT("RunEncounterPool", "MissingCsv", "인카운터 풀 CSV를 읽을 수 없습니다.");
        return false;
    }
    FRunTargetState Candidate = State;
    if (!LoadFromString(MoveTemp(CsvText), 0, Candidate, OutError)) return false;
    Candidate.EncounterSeed = FMath::Rand();
    State = MoveTemp(Candidate);
    return true;
}

bool RunEncounterPool::LoadFromString(FString CsvText, int32 Seed, FRunTargetState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunEncounterPool", "InvalidCsv", "인카운터 CSV의 열·식별자·태그·가중치·상품 조건·진열 정책 또는 활성 후보가 올바르지 않습니다.");
    if (State.SchemaVersion != 1 || State.EncounterSelectionVersion != 0 || State.EncounterSeed != 0 || !State.CompletedEncounterChoices.IsEmpty()) return false;
    const FCsvParser Parser(MoveTemp(CsvText));
    const FCsvParser::FRows& Rows = Parser.GetRows();
    const TCHAR* Headers[] = {TEXT("인카운터 ID"), TEXT("게임 내 이름"), TEXT("상점 종류"), TEXT("속성"), TEXT("분류 태그"), TEXT("판매 대상"), TEXT("활성 여부"), TEXT("그룹 태그"), TEXT("그룹 가중치"), TEXT("변형 가중치"), TEXT("상품 필수 태그"), TEXT("상품 제외 태그"), TEXT("진열 정책"), TEXT("구현 상태"), TEXT("확인 사항")};
    if (Rows.Num() < EncounterOfferCount + 1 || Rows.Num() > 1025 || Rows[0].Num() != UE_ARRAY_COUNT(Headers)) return false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;

    TArray<FRunEncounterOffer> Offers;
    TSet<FName> Ids;
    for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
    {
        const TArray<const TCHAR*>& Row = Rows[RowIndex];
        if (Row.Num() != UE_ARRAY_COUNT(Headers) || !IsValidId(Row[0]) || Ids.Contains(FName(Row[0])) || !IsValidText(Row[1], 128) || !IsValidText(Row[2], 128) || !IsValidText(Row[3], 128, true) || !IsValidText(Row[5], 1024) || !IsValidText(Row[13], 256) || !IsValidText(Row[14], 2048)) return false;
        Ids.Add(FName(Row[0]));
        const FString Enabled(Row[6]);
        if (Enabled != TEXT("0") && Enabled != TEXT("1")) return false;
        const bool bEnabled = Enabled == TEXT("1");
        FRunEncounterOffer Offer;
        Offer.EncounterId = FName(Row[0]);
        Offer.DisplayName = FText::FromString(Row[1]);
        Offer.EncounterTag = FGameplayTag::RequestGameplayTag(FName(Row[4]), false);
        if (!Offer.EncounterTag.IsValid() || Offer.EncounterTag.ToString() != Row[4] || !Offer.IsSupportedEncounter()) return false;
        const FString GroupName(Row[7]);
        if (!GroupName.IsEmpty())
        {
            Offer.SelectionGroupTag = FGameplayTag::RequestGameplayTag(FName(GroupName), false);
            if (!Offer.SelectionGroupTag.IsValid() || Offer.SelectionGroupTag.ToString() != GroupName) return false;
        }
        if ((bEnabled || Row[8][0] != TEXT('\0')) && !ParseWeight(Row[8], Offer.GroupWeight)) return false;
        if ((bEnabled || Row[9][0] != TEXT('\0')) && !ParseWeight(Row[9], Offer.VariantWeight)) return false;
        FGameplayTagContainer Required;
        FGameplayTagContainer Excluded;
        if (!ParseEncounterTags(Row[10], Required) || !ParseEncounterTags(Row[11], Excluded) || Required.HasAnyExact(Excluded)) return false;
        const FString StockPolicy(Row[12]);
        if (StockPolicy != TEXT("기본5") && StockPolicy != TEXT("최대5") && StockPolicy != TEXT("해당없음") && StockPolicy != TEXT("보존")) return false;
        if (!bEnabled) continue;
        if (Offer.IsItemShop())
        {
            if (Required.IsEmpty() || (StockPolicy != TEXT("기본5") && StockPolicy != TEXT("최대5"))) return false;
            Offer.ItemQuery = BuildItemQuery(Required, Excluded);
            Offer.ItemStockPolicyVersion = StockPolicy == TEXT("최대5") ? 1 : 0;
        }
        else if (!Required.IsEmpty() || !Excluded.IsEmpty() || StockPolicy != TEXT("해당없음")) return false;
        Offers.Add(MoveTemp(Offer));
    }
    FRunTargetState Candidate = State;
    Candidate.EncounterSelectionVersion = 1;
    Candidate.EncounterSeed = Seed;
    Candidate.EncounterPool = MoveTemp(Offers);
    if (!Validate(Candidate, OutError)) return false;
    State = MoveTemp(Candidate);
    OutError = FText::GetEmpty();
    return true;
}

bool RunEncounterPool::Validate(const FRunTargetState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunEncounterPool", "InvalidState", "저장된 인카운터 정책·태그·그룹 가중치·상품 조건 또는 선택 가능한 후보 수가 올바르지 않습니다.");
    if (State.EncounterSelectionVersion < 0 || State.EncounterSelectionVersion > 1 || State.EncounterPool.Num() > MaximumPoolSize) return false;
    const bool bWeighted = State.EncounterSelectionVersion == 1;
    if (bWeighted ? State.SchemaVersion != 1 || State.EncounterPool.Num() < EncounterOfferCount : State.EncounterSeed != 0) return false;
    TSet<FName> Ids;
    TMap<FGameplayTag, float> GroupWeights;
    int32 EligibleCount = 0;
    for (const FRunEncounterOffer& Offer : State.EncounterPool)
    {
        if (Offer.EncounterId.IsNone() || Ids.Contains(Offer.EncounterId) || Offer.DisplayName.IsEmpty() || !Offer.EncounterTag.IsValid() || !Offer.IsSupportedEncounter()) return false;
        Ids.Add(Offer.EncounterId);
        if (!bWeighted)
        {
            if (!Offer.ItemQuery.IsEmpty() || Offer.ItemStockPolicyVersion != 0 || Offer.SelectionGroupTag.IsValid() || Offer.GroupWeight != 0.0f || Offer.VariantWeight != 0.0f) return false;
            continue;
        }
        if (!IsValidId(Offer.EncounterId.ToString()) || !IsValidText(Offer.DisplayName.ToString(), 128) || !IsSupportedGroup(Offer.SelectionGroupTag) || Offer.GetResolvedTag() != Offer.SelectionGroupTag || !FMath::IsFinite(Offer.GroupWeight) || Offer.GroupWeight < 0.0f || !FMath::IsFinite(Offer.VariantWeight) || Offer.VariantWeight < 0.0f) return false;
        const float* ExistingWeight = GroupWeights.Find(Offer.SelectionGroupTag);
        if (ExistingWeight && *ExistingWeight != Offer.GroupWeight) return false;
        GroupWeights.Add(Offer.SelectionGroupTag, Offer.GroupWeight);
        if (Offer.IsItemShop())
        {
            const int32 ExpectedPolicy = Offer.SelectionGroupTag == FRunEncounterOffer::GetBasicItemShopTag() ? 0 : 1;
            if (Offer.ItemQuery.IsEmpty() || Offer.ItemStockPolicyVersion != ExpectedPolicy) return false;
        }
        else if (!Offer.IsService() || !Offer.ItemQuery.IsEmpty() || Offer.ItemStockPolicyVersion != 0) return false;
        if (Offer.GroupWeight > 0.0f && Offer.VariantWeight > 0.0f && MatchesEncounterQuery(Offer, State.EncounterQuery)) ++EligibleCount;
    }
    if (bWeighted && EligibleCount < EncounterOfferCount) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool RunEncounterPool::Select(const FRunTargetState& State, int32 CombatIndex, int32 VisitIndex, TArray<FRunEncounterOffer>& OutOffers, FText& OutError)
{
    if (!Validate(State, OutError)) return false;
    OutError = NSLOCTEXT("RunEncounterPool", "InvalidSelection", "가중치 인카운터를 제시할 수 있는 방문 경계 또는 후보가 아닙니다.");
    if (State.EncounterSelectionVersion != 1 || CombatIndex < 0 || CombatIndex >= 20 || VisitIndex < 0 || VisitIndex >= 3) return false;
    TArray<FGameplayTag> Groups;
    TArray<FGameplayTagWeightedCandidate> GroupCandidates;
    TArray<TArray<int32>> GroupEntries;
    for (int32 Index = 0; Index < State.EncounterPool.Num(); ++Index)
    {
        const FRunEncounterOffer& Offer = State.EncounterPool[Index];
        if (Offer.GroupWeight <= 0.0f || Offer.VariantWeight <= 0.0f || !MatchesEncounterQuery(Offer, State.EncounterQuery)) continue;
        int32 GroupIndex = Groups.IndexOfByKey(Offer.SelectionGroupTag);
        if (GroupIndex == INDEX_NONE)
        {
            GroupIndex = Groups.Add(Offer.SelectionGroupTag);
            FGameplayTagWeightedCandidate& Group = GroupCandidates.AddDefaulted_GetRef();
            Group.Tags.AddTag(Offer.SelectionGroupTag);
            Group.BaseWeight = Offer.GroupWeight;
            GroupEntries.AddDefaulted();
        }
        GroupEntries[GroupIndex].Add(Index);
    }
    // Mix the frozen Run seed with the visit index using fixed integer operations, without consuming global RNG.
    // 전역 난수를 소비하지 않고 고정 정수 연산으로 저장된 Run 시드와 방문 번호를 결합합니다.
    uint32 VisitSeed = static_cast<uint32>(State.EncounterSeed) ^ (0x9e3779b9u * static_cast<uint32>(CombatIndex * 3 + VisitIndex + 1));
    VisitSeed ^= VisitSeed >> 16;
    VisitSeed *= 0x85ebca6bu;
    VisitSeed ^= VisitSeed >> 13;
    FRandomStream Random(static_cast<int32>(VisitSeed));
    TArray<FRunEncounterOffer> Offers;
    for (int32 Selection = 0; Selection < EncounterOfferCount; ++Selection)
    {
        TArray<int32> SelectedGroups;
        if (!GameplayTagCandidateSelection::Select(GroupCandidates, FGameplayTagQuery(), 1, false, Random, SelectedGroups)) return false;
        const int32 GroupIndex = SelectedGroups[0];
        TArray<int32>& Entries = GroupEntries[GroupIndex];
        TArray<FGameplayTagWeightedCandidate> Variants;
        Variants.Reserve(Entries.Num());
        for (const int32 Index : Entries)
        {
            FGameplayTagWeightedCandidate& Variant = Variants.AddDefaulted_GetRef();
            Variant.Tags.AddTag(State.EncounterPool[Index].GetResolvedTag());
            Variant.BaseWeight = State.EncounterPool[Index].VariantWeight;
        }
        TArray<int32> SelectedVariants;
        if (!GameplayTagCandidateSelection::Select(Variants, State.EncounterQuery, 1, false, Random, SelectedVariants)) return false;
        Offers.Add(State.EncounterPool[Entries[SelectedVariants[0]]]);
        Entries.RemoveAt(SelectedVariants[0]);
        if (Entries.IsEmpty()) GroupCandidates[GroupIndex].BaseWeight = 0.0f;
    }
    OutOffers = MoveTemp(Offers);
    OutError = FText::GetEmpty();
    return true;
}

bool RunEncounterPool::IsSameOffer(const FRunEncounterOffer& Left, const FRunEncounterOffer& Right)
{
    // CSV FText keys may change on SaveGame serialization; compare the displayed text and every remaining field.
    // CSV 문구의 FText 키는 저장 직렬화에서 바뀔 수 있으므로 표시값과 나머지 모든 필드를 비교합니다.
    if (!FTextProperty::Identical_Implementation(Left.DisplayName, Right.DisplayName, 0, FTextProperty::EIdenticalLexicalCompareMethod::DisplayString)) return false;
    FRunEncounterOffer Comparable = Right;
    Comparable.DisplayName = Left.DisplayName;
    return FRunEncounterOffer::StaticStruct()->CompareScriptStruct(&Left, &Comparable, 0);
}
