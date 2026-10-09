#include "Game/Run/RunSkillBalance.h"

#include "Combat/Round/CombatRoundTypes.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunWeaponSkillTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/Csv/CsvParser.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillRarityWhite, "Selection.WeaponSkill.Rarity.White");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillRarityGreen, "Selection.WeaponSkill.Rarity.Green");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillRarityBlue, "Selection.WeaponSkill.Rarity.Blue");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillRarityPurple, "Selection.WeaponSkill.Rarity.Purple");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillRarityOrange, "Selection.WeaponSkill.Rarity.Orange");

namespace RunSkillBalanceInternal
{
    TArray<FGameplayTag> Rarities()
    {
        return {TAG_SkillRarityWhite, TAG_SkillRarityGreen, TAG_SkillRarityBlue, TAG_SkillRarityPurple, TAG_SkillRarityOrange};
    }

    bool Number(const TCHAR* Text, float& OutValue)
    {
        const FString Value(Text);
        return !Value.IsEmpty() && Value.IsNumeric() && LexTryParseString(OutValue, Text) && FMath::IsFinite(OutValue) && OutValue >= 0.f && OutValue <= 10000.f;
    }

    bool Integer(const TCHAR* Text, int32& OutValue)
    {
        float Value = 0.f;
        if (!Number(Text, Value) || Value != FMath::FloorToFloat(Value)) return false;
        OutValue = static_cast<int32>(Value);
        return true;
    }

    bool Note(const TCHAR* Text)
    {
        const FString Value(Text);
        if (Value.TrimStartAndEnd().IsEmpty() || Value.Len() > 256) return false;
        for (TCHAR Character : Value) if (Character < TEXT(' ') || Character == 0x7f) return false;
        return true;
    }
}

FGameplayTag RunSkillBalance::ResolveRarityTag(const FString& Name)
{
    const TCHAR* Names[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
    const TArray<FGameplayTag> Tags = RunSkillBalanceInternal::Rarities();
    for (int32 Index = 0; Index < Tags.Num(); ++Index) if (Name == Names[Index]) return Tags[Index];
    return FGameplayTag();
}

FText RunSkillBalance::RarityName(FGameplayTag Tag)
{
    const TCHAR* Names[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
    const int32 Index = RunSkillBalanceInternal::Rarities().IndexOfByKey(Tag);
    return Index == INDEX_NONE ? FText::GetEmpty() : FText::FromString(Names[Index]);
}

bool RunSkillBalance::IsEmpty(const FRunSkillBalance& Balance)
{
    return !Balance.RarityTag.IsValid() && Balance.Power == 0.f && Balance.ActionPointCost == 0 && Balance.SubActionPointCost == 0 && Balance.WindupSeconds == 0.f;
}

bool RunSkillBalance::IsValid(const FRunSkillBalance& Balance)
{
    return RunSkillBalanceInternal::Rarities().Contains(Balance.RarityTag) && FMath::IsFinite(Balance.Power) && Balance.Power > 0.f && Balance.Power <= 10000.f && Balance.ActionPointCost >= 1 && Balance.ActionPointCost <= 100 && Balance.SubActionPointCost >= 0 && Balance.SubActionPointCost <= 100 && FMath::IsFinite(Balance.WindupSeconds) && Balance.WindupSeconds >= 0.f && Balance.WindupSeconds <= 10.f;
}

bool RunSkillBalance::Apply(const FRunSkillBalance& Balance, FCombatRoundSkill& Skill, FText& OutError)
{
    OutError = NSLOCTEXT("RunSkillBalance", "InvalidTuning", "저장된 스킬 등급·위력·비용·선딜이 올바르지 않습니다.");
    if (!IsValid(Balance)) return false;
    FCombatRoundSkill Tuned = Skill;
    Tuned.Power = Balance.Power;
    Tuned.ActionPointCost = Balance.ActionPointCost;
    Tuned.SubActionPointCost = Balance.SubActionPointCost;
    Tuned.WindupSeconds = Balance.WindupSeconds;
    if (!CombatRoundRules::IsValidSkill(Tuned)) return false;
    Skill = MoveTemp(Tuned);
    OutError = FText::GetEmpty();
    return true;
}

bool RunSkillBalance::Validate(const FRunWeaponSkillRulesState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunSkillBalance", "InvalidState", "스킬 밸런스의 버전·등급·수치 또는 장비별 5등급 가중치가 올바르지 않습니다.");
    if (State.BalanceVersion == 0)
    {
        if (!State.SkillRarityWeights.IsEmpty()) return false;
        for (const FRunWeaponSkillCandidate& Candidate : State.Candidates) if (!IsEmpty(Candidate.Balance)) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (State.BalanceVersion != 1 || State.SchemaVersion != 1 || State.Candidates.IsEmpty() || State.Rarities.Num() != 5 || State.SkillRarityWeights.Num() != 25) return false;
    for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
    {
        if (!IsValid(Candidate.Balance) || !Candidate.SelectionTags.HasTagExact(Candidate.Balance.RarityTag)) return false;
        for (FGameplayTag Tag : RunSkillBalanceInternal::Rarities()) if (Tag != Candidate.Balance.RarityTag && Candidate.SelectionTags.HasTagExact(Tag)) return false;
    }
    TSet<FString> Keys;
    for (const FRunSkillRarityWeight& Entry : State.SkillRarityWeights)
    {
        const FString Key = Entry.EquipmentRarityTag.ToString() + TEXT("|") + Entry.SkillRarityTag.ToString();
        if (!RunItemShopCatalog::IsSupportedRarityTag(Entry.EquipmentRarityTag) || !RunSkillBalanceInternal::Rarities().Contains(Entry.SkillRarityTag) || !FMath::IsFinite(Entry.Weight) || Entry.Weight < 0.f || Entry.Weight > 100.f || Keys.Contains(Key)) return false;
        Keys.Add(Key);
    }
    for (const FRunWeaponRarityRule& Rarity : State.Rarities)
    {
        if (!RunItemShopCatalog::IsSupportedRarityTag(Rarity.RarityTag)) return false;
        float Total = 0.f;
        for (const FRunSkillRarityWeight& Entry : State.SkillRarityWeights) if (Entry.EquipmentRarityTag == Rarity.RarityTag) Total += Entry.Weight;
        if (!FMath::IsNearlyEqual(Total, 100.f, 0.001f)) return false;
    }
    OutError = FText::GetEmpty();
    return true;
}

bool RunSkillBalance::Load(FRunWeaponSkillRulesState& State, FText& OutError)
{
    FString BalanceCsv;
    FString ProbabilityCsv;
    if (!FFileHelper::LoadFileToString(BalanceCsv, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/SKILL_BALANCE.csv"))) || !FFileHelper::LoadFileToString(ProbabilityCsv, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/SKILL_RARITY_PROBABILITIES.csv"))))
    {
        OutError = NSLOCTEXT("RunSkillBalance", "MissingCsv", "스킬 수치 또는 장비별 스킬 등급 확률 CSV를 읽을 수 없습니다.");
        return false;
    }
    return LoadFromStrings(MoveTemp(BalanceCsv), MoveTemp(ProbabilityCsv), State, OutError);
}

bool RunSkillBalance::LoadFromStrings(FString BalanceCsv, FString ProbabilityCsv, FRunWeaponSkillRulesState& State, FText& OutError)
{
    const FText InvalidCsv = NSLOCTEXT("RunSkillBalance", "InvalidCsv", "스킬 CSV의 열·경로·등급·수치 또는 가중치 행이 올바르지 않습니다.");
    OutError = InvalidCsv;
    const FCsvParser BalanceParser(MoveTemp(BalanceCsv));
    const FCsvParser ProbabilityParser(MoveTemp(ProbabilityCsv));
    const FCsvParser::FRows& Rows = BalanceParser.GetRows();
    const FCsvParser::FRows& WeightRows = ProbabilityParser.GetRows();
    const TCHAR* Headers[] = {TEXT("스킬 ID"), TEXT("스킬 이름"), TEXT("스킬 경로"), TEXT("등급"), TEXT("등급 태그"), TEXT("기본 가중치"), TEXT("위력"), TEXT("AP"), TEXT("SAP"), TEXT("선딜(초)"), TEXT("비고")};
    const TCHAR* WeightHeaders[] = {TEXT("장비 등급"), TEXT("장비 등급 태그"), TEXT("스킬 등급"), TEXT("스킬 등급 태그"), TEXT("가중치"), TEXT("비고")};
    if (State.SchemaVersion != 1 || State.Candidates.IsEmpty() || Rows.Num() != State.Candidates.Num() + 1 || Rows[0].Num() != UE_ARRAY_COUNT(Headers) || WeightRows.Num() != 26 || WeightRows[0].Num() != UE_ARRAY_COUNT(WeightHeaders)) return false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeightHeaders); ++Index) if (FCString::Strcmp(WeightRows[0][Index], WeightHeaders[Index]) != 0) return false;
    FRunWeaponSkillRulesState CandidateState = State;
    CandidateState.BalanceVersion = 1;
    CandidateState.SkillRarityWeights.Reset();
    TSet<FSoftObjectPath> Seen;
    for (int32 Index = 1; Index < Rows.Num(); ++Index)
    {
        const TArray<const TCHAR*>& Row = Rows[Index];
        if (Row.Num() != UE_ARRAY_COUNT(Headers)) return false;
        const FSoftObjectPath Path(Row[2]);
        FRunWeaponSkillCandidate* Candidate = CandidateState.Candidates.FindByPredicate([&Path](const FRunWeaponSkillCandidate& Entry) { return Entry.Skill == Path; });
        const USkillDefinitionDataAsset* Asset = Candidate ? Cast<USkillDefinitionDataAsset>(Path.TryLoad()) : nullptr;
        if (!Asset || Asset->GetPrimaryAssetId().ToString() != Row[0] || Seen.Contains(Path) || !RunSkillBalanceInternal::Note(Row[1]) || !RunSkillBalanceInternal::Note(Row[10])) return false;
        Seen.Add(Path);
        FRunSkillBalance Balance;
        Balance.RarityTag = ResolveRarityTag(Row[3]);
        if (!Balance.RarityTag.IsValid() || Balance.RarityTag.ToString() != Row[4] || !RunSkillBalanceInternal::Number(Row[5], Candidate->BaseWeight) || !RunSkillBalanceInternal::Number(Row[6], Balance.Power) || !RunSkillBalanceInternal::Integer(Row[7], Balance.ActionPointCost) || !RunSkillBalanceInternal::Integer(Row[8], Balance.SubActionPointCost) || !RunSkillBalanceInternal::Number(Row[9], Balance.WindupSeconds) || !IsValid(Balance)) return false;
        FCombatRoundSkill Definition;
        if (!Asset->ResolveRoundSkill(Definition, OutError) || !Apply(Balance, Definition, OutError)) return false;
        Candidate->Balance = Balance;
        for (FGameplayTag Tag : RunSkillBalanceInternal::Rarities()) Candidate->SelectionTags.RemoveTag(Tag);
        Candidate->SelectionTags.AddTag(Balance.RarityTag);
        OutError = InvalidCsv;
    }
    for (int32 Index = 1; Index < WeightRows.Num(); ++Index)
    {
        const TArray<const TCHAR*>& Row = WeightRows[Index];
        if (Row.Num() != UE_ARRAY_COUNT(WeightHeaders)) return false;
        FRunSkillRarityWeight Entry;
        Entry.EquipmentRarityTag = RunItemShopCatalog::ResolveRarityTag(Row[0]);
        Entry.SkillRarityTag = ResolveRarityTag(Row[2]);
        if (!Entry.EquipmentRarityTag.IsValid() || Entry.EquipmentRarityTag.ToString() != Row[1] || !Entry.SkillRarityTag.IsValid() || Entry.SkillRarityTag.ToString() != Row[3] || !RunSkillBalanceInternal::Number(Row[4], Entry.Weight) || !RunSkillBalanceInternal::Note(Row[5])) return false;
        CandidateState.SkillRarityWeights.Add(Entry);
    }
    // Keep legacy selection metadata while new Runs query explicit skill rarity tags.
    // 기존 선택 메타데이터는 보존하고 새 Run은 명시적인 스킬 등급 태그를 쿼리합니다.
    FGameplayTagContainer RarityTags;
    for (FGameplayTag Tag : RunSkillBalanceInternal::Rarities()) RarityTags.AddTag(Tag);
    for (FRunWeaponRarityRule& Rarity : CandidateState.Rarities)
    {
        const FRunWeaponRarityRule* Default = GetDefault<URunWeaponSkillRulesDataAsset>()->Rules.Rarities.FindByPredicate([&Rarity](const FRunWeaponRarityRule& Entry) { return Entry.RarityTag == Rarity.RarityTag; });
        // Replace only the known prototype pool gates; preserve separately authored GAS conditions.
        // 알려진 프로토타입 풀 조건만 전환하고 별도로 작성된 GAS 조건은 유지합니다.
        if (Rarity.SkillQuery.IsEmpty() || (Default && Rarity.SkillQuery == Default->SkillQuery)) Rarity.SkillQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(RarityTags);
    }
    if (!Validate(CandidateState, OutError)) return false;
    State = MoveTemp(CandidateState);
    OutError = FText::GetEmpty();
    return true;
}
