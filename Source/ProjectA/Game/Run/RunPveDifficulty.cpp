#include "Game/Run/RunPveDifficulty.h"

#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/TargetRunTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/Csv/CsvParser.h"
#include "Unit/UnitDataRules.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PveDifficultyLow, "Encounter.Combat.PvE.Difficulty.Low");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PveDifficultyMedium, "Encounter.Combat.PvE.Difficulty.Medium");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PveDifficultyHigh, "Encounter.Combat.PvE.Difficulty.High");

namespace RunPveDifficultyInternal
{
    constexpr int32 GroupCount = 10;
    constexpr int32 DifficultyCount = 3;

    TArray<FGameplayTag> Tags()
    {
        return {TAG_PveDifficultyLow, TAG_PveDifficultyMedium, TAG_PveDifficultyHigh};
    }

    bool ValidText(const FString& Value, int32 MaximumLength)
    {
        if (Value.IsEmpty() || Value.Len() > MaximumLength || Value.TrimStartAndEnd() != Value) return false;
        for (TCHAR Character : Value) if (Character < TEXT(' ') || Character == 0x7f) return false;
        return true;
    }

    bool ParseScale(const TCHAR* Text, float& OutValue)
    {
        const FString Value(Text);
        if (Value.IsEmpty() || Value.Len() > 32) return false;
        bool bDecimal = false;
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            if (Value[Index] == TEXT('.'))
            {
                if (bDecimal || Index == 0 || Index == Value.Len() - 1) return false;
                bDecimal = true;
            }
            else if (Value[Index] < TEXT('0') || Value[Index] > TEXT('9')) return false;
        }
        return LexTryParseString(OutValue, Text) && FMath::IsFinite(OutValue) && OutValue > 0.f;
    }

    bool Empty(const FRunPveDifficultyState& State)
    {
        return State.SchemaVersion == 0 && State.PresentationVersion == 0 && State.Rules.IsEmpty() && State.SelectedTags.IsEmpty();
    }

    bool ValidRules(const FRunPveDifficultyState& State)
    {
        if (State.SchemaVersion != 1 || State.PresentationVersion < 0 || State.PresentationVersion > 1 || State.Rules.Num() != DifficultyCount || State.SelectedTags.Num() > GroupCount) return false;
        const TArray<FGameplayTag> ExpectedTags = Tags();
        TSet<FName> ArenaIds;
        for (int32 Index = 0; Index < DifficultyCount; ++Index)
        {
            const FRunPveDifficultyRule& Rule = State.Rules[Index];
            if (Rule.DifficultyTag != ExpectedTags[Index] || !ValidText(Rule.DisplayName.ToString(), 64) || !FMath::IsFinite(Rule.HPScale) || Rule.HPScale <= 0.f || !FMath::IsFinite(Rule.SpeedScale) || Rule.SpeedScale <= 0.f || !FMath::IsFinite(Rule.GoldScale) || Rule.GoldScale <= 0.f) return false;
            if (State.PresentationVersion == 0)
            {
                if (!Rule.ArenaId.IsNone()) return false;
            }
            else
            {
                if (!CombatArenaEnvironment::Find(Rule.ArenaId) || ArenaIds.Contains(Rule.ArenaId)) return false;
                ArenaIds.Add(Rule.ArenaId);
            }
        }
        for (FGameplayTag Tag : State.SelectedTags) if (!ExpectedTags.Contains(Tag)) return false;
        return true;
    }

    bool ScaleGroup(const FTargetRunGroup& Group, const FRunPveDifficultyRule& Rule, TArray<FRunMonsterDefinition>& OutRoster, TArray<int32>& OutGold)
    {
        if (Group.EnemyRoster.IsEmpty() || Group.EnemyRoster.Num() > 4 || Group.EnemyClasses.Num() != Group.EnemyRoster.Num() || Group.GoldChoices.Num() != 3) return false;
        TArray<FRunMonsterDefinition> Roster = Group.EnemyRoster;
        TArray<int32> Gold;
        TSet<FName> MonsterIds;
        for (int32 Index = 0; Index < Roster.Num(); ++Index)
        {
            FRunMonsterDefinition& Monster = Roster[Index];
            if (Monster.MonsterId.IsNone() || MonsterIds.Contains(Monster.MonsterId) || !Monster.UnitClass.IsValid() || Monster.UnitClass != Group.EnemyClasses[Index] || !Monster.Skill.IsValid() || !UnitDataRules::IsValidMaxHP(Monster.MaxHP) || !UnitDataRules::IsValidSpeed(Monster.Speed) || !UnitDataRules::IsValidActionPoints(Monster.AP, Monster.SAP) || !UnitDataRules::IsValidMoveRange(Monster.MoveRange)) return false;
            MonsterIds.Add(Monster.MonsterId);
            const double HP = static_cast<double>(Monster.MaxHP) * Rule.HPScale;
            const double Speed = static_cast<double>(Monster.Speed) * Rule.SpeedScale;
            if (!FMath::IsFinite(HP) || HP <= 0.0 || HP > UnitDataRules::MaxStatValue || !FMath::IsFinite(Speed) || Speed < 0.0 || Speed > UnitDataRules::MaxStatValue) return false;
            Monster.MaxHP = FMath::CeilToFloat(static_cast<float>(HP));
            Monster.Speed = static_cast<float>(Speed);
            if (!UnitDataRules::IsValidMaxHP(Monster.MaxHP) || !UnitDataRules::IsValidSpeed(Monster.Speed)) return false;
        }
        for (int32 Original : Group.GoldChoices)
        {
            if (Original < 1 || Original > 1000) return false;
            const double Scaled = static_cast<double>(Original) * Rule.GoldScale;
            if (!FMath::IsFinite(Scaled) || Scaled <= 0.0 || Scaled >= 1000.5) return false;
            const int32 Value = FMath::Max(1, FMath::RoundToInt(Scaled));
            if (Value > 1000) return false;
            Gold.Add(Value);
        }
        OutRoster = MoveTemp(Roster);
        OutGold = MoveTemp(Gold);
        return true;
    }

    bool ValidPolicy(const FRunTargetState& State)
    {
        if (State.SchemaVersion != 1 || State.LevelDesign.SchemaVersion != 1 || State.Groups.Num() != GroupCount || !ValidRules(State.PveDifficulty)) return false;
        for (const FTargetRunGroup& Group : State.Groups)
        {
            for (const FRunPveDifficultyRule& Rule : State.PveDifficulty.Rules)
            {
                TArray<FRunMonsterDefinition> Roster;
                TArray<int32> Gold;
                if (!ScaleGroup(Group, Rule, Roster, Gold)) return false;
            }
        }
        return true;
    }

    bool ValidCombatIndex(const FRunTargetState& State, int32 CombatIndex)
    {
        return State.SchemaVersion == 1 && State.Groups.Num() == GroupCount && CombatIndex >= 0 && CombatIndex < GroupCount * 2 && CombatIndex % 2 == 0;
    }
}

FGameplayTag RunPveDifficulty::GetLowTag()
{
    return TAG_PveDifficultyLow;
}

FGameplayTag RunPveDifficulty::GetMediumTag()
{
    return TAG_PveDifficultyMedium;
}

FGameplayTag RunPveDifficulty::GetHighTag()
{
    return TAG_PveDifficultyHigh;
}

bool RunPveDifficulty::Load(FRunPveDifficultyState& State, FText& OutError)
{
    FString Csv;
    OutError = NSLOCTEXT("RunPveDifficulty", "MissingCsv", "PvE 난이도 CSV를 읽을 수 없습니다.");
    if (!FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/PVE_DIFFICULTIES.csv")))) return false;
    return LoadFromString(MoveTemp(Csv), State, OutError);
}

bool RunPveDifficulty::LoadFromString(FString Csv, FRunPveDifficultyState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunPveDifficulty", "InvalidCsv", "PvE 난이도 CSV의 열·순서·등록 태그·배율 또는 초기 저장 상태가 올바르지 않습니다.");
    if (!RunPveDifficultyInternal::Empty(State)) return false;
    const FCsvParser Parser(MoveTemp(Csv));
    const FCsvParser::FRows& Rows = Parser.GetRows();
    const TCHAR* Headers[] = {TEXT("난이도"), TEXT("난이도 태그"), TEXT("HP 배율"), TEXT("속도 배율"), TEXT("골드 배율"), TEXT("비고")};
    const TCHAR* Names[] = {TEXT("하"), TEXT("중"), TEXT("상")};
    if (Rows.Num() != 4 || Rows[0].Num() != UE_ARRAY_COUNT(Headers)) return false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;
    FRunPveDifficultyState Candidate;
    Candidate.SchemaVersion = 1;
    Candidate.PresentationVersion = 1;
    const TArray<FGameplayTag> Tags = RunPveDifficultyInternal::Tags();
    for (int32 Index = 0; Index < RunPveDifficultyInternal::DifficultyCount; ++Index)
    {
        const TArray<const TCHAR*>& Row = Rows[Index + 1];
        if (Row.Num() != UE_ARRAY_COUNT(Headers) || FCString::Strcmp(Row[0], Names[Index]) != 0 || Tags[Index].ToString() != Row[1] || !RunPveDifficultyInternal::ValidText(Row[5], 256)) return false;
        FRunPveDifficultyRule Rule;
        Rule.DifficultyTag = Tags[Index];
        Rule.DisplayName = FText::FromString(Row[0]);
        if (!RunPveDifficultyInternal::ParseScale(Row[2], Rule.HPScale) || !RunPveDifficultyInternal::ParseScale(Row[3], Rule.SpeedScale) || !RunPveDifficultyInternal::ParseScale(Row[4], Rule.GoldScale)) return false;
        // Resolve tag queries only when creating a Run; saved mappings never follow later catalog reordering.
        // 태그 쿼리는 Run 생성 시에만 해석하며 저장된 연결은 이후 카탈로그 순서 변경을 따르지 않습니다.
        FGameplayTagContainer DifficultyTags;
        DifficultyTags.AddTag(Rule.DifficultyTag);
        for (const FCombatArenaEnvironmentProfile& Profile : CombatArenaEnvironment::GetProfiles())
        {
            if (Profile.DifficultyQuery.IsEmpty() || !Profile.DifficultyQuery.Matches(DifficultyTags)) continue;
            if (!Rule.ArenaId.IsNone()) return false;
            Rule.ArenaId = Profile.ArenaId;
        }
        Candidate.Rules.Add(MoveTemp(Rule));
    }
    if (!RunPveDifficultyInternal::ValidRules(Candidate)) return false;
    State = MoveTemp(Candidate);
    OutError = FText::GetEmpty();
    return true;
}

bool RunPveDifficulty::Validate(const FRunTargetState& State, const FRunProgressView& Progress, FText& OutError)
{
    OutError = NSLOCTEXT("RunPveDifficulty", "InvalidState", "저장된 PvE 난이도 규칙·선택 기록·전투 배율이 진행 상태와 일치하지 않습니다.");
    if (State.PveDifficulty.SchemaVersion == 0)
    {
        if (!RunPveDifficultyInternal::Empty(State.PveDifficulty)) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (!RunPveDifficultyInternal::ValidPolicy(State) || Progress.Nodes.Num() != 20 || Progress.CompletedNodes.Num() > 20) return false;
    int32 ExpectedSelections = (Progress.CompletedNodes.Num() + 1) / 2;
    if (Progress.Phase == ERunPhase::Preparing || Progress.Phase == ERunPhase::Combat || Progress.Phase == ERunPhase::Defeat)
    {
        const int32 CurrentIndex = Progress.Nodes.IndexOfByPredicate([&Progress](const FRunNodeDefinition& Node) { return Node.NodeId == Progress.CurrentNode; });
        if (CurrentIndex == INDEX_NONE || CurrentIndex != Progress.CompletedNodes.Num()) return false;
        if (CurrentIndex % 2 == 0) ++ExpectedSelections;
    }
    if (State.PveDifficulty.SelectedTags.Num() != ExpectedSelections) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool RunPveDifficulty::Resolve(const FRunTargetState& State, int32 CombatIndex, TArray<FRunMonsterDefinition>& OutRoster, TArray<int32>& OutGoldChoices, FText& OutError)
{
    OutError = NSLOCTEXT("RunPveDifficulty", "InvalidSelection", "해당 PvE에 확정된 난이도 또는 저장된 편성·보상 배율이 올바르지 않습니다.");
    if (!RunPveDifficultyInternal::ValidCombatIndex(State, CombatIndex)) return false;
    const FTargetRunGroup& Group = State.Groups[CombatIndex / 2];
    if (State.PveDifficulty.SchemaVersion == 0)
    {
        if (!RunPveDifficultyInternal::Empty(State.PveDifficulty)) return false;
        OutRoster = Group.EnemyRoster;
        OutGoldChoices = Group.GoldChoices;
    }
    else
    {
        if (!RunPveDifficultyInternal::ValidPolicy(State) || !State.PveDifficulty.SelectedTags.IsValidIndex(CombatIndex / 2)) return false;
        const FGameplayTag Selected = State.PveDifficulty.SelectedTags[CombatIndex / 2];
        const FRunPveDifficultyRule* Rule = State.PveDifficulty.Rules.FindByPredicate([Selected](const FRunPveDifficultyRule& Entry) { return Entry.DifficultyTag == Selected; });
        if (!Rule || !RunPveDifficultyInternal::ScaleGroup(Group, *Rule, OutRoster, OutGoldChoices)) return false;
    }
    OutError = FText::GetEmpty();
    return true;
}

bool RunPveDifficulty::ResolveArena(const FRunTargetState& State, int32 CombatIndex, FName& OutArenaId, FText& OutError)
{
    OutError = NSLOCTEXT("RunPveDifficulty", "InvalidArena", "저장된 PvE 전투 무대 또는 확정된 난이도를 해석할 수 없습니다.");
    if (State.SchemaVersion != 1 || State.Groups.Num() != RunPveDifficultyInternal::GroupCount || CombatIndex < 0 || CombatIndex >= RunPveDifficultyInternal::GroupCount * 2) return false;
    if (State.PveDifficulty.SchemaVersion == 0)
    {
        if (!RunPveDifficultyInternal::Empty(State.PveDifficulty)) return false;
    }
    else
    {
        if (!RunPveDifficultyInternal::ValidPolicy(State)) return false;
        if (CombatIndex % 2 == 0)
        {
            if (!State.PveDifficulty.SelectedTags.IsValidIndex(CombatIndex / 2)) return false;
            const FGameplayTag Selected = State.PveDifficulty.SelectedTags[CombatIndex / 2];
            const FRunPveDifficultyRule* Rule = State.PveDifficulty.Rules.FindByPredicate([Selected](const FRunPveDifficultyRule& Entry) { return Entry.DifficultyTag == Selected; });
            if (!Rule) return false;
            OutArenaId = Rule->ArenaId;
            OutError = FText::GetEmpty();
            return true;
        }
    }
    OutArenaId = NAME_None;
    OutError = FText::GetEmpty();
    return true;
}

bool RunPveDifficulty::BuildOffers(const FRunTargetState& State, int32 CombatIndex, TArray<FRunPveDifficultyOffer>& OutOffers, FText& OutError)
{
    OutError = NSLOCTEXT("RunPveDifficulty", "InvalidOffers", "PvE 난이도 선택지를 구성할 수 없습니다.");
    if (!RunPveDifficultyInternal::ValidCombatIndex(State, CombatIndex) || !RunPveDifficultyInternal::ValidPolicy(State)) return false;
    TArray<FRunPveDifficultyOffer> Offers;
    for (const FRunPveDifficultyRule& Rule : State.PveDifficulty.Rules)
    {
        TArray<FRunMonsterDefinition> Roster;
        TArray<int32> Gold;
        if (!RunPveDifficultyInternal::ScaleGroup(State.Groups[CombatIndex / 2], Rule, Roster, Gold)) return false;
        FRunPveDifficultyOffer Offer;
        Offer.DifficultyTag = Rule.DifficultyTag;
        Offer.DisplayName = Rule.DisplayName;
        Offer.ArenaId = Rule.ArenaId;
        Offer.HPScale = Rule.HPScale;
        Offer.SpeedScale = Rule.SpeedScale;
        Offer.GoldScale = Rule.GoldScale;
        Offer.EnemyCount = Roster.Num();
        Offer.GoldMin = Gold[0];
        Offer.GoldMax = Gold[0];
        for (int32 Value : Gold)
        {
            Offer.GoldMin = FMath::Min(Offer.GoldMin, Value);
            Offer.GoldMax = FMath::Max(Offer.GoldMax, Value);
        }
        for (const FRunMonsterDefinition& Monster : Roster) Offer.TotalEnemyHP += Monster.MaxHP;
        Offer.EnemyRoster = MoveTemp(Roster);
        Offers.Add(MoveTemp(Offer));
    }
    OutOffers = MoveTemp(Offers);
    OutError = FText::GetEmpty();
    return true;
}
