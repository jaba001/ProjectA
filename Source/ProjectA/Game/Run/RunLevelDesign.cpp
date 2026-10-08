#include "Game/Run/RunLevelDesign.h"

#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/TargetRunTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/Csv/CsvParser.h"
#include "Types/GameplayTagCandidateSelection.h"
#include "Unit/EnemyUnit.h"
#include "Unit/UnitDataRules.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelMonster, "Monster");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelForest, "Monster.Region.Forest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelSwamp, "Monster.Region.Swamp");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelCave, "Monster.Region.Cave");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelSnow, "Monster.Region.Snow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelCommon, "Monster.Role.Common");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelSkirmisher, "Monster.Role.Skirmisher");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelBrute, "Monster.Role.Brute");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelBoss, "Monster.Role.Boss");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LevelDevelopment, "Monster.Development");

namespace
{
    constexpr int32 GroupCount = 10;
    constexpr int32 MaximumCatalogSize = 1024;

    bool ValidText(const FString& Text, int32 MaximumLength, bool bAllowEmpty = false)
    {
        if (Text.Len() > MaximumLength || (!bAllowEmpty && Text.TrimStartAndEnd().IsEmpty())) return false;
        for (const TCHAR Character : Text) if (Character < TEXT(' ') || Character == 0x7f) return false;
        return true;
    }

    bool ValidId(const FString& Text)
    {
        if (Text.IsEmpty() || Text.Len() > 64 || FName(Text).IsNone()) return false;
        for (const TCHAR Character : Text)
        {
            if ((Character < TEXT('A') || Character > TEXT('Z')) && (Character < TEXT('a') || Character > TEXT('z')) && (Character < TEXT('0') || Character > TEXT('9')) && Character != TEXT('_')) return false;
        }
        return true;
    }

    bool ParseNumber(const TCHAR* Text, double& OutValue, bool bInteger = false)
    {
        const FString Value(Text);
        if (Value.IsEmpty() || Value.Len() > 64) return false;
        bool bDecimal = false;
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            const TCHAR Character = Value[Index];
            if (Character == TEXT('.'))
            {
                if (bInteger || bDecimal || Index == 0 || Index == Value.Len() - 1) return false;
                bDecimal = true;
            }
            else if (Character < TEXT('0') || Character > TEXT('9')) return false;
        }
        OutValue = FCString::Atod(*Value);
        return FMath::IsFinite(OutValue) && OutValue <= MAX_flt;
    }

    bool ParseFloat(const TCHAR* Text, float& OutValue)
    {
        double Value = 0.0;
        if (!ParseNumber(Text, Value)) return false;
        OutValue = static_cast<float>(Value);
        return FMath::IsFinite(OutValue);
    }

    bool ParseInt(const TCHAR* Text, int32& OutValue)
    {
        double Value = 0.0;
        if (!ParseNumber(Text, Value, true) || Value > MAX_int32) return false;
        OutValue = static_cast<int32>(Value);
        return true;
    }

    bool ValidPath(const FSoftObjectPath& Path, bool bClass)
    {
        const FString Value = Path.ToString();
        if (!Path.IsValid() || !Path.GetSubPathUtf8String().IsEmpty() || !ValidText(Value, 512) || !FPackageName::IsValidObjectPath(Value)) return false;
        if (bClass && Value.StartsWith(TEXT("/Script/ProjectA."))) return true;
        return Value.StartsWith(TEXT("/Game/")) && (!bClass || Value.EndsWith(TEXT("_C")));
    }

    bool ParseTags(const TCHAR* Text, FGameplayTagContainer& OutTags)
    {
        const FString Value(Text);
        if (!ValidText(Value, 1024, true)) return false;
        if (Value.IsEmpty()) return true;
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

    FGameplayTagQuery MakeQuery(const FGameplayTagContainer& Required, const FGameplayTagContainer& Excluded)
    {
        if (Required.IsEmpty() && Excluded.IsEmpty()) return FGameplayTagQuery();
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

    bool ValidQuery(const FGameplayTagQuery& Query)
    {
        for (const FGameplayTag Tag : Query.GetGameplayTagArray()) if (!Tag.IsValid()) return false;
        return true;
    }

    int32 ScaledCount(int32 Count, int32 PartySize)
    {
        return (Count * PartySize + 3) / 4;
    }

    bool ScaleMonster(const FRunMonsterDefinition& Original, const FRunLevelRule& Rule, int32 PartySize, FRunMonsterDefinition& OutMonster)
    {
        const double HP = static_cast<double>(Original.MaxHP) * Rule.HPScale * (0.4 + 0.15 * PartySize);
        const double Speed = static_cast<double>(Original.Speed) * Rule.SpeedScale;
        if (!FMath::IsFinite(HP) || HP <= 0.0 || HP > UnitDataRules::MaxStatValue || !FMath::IsFinite(Speed) || Speed < 0.0 || Speed > UnitDataRules::MaxStatValue) return false;
        OutMonster = Original;
        OutMonster.MaxHP = FMath::CeilToFloat(static_cast<float>(HP));
        OutMonster.Speed = static_cast<float>(Speed);
        return UnitDataRules::IsValidMaxHP(OutMonster.MaxHP) && UnitDataRules::IsValidSpeed(OutMonster.Speed);
    }

    bool ValidPolicy(const FRunTargetState& State)
    {
        const FRunLevelDesignState& Policy = State.LevelDesign;
        if (State.SchemaVersion != 1 || Policy.SchemaVersion != 1 || Policy.PartySize < 1 || Policy.PartySize > 4 || Policy.Catalog.IsEmpty() || Policy.Catalog.Num() > MaximumCatalogSize || Policy.Rules.Num() != GroupCount || State.Groups.Num() != GroupCount) return false;
        TSet<FName> Ids;
        TSet<FSoftClassPath> Classes;
        for (const FRunMonsterDefinition& Monster : Policy.Catalog)
        {
            if (!ValidId(Monster.MonsterId.ToString()) || Ids.Contains(Monster.MonsterId) || Classes.Contains(Monster.UnitClass) || !ValidText(Monster.DisplayName.ToString(), 128) || !ValidPath(Monster.UnitClass, true) || !ValidPath(Monster.Skill, false) || !Monster.Tags.HasTag(TAG_LevelMonster) || !UnitDataRules::IsValidMaxHP(Monster.MaxHP) || !UnitDataRules::IsValidActionPoints(Monster.AP, Monster.SAP) || !UnitDataRules::IsValidSpeed(Monster.Speed) || !UnitDataRules::IsValidMoveRange(Monster.MoveRange) || !FMath::IsFinite(Monster.Weight) || Monster.Weight < 0.0f) return false;
            for (const FGameplayTag Tag : Monster.Tags) if (!Tag.IsValid()) return false;
            Ids.Add(Monster.MonsterId);
            Classes.Add(Monster.UnitClass);
        }
        for (int32 Index = 0; Index < Policy.Rules.Num(); ++Index)
        {
            const FRunLevelRule& Rule = Policy.Rules[Index];
            if (!ValidText(Rule.Name.ToString(), 128) || !ValidQuery(Rule.EnemyQuery) || !ValidQuery(Rule.LeaderQuery) || Rule.EnemyCount < 1 || Rule.EnemyCount > 4 || Rule.SnapshotCount < 1 || Rule.SnapshotCount > 4 || !FMath::IsFinite(Rule.HPScale) || Rule.HPScale <= 0.0f || !FMath::IsFinite(Rule.SpeedScale) || Rule.SpeedScale <= 0.0f || Rule.GoldMin < 1 || Rule.GoldMax > 1000 || Rule.GoldMin > Rule.GoldMax || !FMath::IsFinite(Rule.MaxHPGrowth) || Rule.MaxHPGrowth < 0.0f || Rule.MaxHPGrowth > 100.0f || !FMath::IsFinite(Rule.SpeedGrowth) || Rule.SpeedGrowth < 0.0f || Rule.SpeedGrowth > 100.0f || !UnitDataRules::IsValidAttribute(Rule.RestHP) || !UnitDataRules::IsValidMaxHP(Rule.SnapshotHP) || !UnitDataRules::IsValidSpeed(Rule.SnapshotSpeed)) return false;
            if (State.Groups[Index].Opponent.Members.Num() < ScaledCount(Rule.SnapshotCount, Policy.PartySize)) return false;
            int32 Eligible = 0;
            int32 Leaders = 0;
            for (const FRunMonsterDefinition& Monster : Policy.Catalog)
            {
                if (Monster.Weight == 0.0f || (!Rule.EnemyQuery.IsEmpty() && !Rule.EnemyQuery.Matches(Monster.Tags))) continue;
                FRunMonsterDefinition Scaled;
                if (!ScaleMonster(Monster, Rule, Policy.PartySize, Scaled)) return false;
                ++Eligible;
                if (Rule.LeaderQuery.IsEmpty() || Rule.LeaderQuery.Matches(Monster.Tags)) ++Leaders;
            }
            if (Eligible < ScaledCount(Rule.EnemyCount, Policy.PartySize) || Leaders == 0) return false;
        }
        return true;
    }

    bool BuildGroups(const FRunTargetState& State, TArray<FTargetRunGroup>& OutGroups)
    {
        if (!ValidPolicy(State)) return false;
        TArray<FTargetRunGroup> Groups = State.Groups;
        FRandomStream Random(State.LevelDesign.Seed);
        for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
        {
            const FRunLevelRule& Rule = State.LevelDesign.Rules[GroupIndex];
            TArray<FGameplayTagWeightedCandidate> Candidates;
            for (const FRunMonsterDefinition& Monster : State.LevelDesign.Catalog)
            {
                FGameplayTagWeightedCandidate& Candidate = Candidates.AddDefaulted_GetRef();
                Candidate.Tags = Monster.Tags;
                Candidate.BaseWeight = Rule.EnemyQuery.IsEmpty() || Rule.EnemyQuery.Matches(Monster.Tags) ? Monster.Weight : 0.0f;
            }
            const int32 Count = ScaledCount(Rule.EnemyCount, State.LevelDesign.PartySize);
            TArray<int32> Selected;
            if (Rule.LeaderQuery.IsEmpty())
            {
                if (!GameplayTagCandidateSelection::Select(Candidates, Rule.EnemyQuery, Count, false, Random, Selected)) return false;
            }
            else
            {
                if (!GameplayTagCandidateSelection::Select(Candidates, Rule.LeaderQuery, 1, false, Random, Selected)) return false;
                Candidates[Selected[0]].BaseWeight = 0.0f;
                TArray<int32> Others;
                if (Count > 1 && !GameplayTagCandidateSelection::Select(Candidates, Rule.EnemyQuery, Count - 1, false, Random, Others)) return false;
                Selected.Append(Others);
            }
            FTargetRunGroup& Group = Groups[GroupIndex];
            Group.EnemyClasses.Reset();
            Group.EnemyRoster.Reset();
            for (const int32 Index : Selected)
            {
                FRunMonsterDefinition Monster;
                if (!ScaleMonster(State.LevelDesign.Catalog[Index], Rule, State.LevelDesign.PartySize, Monster)) return false;
                Group.EnemyClasses.Add(Monster.UnitClass);
                Group.EnemyRoster.Add(MoveTemp(Monster));
            }
            Group.GoldChoices = {Rule.GoldMin, (Rule.GoldMin + Rule.GoldMax) / 2, Rule.GoldMax};
            Group.MaxHPGrowth = Rule.MaxHPGrowth;
            Group.SpeedGrowth = Rule.SpeedGrowth;
            Group.Opponent.Members.SetNum(ScaledCount(Rule.SnapshotCount, State.LevelDesign.PartySize));
            for (FPartySnapshotMember& Member : Group.Opponent.Members)
            {
                Member.Stats.MaxHP = Member.Stats.CurrentHP = Rule.SnapshotHP;
                Member.Stats.Speed = Rule.SnapshotSpeed;
            }
        }
        OutGroups = MoveTemp(Groups);
        return true;
    }

    bool ParseStats(FString Text, TArray<FRunMonsterDefinition>& OutCatalog)
    {
        const FCsvParser Parser(MoveTemp(Text));
        const FCsvParser::FRows& Rows = Parser.GetRows();
        const TCHAR* Headers[] = {TEXT("몬스터 ID"), TEXT("몬스터 이름"), TEXT("Blueprint 클래스 경로"), TEXT("분류 태그"), TEXT("최대 HP"), TEXT("AP"), TEXT("SAP"), TEXT("속도"), TEXT("이동거리(타일)"), TEXT("스킬 경로"), TEXT("공격 피해(참고)"), TEXT("공격 선딜(초·참고)"), TEXT("비고")};
        if (Rows.Num() < 2 || Rows.Num() > MaximumCatalogSize + 1 || Rows[0].Num() != UE_ARRAY_COUNT(Headers)) return false;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;
        for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
        {
            const TArray<const TCHAR*>& Row = Rows[RowIndex];
            if (Row.Num() != UE_ARRAY_COUNT(Headers) || !ValidId(Row[0]) || !ValidText(Row[1], 128) || !ValidText(Row[12], 2048)) return false;
            FRunMonsterDefinition Monster;
            Monster.MonsterId = FName(Row[0]);
            Monster.DisplayName = FText::FromString(Row[1]);
            Monster.UnitClass = FSoftClassPath(Row[2]);
            Monster.Skill = FSoftObjectPath(Row[9]);
            float ReferenceDamage = 0.0f;
            float ReferenceWindup = 0.0f;
            if (!ParseTags(Row[3], Monster.Tags) || !ParseFloat(Row[4], Monster.MaxHP) || !ParseInt(Row[5], Monster.AP) || !ParseInt(Row[6], Monster.SAP) || !ParseFloat(Row[7], Monster.Speed) || !ParseInt(Row[8], Monster.MoveRange) || !ParseFloat(Row[10], ReferenceDamage) || !ParseFloat(Row[11], ReferenceWindup) || !UnitDataRules::IsValidAttribute(ReferenceDamage) || !UnitDataRules::IsValidAttribute(ReferenceWindup)) return false;
            OutCatalog.Add(MoveTemp(Monster));
        }
        return true;
    }

    bool ParseWeights(FString Text, TArray<FRunMonsterDefinition>& Catalog)
    {
        const FCsvParser Parser(MoveTemp(Text));
        const FCsvParser::FRows& Rows = Parser.GetRows();
        const TCHAR* Headers[] = {TEXT("몬스터 ID"), TEXT("출현 가중치"), TEXT("전체 후보 기준 확률(%)"), TEXT("활성 여부"), TEXT("비고")};
        if (Rows.Num() != Catalog.Num() + 1 || Rows[0].Num() != UE_ARRAY_COUNT(Headers)) return false;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;
        TSet<FName> Ids;
        TMap<FName, double> Percentages;
        double TotalWeight = 0.0;
        for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
        {
            const TArray<const TCHAR*>& Row = Rows[RowIndex];
            if (Row.Num() != UE_ARRAY_COUNT(Headers) || !ValidId(Row[0]) || !ValidText(Row[4], 2048)) return false;
            const FName Id(Row[0]);
            FRunMonsterDefinition* Monster = Catalog.FindByPredicate([Id](const FRunMonsterDefinition& Entry) { return Entry.MonsterId == Id; });
            const FString Enabled(Row[3]);
            double Percentage = 0.0;
            if (!Monster || Ids.Contains(Id) || (Enabled != TEXT("0") && Enabled != TEXT("1")) || !ParseFloat(Row[1], Monster->Weight) || !ParseNumber(Row[2], Percentage) || Percentage > 100.0) return false;
            if (Enabled == TEXT("0")) Monster->Weight = 0.0f;
            Ids.Add(Id);
            Percentages.Add(Id, Percentage);
            TotalWeight += Monster->Weight;
        }
        if (TotalWeight <= 0.0 || !FMath::IsFinite(TotalWeight)) return false;
        // Display percentages may be rounded to two decimals; selection always uses authored weights.
        // 표시 확률의 소수 둘째 자리 반올림을 허용하며 추첨에는 항상 작성된 가중치를 사용합니다.
        for (const FRunMonsterDefinition& Monster : Catalog) if (FMath::Abs(Percentages.FindRef(Monster.MonsterId) - 100.0 * Monster.Weight / TotalWeight) > 0.011) return false;
        return true;
    }

    bool ParseLevels(FString Text, TArray<FRunLevelRule>& OutRules)
    {
        const FCsvParser Parser(MoveTemp(Text));
        const FCsvParser::FRows& Rows = Parser.GetRows();
        const TCHAR* Headers[] = {TEXT("묶음"), TEXT("구간 이름"), TEXT("필수 태그"), TEXT("제외 태그"), TEXT("필수 선봉 태그"), TEXT("기준 몬스터 수"), TEXT("HP 배율"), TEXT("속도 배율"), TEXT("골드 최소"), TEXT("골드 최대"), TEXT("아군 HP 성장"), TEXT("아군 속도 성장"), TEXT("승리 휴식 회복량"), TEXT("Snapshot 기준 인원"), TEXT("Snapshot HP"), TEXT("Snapshot 속도"), TEXT("설계 의도")};
        if (Rows.Num() != GroupCount + 1 || Rows[0].Num() != UE_ARRAY_COUNT(Headers)) return false;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;
        for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
        {
            const TArray<const TCHAR*>& Row = Rows[RowIndex];
            int32 Group = 0;
            if (Row.Num() != UE_ARRAY_COUNT(Headers) || !ParseInt(Row[0], Group) || Group != RowIndex || !ValidText(Row[1], 128) || !ValidText(Row[16], 2048)) return false;
            FRunLevelRule Rule;
            Rule.Name = FText::FromString(Row[1]);
            FGameplayTagContainer Required;
            FGameplayTagContainer Excluded;
            FGameplayTagContainer Leaders;
            if (!ParseTags(Row[2], Required) || !ParseTags(Row[3], Excluded) || !ParseTags(Row[4], Leaders) || Required.HasAnyExact(Excluded) || Leaders.HasAnyExact(Excluded)) return false;
            Rule.EnemyQuery = MakeQuery(Required, Excluded);
            Rule.LeaderQuery = MakeQuery(Leaders, FGameplayTagContainer());
            if (!ParseInt(Row[5], Rule.EnemyCount) || !ParseFloat(Row[6], Rule.HPScale) || !ParseFloat(Row[7], Rule.SpeedScale) || !ParseInt(Row[8], Rule.GoldMin) || !ParseInt(Row[9], Rule.GoldMax) || !ParseFloat(Row[10], Rule.MaxHPGrowth) || !ParseFloat(Row[11], Rule.SpeedGrowth) || !ParseFloat(Row[12], Rule.RestHP) || !ParseInt(Row[13], Rule.SnapshotCount) || !ParseFloat(Row[14], Rule.SnapshotHP) || !ParseFloat(Row[15], Rule.SnapshotSpeed)) return false;
            OutRules.Add(MoveTemp(Rule));
        }
        return true;
    }
}

bool RunLevelDesign::Load(FRunTargetState& State, int32 PartySize, FText& OutError)
{
    FString Stats;
    FString Weights;
    FString Levels;
    OutError = NSLOCTEXT("RunLevelDesign", "MissingCsv", "몬스터 스탯·출현 가중치·레벨 디자인 CSV를 읽을 수 없습니다.");
    if (!FFileHelper::LoadFileToString(Stats, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_STATS.csv"))) || !FFileHelper::LoadFileToString(Weights, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_SPAWN_PROBABILITIES.csv"))) || !FFileHelper::LoadFileToString(Levels, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_ENCOUNTERS.csv")))) return false;
    FRunTargetState Candidate = State;
    if (!LoadFromStrings(MoveTemp(Stats), MoveTemp(Weights), MoveTemp(Levels), 0, PartySize, Candidate, OutError)) return false;
    for (const FRunMonsterDefinition& Monster : Candidate.LevelDesign.Catalog)
    {
        UClass* Class = Monster.UnitClass.TryLoadClass<AEnemyUnit>();
        USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Monster.Skill.TryLoad());
        OutError = NSLOCTEXT("RunLevelDesign", "MissingContent", "몬스터 클래스 또는 원본 공격 스킬을 불러올 수 없습니다.");
        if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Skill) return false;
        const TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills = {Skill};
        if (!UnitDataRules::ValidateSkills(Skills, true, OutError)) return false;
    }
    // Draw the seed only after every input and possible scaled candidate has passed validation.
    // 모든 입력과 선택 가능한 배율 적용 후보를 검증한 뒤에만 최초 시드를 추첨합니다.
    Candidate.LevelDesign.Seed = FMath::Rand();
    if (!Build(Candidate, OutError)) return false;
    State = MoveTemp(Candidate);
    OutError = FText::GetEmpty();
    return true;
}

bool RunLevelDesign::LoadFromStrings(FString Stats, FString Weights, FString Levels, int32 Seed, int32 PartySize, FRunTargetState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunLevelDesign", "InvalidCsv", "몬스터·가중치·레벨 CSV의 열·식별자·등록 태그·수치·후보 수 또는 새 Run 상태가 올바르지 않습니다.");
    const FRunLevelDesignState Empty;
    if (State.SchemaVersion != 1 || !State.CompletedEncounterChoices.IsEmpty() || !FRunLevelDesignState::StaticStruct()->CompareScriptStruct(&State.LevelDesign, &Empty, 0) || State.Groups.ContainsByPredicate([](const FTargetRunGroup& Group) { return !Group.EnemyRoster.IsEmpty(); })) return false;
    FRunTargetState Candidate = State;
    Candidate.LevelDesign.SchemaVersion = 1;
    Candidate.LevelDesign.Seed = Seed;
    Candidate.LevelDesign.PartySize = PartySize;
    if (!ParseStats(MoveTemp(Stats), Candidate.LevelDesign.Catalog) || !ParseWeights(MoveTemp(Weights), Candidate.LevelDesign.Catalog) || !ParseLevels(MoveTemp(Levels), Candidate.LevelDesign.Rules) || !Build(Candidate, OutError)) return false;
    State = MoveTemp(Candidate);
    OutError = FText::GetEmpty();
    return true;
}

bool RunLevelDesign::Build(FRunTargetState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunLevelDesign", "InvalidBuild", "레벨 디자인의 저장 조건 또는 후보 수가 유효하지 않습니다.");
    TArray<FTargetRunGroup> Groups;
    if (!BuildGroups(State, Groups)) return false;
    State.Groups = MoveTemp(Groups);
    OutError = FText::GetEmpty();
    return true;
}

bool RunLevelDesign::Validate(const FRunTargetState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunLevelDesign", "InvalidState", "저장된 몬스터 카탈로그·레벨 규칙·시드 또는 확정 편성이 일치하지 않습니다. 저장 원본을 유지합니다.");
    if (State.LevelDesign.SchemaVersion == 0)
    {
        const FRunLevelDesignState Empty;
        if (!FRunLevelDesignState::StaticStruct()->CompareScriptStruct(&State.LevelDesign, &Empty, 0) || State.Groups.ContainsByPredicate([](const FTargetRunGroup& Group) { return !Group.EnemyRoster.IsEmpty(); })) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    TArray<FTargetRunGroup> Expected;
    if (!BuildGroups(State, Expected)) return false;
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        const FTargetRunGroup& Actual = State.Groups[Index];
        const FTargetRunGroup& Group = Expected[Index];
        if (Actual.EnemyClasses != Group.EnemyClasses || Actual.EnemyRoster.Num() != Group.EnemyRoster.Num() || Actual.GoldChoices != Group.GoldChoices || Actual.MaxHPGrowth != Group.MaxHPGrowth || Actual.SpeedGrowth != Group.SpeedGrowth || Actual.Opponent.Members.Num() != Group.Opponent.Members.Num()) return false;
        for (int32 Slot = 0; Slot < Group.EnemyRoster.Num(); ++Slot) if (!FRunMonsterDefinition::StaticStruct()->CompareScriptStruct(&Actual.EnemyRoster[Slot], &Group.EnemyRoster[Slot], 0)) return false;
        for (int32 Slot = 0; Slot < Group.Opponent.Members.Num(); ++Slot)
        {
            const FPartySnapshotStats& Stats = Actual.Opponent.Members[Slot].Stats;
            const FPartySnapshotStats& ExpectedStats = Group.Opponent.Members[Slot].Stats;
            if (Stats.MaxHP != ExpectedStats.MaxHP || Stats.CurrentHP != ExpectedStats.CurrentHP || Stats.Speed != ExpectedStats.Speed) return false;
        }
    }
    OutError = FText::GetEmpty();
    return true;
}
