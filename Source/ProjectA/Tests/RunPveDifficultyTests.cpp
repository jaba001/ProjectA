#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Game/Run/RunLevelDesign.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace RunPveDifficultyTests
{
    FString Csv()
    {
        return TEXT("난이도,난이도 태그,HP 배율,속도 배율,골드 배율,비고\n하,Encounter.Combat.PvE.Difficulty.Low,0.80,0.90,0.75,하 단계\n중,Encounter.Combat.PvE.Difficulty.Medium,1,1,1,중 단계\n상,Encounter.Combat.PvE.Difficulty.High,1.30,1.10,1.50,상 단계\n");
    }

    const int32 ReferenceHP[10][3] = {{50, 100, 150}, {75, 140, 220}, {110, 200, 330}, {160, 280, 480}, {240, 400, 720}, {340, 560, 1020}, {460, 750, 1380}, {620, 980, 1860}, {800, 1230, 2400}, {1000, 1500, 3000}};

    FString HealthCsv()
    {
        FString Result = TEXT("묶음,난이도 태그,기준 몬스터 HP,설계 의도\n");
        const TCHAR* Tags[] = {TEXT("Low"), TEXT("Medium"), TEXT("High")};
        for (int32 Group = 0; Group < 10; ++Group)
        {
            for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty) Result += FString::Printf(TEXT("%d,Encounter.Combat.PvE.Difficulty.%s,%d,curve fixture\n"), Group + 1, Tags[Difficulty], ReferenceHP[Group][Difficulty]);
        }
        return Result;
    }

    struct FFixture
    {
        FRunTargetState State;
        TArray<FRunNodeDefinition> Nodes = RunProgressRules::GetTargetRoute().Nodes;
        TArray<FName> Completed;

        bool Initialize(FText& Error)
        {
            State.SchemaVersion = 1;
            State.LevelDesign.SchemaVersion = 1;
            for (int32 Index = 0; Index < 10; ++Index)
            {
                FTargetRunGroup& Group = State.Groups.AddDefaulted_GetRef();
                Group.GoldChoices = {3, 4, 5};
                FRunMonsterDefinition Monster;
                Monster.MonsterId = TEXT("DifficultyFixture");
                Monster.DisplayName = FText::FromString(TEXT("Difficulty fixture"));
                Monster.UnitClass = FSoftClassPath(TEXT("/Script/ProjectA.EnemyUnit"));
                Monster.Skill = FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/DifficultyAttack.DifficultyAttack"));
                Monster.MaxHP = 100.f;
                Monster.Speed = 10.f;
                Monster.AP = 2;
                Monster.SAP = 1;
                Monster.MoveRange = 2;
                Group.EnemyRoster.Add(Monster);
                Group.EnemyClasses.Add(Monster.UnitClass);
            }
            return RunPveDifficulty::LoadFromString(Csv(), State.PveDifficulty, Error);
        }

        void Complete(int32 Count)
        {
            Completed.Reset();
            for (int32 Index = 0; Index < Count; ++Index) Completed.Add(Nodes[Index].NodeId);
            State.PveDifficulty.SelectedTags.Init(RunPveDifficulty::GetMediumTag(), (Count + 1) / 2);
        }

        FRunProgressView Progress(ERunPhase Phase, int32 ActiveIndex = INDEX_NONE) const
        {
            return {Nodes, Completed, Nodes.IsValidIndex(ActiveIndex) ? Nodes[ActiveIndex].NodeId : NAME_None, NAME_None, Phase, ECombatResult::None};
        }
    };

    bool SamePolicy(const FRunPveDifficultyState& Left, const FRunPveDifficultyState& Right)
    {
        if (Left.HealthCurveVersion != Right.HealthCurveVersion) return false;
        if (Left.SchemaVersion != Right.SchemaVersion || Left.PresentationVersion != Right.PresentationVersion || Left.SelectedTags != Right.SelectedTags || Left.Rules.Num() != Right.Rules.Num()) return false;
        for (int32 Index = 0; Index < Left.Rules.Num(); ++Index)
        {
            const FRunPveDifficultyRule& A = Left.Rules[Index];
            const FRunPveDifficultyRule& B = Right.Rules[Index];
            if (A.ReferenceHPByGroup != B.ReferenceHPByGroup) return false;
            if (A.DifficultyTag != B.DifficultyTag || A.DisplayName.ToString() != B.DisplayName.ToString() || A.ArenaId != B.ArenaId || A.HPScale != B.HPScale || A.SpeedScale != B.SpeedScale || A.GoldScale != B.GoldScale) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveDifficultyCsvTest, "ProjectA.Run.PveDifficulty.StrictCsvAndAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveDifficultyCsvTest::RunTest(const FString& Parameters)
{
    FText Error;
    FRunPveDifficultyState State;
    if (!TestTrue(TEXT("Three explicitly ordered difficulties parse without loading assets."), RunPveDifficulty::LoadFromString(RunPveDifficultyTests::Csv(), State, Error))) return false;
    TestTrue(TEXT("The policy fixes Low, Medium and High without selection weights or a random seed."), State.SchemaVersion == 1 && State.Rules.Num() == 3 && State.Rules[0].DifficultyTag == RunPveDifficulty::GetLowTag() && State.Rules[1].DifficultyTag == RunPveDifficulty::GetMediumTag() && State.Rules[2].DifficultyTag == RunPveDifficulty::GetHighTag() && State.SelectedTags.IsEmpty());
    const FRunPveDifficultyState Before = State;
    TestFalse(TEXT("Loading CSV cannot replace a previously frozen policy."), RunPveDifficulty::LoadFromString(RunPveDifficultyTests::Csv(), State, Error));
    TestTrue(TEXT("Rejected reload preserves all frozen values and provides a reason."), !Error.IsEmpty() && RunPveDifficultyTests::SamePolicy(State, Before));
    TArray<FString> InvalidInputs;
    InvalidInputs.Add(RunPveDifficultyTests::Csv().Replace(TEXT("HP 배율"), TEXT("HP")));
    InvalidInputs.Add(RunPveDifficultyTests::Csv().Replace(TEXT("Difficulty.Low"), TEXT("Difficulty.Unknown")));
    InvalidInputs.Add(RunPveDifficultyTests::Csv().Replace(TEXT("Difficulty.High"), TEXT("Difficulty.Medium")));
    InvalidInputs.Add(RunPveDifficultyTests::Csv().Replace(TEXT("하,"), TEXT("상,")));
    for (const TCHAR* InvalidNumber : {TEXT("0"), TEXT("-1"), TEXT("nan"), TEXT("inf"), TEXT("1e2"), TEXT(" 1"), TEXT("1."), TEXT(".8"), TEXT("1..2"), TEXT("")}) InvalidInputs.Add(RunPveDifficultyTests::Csv().Replace(TEXT("0.80"), InvalidNumber));
    InvalidInputs.Add(RunPveDifficultyTests::Csv() + TEXT("상,Encounter.Combat.PvE.Difficulty.High,1,1,1,중복\n"));
    for (const FString& Csv : InvalidInputs)
    {
        FRunPveDifficultyState Candidate;
        TestFalse(TEXT("Malformed columns, order, tags and numeric syntax are rejected."), RunPveDifficulty::LoadFromString(Csv, Candidate, Error));
        TestTrue(TEXT("A failed initial load leaves schema zero entirely empty."), !Error.IsEmpty() && Candidate.SchemaVersion == 0 && Candidate.PresentationVersion == 0 && Candidate.HealthCurveVersion == 0 && Candidate.Rules.IsEmpty() && Candidate.SelectedTags.IsEmpty());
    }
    FRunPveDifficultyState Disk;
    FRunPveDifficultyState Expected;
    TestTrue(TEXT("Packaged difficulty and health CSVs freeze the exact reviewed ten-group policy."), RunPveDifficulty::Load(Disk, Error) && RunPveDifficulty::LoadFromStrings(RunPveDifficultyTests::Csv(), RunPveDifficultyTests::HealthCsv(), Expected, Error) && RunPveDifficultyTests::SamePolicy(Disk, Expected));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveDifficultyScalingTest, "ProjectA.Run.PveDifficulty.PreviewScalingAndFailurePreservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveDifficultyScalingTest::RunTest(const FString& Parameters)
{
    RunPveDifficultyTests::FFixture Fixture;
    FText Error;
    if (!Fixture.Initialize(Error)) return false;
    TArray<FRunPveDifficultyOffer> Offers;
    if (!TestTrue(TEXT("All three previews exist before any difficulty selection."), RunPveDifficulty::BuildOffers(Fixture.State, 0, Offers, Error) && Offers.Num() == 3)) return false;
    TestTrue(TEXT("Previews preserve enemy count and show scaled HP and gold bounds."), Offers[0].EnemyCount == 1 && Offers[0].TotalEnemyHP == 80.f && Offers[0].GoldMin == 2 && Offers[0].GoldMax == 4 && Offers[1].TotalEnemyHP == 100.f && Offers[1].GoldMin == 3 && Offers[1].GoldMax == 5 && Offers[2].TotalEnemyHP == 130.f && Offers[2].GoldMin == 5 && Offers[2].GoldMax == 8);
    TArray<FRunMonsterDefinition> Roster = Fixture.State.Groups[0].EnemyRoster;
    TArray<int32> Gold = {99};
    TestFalse(TEXT("A new-policy battle cannot silently assume Medium before selection."), RunPveDifficulty::Resolve(Fixture.State, 0, Roster, Gold, Error));
    TestTrue(TEXT("Rejected resolution leaves both caller outputs unchanged."), !Error.IsEmpty() && Roster.Num() == 1 && Roster[0].MaxHP == 100.f && Gold == TArray<int32>({99}));
    Fixture.State.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetHighTag());
    if (!TestTrue(TEXT("The chosen difficulty resolves from the frozen state."), RunPveDifficulty::Resolve(Fixture.State, 0, Roster, Gold, Error))) return false;
    FRunMonsterDefinition Comparable = Roster[0];
    Comparable.MaxHP = Fixture.State.Groups[0].EnemyRoster[0].MaxHP;
    Comparable.Speed = Fixture.State.Groups[0].EnemyRoster[0].Speed;
    TestTrue(TEXT("Only HP and speed change; identity, tags, class, skill, AP, SAP and movement remain original."), Roster[0].MaxHP == 130.f && FMath::IsNearlyEqual(Roster[0].Speed, 11.f) && FRunMonsterDefinition::StaticStruct()->CompareScriptStruct(&Comparable, &Fixture.State.Groups[0].EnemyRoster[0], 0) && Gold == TArray<int32>({5, 6, 8}));
    const TArray<FRunMonsterDefinition> BeforeRoster = Roster;
    const TArray<int32> BeforeGold = Gold;
    for (int32 InvalidIndex : {-2, 1, 19, 20}) TestFalse(TEXT("Only even PvE combat indices zero through eighteen may resolve."), RunPveDifficulty::Resolve(Fixture.State, InvalidIndex, Roster, Gold, Error));
    TestTrue(TEXT("Invalid combat indices preserve previously resolved outputs."), Roster.Num() == BeforeRoster.Num() && Roster[0].MaxHP == BeforeRoster[0].MaxHP && Gold == BeforeGold);
    FRunTargetState Invalid = Fixture.State;
    Invalid.PveDifficulty.Rules[0].SpeedScale = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Even an unselected nonfinite rule invalidates the whole policy."), RunPveDifficulty::BuildOffers(Invalid, 0, Offers, Error));
    Invalid = Fixture.State;
    Invalid.Groups.Last().GoldChoices[2] = 1000;
    TestFalse(TEXT("A future group's possible reward above one thousand is rejected before starting the Run."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Combat, 0), Error));
    Invalid = Fixture.State;
    Invalid.Groups.Last().EnemyRoster[0].MaxHP = 1000000.f;
    TestFalse(TEXT("Every group's HP is checked at all three difficulties."), RunPveDifficulty::BuildOffers(Invalid, 0, Offers, Error));
    TestTrue(TEXT("Failed previews preserve all three earlier options."), Offers.Num() == 3 && Offers[0].TotalEnemyHP == 80.f && Offers[2].TotalEnemyHP == 130.f);
    Invalid = Fixture.State;
    Invalid.PveDifficulty.Rules[0].GoldScale = 0.01f;
    Invalid.PveDifficulty.SelectedTags[0] = RunPveDifficulty::GetLowTag();
    TestTrue(TEXT("Small positive rewards clamp to one gold after rounding."), RunPveDifficulty::Resolve(Invalid, 0, Roster, Gold, Error) && Gold == TArray<int32>({1, 1, 1}));
    FRunTargetState Legacy = Fixture.State;
    Legacy.PveDifficulty = FRunPveDifficultyState();
    TestTrue(TEXT("Legacy resolution returns original roster and gold values without applying today's CSV."), RunPveDifficulty::Resolve(Legacy, 0, Roster, Gold, Error) && Roster[0].MaxHP == 100.f && Roster[0].Speed == 10.f && Gold == Legacy.Groups[0].GoldChoices);
    TestFalse(TEXT("Legacy Runs do not expose new difficulty choices."), RunPveDifficulty::BuildOffers(Legacy, 0, Offers, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveDifficultyProgressTest, "ProjectA.Run.PveDifficulty.ProgressAndFrozenSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveDifficultyProgressTest::RunTest(const FString& Parameters)
{
    RunPveDifficultyTests::FFixture Fixture;
    FText Error;
    if (!Fixture.Initialize(Error)) return false;
    for (int32 Completed = 0; Completed <= 20; ++Completed)
    {
        Fixture.Complete(Completed);
        TestTrue(TEXT("Only completed PvE selections remain on map and intermission boundaries."), RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(Completed == 20 ? ERunPhase::Complete : ERunPhase::Map), Error));
        if (Completed == 20) continue;
        if (Completed % 2 == 0)
        {
            TestFalse(TEXT("An active PvE requires its own additional selected tag."), RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(ERunPhase::Combat, Completed), Error));
            Fixture.State.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetHighTag());
        }
        for (ERunPhase Phase : {ERunPhase::Preparing, ERunPhase::Combat, ERunPhase::Defeat}) TestTrue(TEXT("Preparing, combat and defeat retain exactly the started battle's selection."), RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(Phase, Completed), Error));
        Fixture.State.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetLowTag());
        TestFalse(TEXT("Future selections cannot be prefilled in a saved active battle."), RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(ERunPhase::Combat, Completed), Error));
    }
    Fixture.Complete(3);
    TestTrue(TEXT("PvE victory result, shopping and encounter choices retain completed selections."), RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(ERunPhase::Result), Error) && RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(ERunPhase::Shop), Error) && RunPveDifficulty::Validate(Fixture.State, Fixture.Progress(ERunPhase::EncounterChoice), Error));
    FRunTargetState Invalid = Fixture.State;
    Invalid.PveDifficulty.SelectedTags[0] = FGameplayTag();
    TestFalse(TEXT("Unknown or missing difficulty tags cannot enter the saved history."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Map), Error));
    Invalid = Fixture.State;
    Swap(Invalid.PveDifficulty.Rules[0], Invalid.PveDifficulty.Rules[1]);
    TestFalse(TEXT("Frozen rules must preserve their Low, Medium and High order."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Map), Error));
    Invalid = Fixture.State;
    Invalid.PveDifficulty.SchemaVersion = 0;
    TestFalse(TEXT("Legacy version zero cannot hide rule or selection payloads."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Map), Error));
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->TargetRun = Fixture.State;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The frozen rules and completed choices serialize through Unreal SaveGame."), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The serialized difficulty policy restores."), Restored.Get())) return false;
    TestTrue(TEXT("Restored values and choices validate without reading current CSV."), RunPveDifficultyTests::SamePolicy(Restored->TargetRun.PveDifficulty, Fixture.State.PveDifficulty) && RunPveDifficulty::Validate(Restored->TargetRun, Fixture.Progress(ERunPhase::Map), Error));
    Restored->TargetRun.PveDifficulty.Rules[1].HPScale = 1.25f;
    TArray<FRunMonsterDefinition> Roster;
    TArray<int32> Gold;
    TestTrue(TEXT("Resolution uses the saved multiplier rather than reloading today's Medium row."), RunPveDifficulty::Resolve(Restored->TargetRun, 0, Roster, Gold, Error) && Roster[0].MaxHP == 125.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveDifficultyArenaTest, "ProjectA.Run.PveDifficulty.FrozenArenaAndExactRosterPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveDifficultyArenaTest::RunTest(const FString& Parameters)
{
    RunPveDifficultyTests::FFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("The unchanged six-column CSV freezes a new presentation policy."), Fixture.Initialize(Error) && Fixture.State.PveDifficulty.PresentationVersion == 1)) return false;
    const FRunTargetState Before = Fixture.State;
    const FName ExpectedIds[] = {TEXT("MeadowBloom"), TEXT("DungeonStone"), TEXT("IceCitadel")};
    TArray<FRunPveDifficultyOffer> Offers;
    if (!TestTrue(TEXT("Three scenery choices retain the exact scaled monster roster."), RunPveDifficulty::BuildOffers(Fixture.State, 0, Offers, Error) && Offers.Num() == 3)) return false;
    FName ArenaId = TEXT("CallerRetainedArena");
    TestFalse(TEXT("An unselected PvE cannot infer its environment from the card order."), RunPveDifficulty::ResolveArena(Fixture.State, 0, ArenaId, Error));
    TestTrue(TEXT("Failed environment resolution preserves the caller's previous ID."), ArenaId == FName(TEXT("CallerRetainedArena")) && !Error.IsEmpty());
    for (int32 Index = 0; Index < Offers.Num(); ++Index)
    {
        const FRunPveDifficultyOffer& Offer = Offers[Index];
        const FCombatArenaEnvironmentProfile* Profile = CombatArenaEnvironment::Find(Offer.ArenaId);
        FGameplayTagContainer Tags;
        Tags.AddTag(Offer.DifficultyTag);
        TestTrue(TEXT("Each new choice freezes its unique tag-matched native environment."), Offer.ArenaId == ExpectedIds[Index] && Profile && Profile->DifficultyQuery.Matches(Tags) && Offer.ArenaId == Fixture.State.PveDifficulty.Rules[Index].ArenaId);
        FRunTargetState Selected = Fixture.State;
        Selected.PveDifficulty.SelectedTags.Add(Offer.DifficultyTag);
        TArray<FRunMonsterDefinition> ActualRoster;
        TArray<int32> ActualGold;
        if (!TestTrue(TEXT("The selected runtime battle resolves the same arena and monster count as its card."), RunPveDifficulty::ResolveArena(Selected, 0, ArenaId, Error) && ArenaId == Offer.ArenaId && RunPveDifficulty::Resolve(Selected, 0, ActualRoster, ActualGold, Error) && ActualRoster.Num() == Offer.EnemyRoster.Num() && Offer.EnemyCount == ActualRoster.Num())) return false;
        for (int32 MonsterIndex = 0; MonsterIndex < ActualRoster.Num(); ++MonsterIndex) TestTrue(TEXT("Every previewed name class tag skill HP speed AP SAP and movement value equals the actual spawn definition."), FRunMonsterDefinition::StaticStruct()->CompareScriptStruct(&Offer.EnemyRoster[MonsterIndex], &ActualRoster[MonsterIndex], 0));
        TestTrue(TEXT("Snapshot encounters explicitly restore the original environment instead of reusing the previous PvE."), RunPveDifficulty::ResolveArena(Selected, 1, ArenaId, Error) && ArenaId.IsNone());
    }
    TestTrue(TEXT("Building and resolving cards never rewrites the seed, groups or saved choice policy."), FRunTargetState::StaticStruct()->CompareScriptStruct(&Fixture.State, &Before, 0));
    for (FName InvalidId : {FName(), FName(TEXT("UntrustedMap")), ExpectedIds[1]})
    {
        FRunTargetState Invalid = Fixture.State;
        Invalid.PveDifficulty.Rules[0].ArenaId = InvalidId;
        TestFalse(TEXT("Missing, unknown and duplicate saved arena IDs are rejected."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Map), Error));
        TestFalse(TEXT("Invalid saved environments cannot produce selectable cards."), RunPveDifficulty::BuildOffers(Invalid, 0, Offers, Error));
        TestTrue(TEXT("A rejected preview preserves the previous full card payload."), Offers.Num() == 3 && Offers[0].ArenaId == ExpectedIds[0] && Offers[0].EnemyRoster.Num() == 1 && Offers[0].EnemyRoster[0].MaxHP == 80.f);
        Invalid.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetLowTag());
        ArenaId = TEXT("CallerRetainedArena");
        TestFalse(TEXT("A forged selected environment fails before spawning."), RunPveDifficulty::ResolveArena(Invalid, 0, ArenaId, Error));
        TestTrue(TEXT("A failed selected environment does not replace the current presentation ID."), ArenaId == FName(TEXT("CallerRetainedArena")) && !Error.IsEmpty());
    }
    for (int32 InvalidVersion : {-1, 2})
    {
        FRunTargetState Invalid = Fixture.State;
        Invalid.PveDifficulty.PresentationVersion = InvalidVersion;
        TestFalse(TEXT("Unknown environment subversions fail closed."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Map), Error));
    }
    FRunTargetState Legacy = Fixture.State;
    Legacy.PveDifficulty.PresentationVersion = 0;
    TestFalse(TEXT("A legacy presentation version cannot conceal new arena payloads."), RunPveDifficulty::Validate(Legacy, Fixture.Progress(ERunPhase::Map), Error));
    for (FRunPveDifficultyRule& Rule : Legacy.PveDifficulty.Rules) Rule.ArenaId = NAME_None;
    TestTrue(TEXT("Existing difficulty saves still validate without adopting new environments."), RunPveDifficulty::Validate(Legacy, Fixture.Progress(ERunPhase::Map), Error));
    TestTrue(TEXT("Existing difficulty cards gain exact monster details while retaining their original arena."), RunPveDifficulty::BuildOffers(Legacy, 0, Offers, Error) && Offers.Num() == 3 && Offers[0].ArenaId.IsNone() && Offers[0].EnemyRoster[0].MaxHP == 80.f);
    Legacy.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetHighTag());
    TestTrue(TEXT("An old high-difficulty selection resolves the original environment."), RunPveDifficulty::ResolveArena(Legacy, 0, ArenaId, Error) && ArenaId.IsNone());
    Legacy.PveDifficulty = FRunPveDifficultyState();
    TestTrue(TEXT("Pre-difficulty custom and saved targets retain their original environment."), RunPveDifficulty::ResolveArena(Legacy, 0, ArenaId, Error) && ArenaId.IsNone());
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->TargetRun = Fixture.State;
    // A serialized mapping is authoritative even if future creation queries choose a different permutation.
    // 이후 생성 쿼리가 다른 조합을 선택해도 직렬화된 무대 연결을 권위 값으로 유지합니다.
    Swap(Save->TargetRun.PveDifficulty.Rules[0].ArenaId, Save->TargetRun.PveDifficulty.Rules[2].ArenaId);
    Save->TargetRun.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetLowTag());
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The presentation subversion and per-rule IDs serialize using existing SaveGame storage."), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The frozen presentation payload restores."), Restored.Get())) return false;
    TestTrue(TEXT("Continue validates and resolves the saved mapping without reading CSV or reapplying creation queries."), RunPveDifficultyTests::SamePolicy(Restored->TargetRun.PveDifficulty, Save->TargetRun.PveDifficulty) && RunPveDifficulty::Validate(Restored->TargetRun, Fixture.Progress(ERunPhase::Combat, 0), Error) && RunPveDifficulty::ResolveArena(Restored->TargetRun, 0, ArenaId, Error) && ArenaId == ExpectedIds[2]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveHealthCurveCsvTest, "ProjectA.Run.PveDifficulty.HealthCurve.StrictCsvAndAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveHealthCurveCsvTest::RunTest(const FString& Parameters)
{
    const FString RulesCsv = RunPveDifficultyTests::Csv();
    const FString HealthCsv = RunPveDifficultyTests::HealthCsv();
    FText Error;
    FRunPveDifficultyState State;
    if (!TestTrue(TEXT("Thirty ordered health rows freeze the version-one policy."), RunPveDifficulty::LoadFromStrings(RulesCsv, HealthCsv, State, Error) && State.HealthCurveVersion == 1 && State.Rules.Num() == 3)) return false;
    for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
    {
        if (!TestEqual(TEXT("Each difficulty freezes ten health references."), State.Rules[Difficulty].ReferenceHPByGroup.Num(), 10)) return false;
        for (int32 Group = 0; Group < 10; ++Group) TestEqual(TEXT("The complete reviewed progression is preserved exactly."), State.Rules[Difficulty].ReferenceHPByGroup[Group], static_cast<float>(RunPveDifficultyTests::ReferenceHP[Group][Difficulty]));
    }
    const FRunPveDifficultyState Before = State;
    TestFalse(TEXT("CSV reload cannot replace an already saved health curve."), RunPveDifficulty::LoadFromStrings(RulesCsv, HealthCsv, State, Error));
    TestTrue(TEXT("Rejected curve replacement preserves the complete policy."), !Error.IsEmpty() && RunPveDifficultyTests::SamePolicy(State, Before));
    TArray<FString> InvalidInputs;
    InvalidInputs.Add(HealthCsv.Replace(TEXT("기준 몬스터 HP"), TEXT("HP")));
    InvalidInputs.Add(HealthCsv.Replace(TEXT("1,Encounter"), TEXT("01,Encounter")));
    InvalidInputs.Add(HealthCsv.Replace(TEXT("Difficulty.High"), TEXT("Difficulty.Medium")));
    InvalidInputs.Add(HealthCsv.Replace(TEXT("Difficulty.Low"), TEXT("Difficulty.Unknown")));
    InvalidInputs.Add(HealthCsv.Replace(TEXT("1,Encounter.Combat.PvE.Difficulty.Low,50,curve fixture\n"), TEXT("")));
    InvalidInputs.Add(HealthCsv + TEXT("10,Encounter.Combat.PvE.Difficulty.High,3000,duplicate\n"));
    InvalidInputs.Add(HealthCsv.Replace(TEXT("Low,75,"), TEXT("Low,50,")));
    InvalidInputs.Add(HealthCsv.Replace(TEXT("Low,50,"), TEXT("Low,100,")));
    for (const TCHAR* InvalidNumber : {TEXT("0"), TEXT("-1"), TEXT("nan"), TEXT("inf"), TEXT("1e2"), TEXT(" 50"), TEXT("50.0"), TEXT("50.5"), TEXT("1000001"), TEXT("")}) InvalidInputs.Add(HealthCsv.Replace(TEXT("Low,50,"), *FString::Printf(TEXT("Low,%s,"), InvalidNumber)));
    for (const FString& Invalid : InvalidInputs)
    {
        FRunPveDifficultyState Candidate;
        const FRunPveDifficultyState Empty;
        TestFalse(TEXT("Missing duplicate unordered malformed nonintegral and nonmonotonic curve rows are rejected."), RunPveDifficulty::LoadFromStrings(RulesCsv, Invalid, Candidate, Error));
        TestTrue(TEXT("Failed two-file creation leaves all default fields untouched."), !Error.IsEmpty() && RunPveDifficultyTests::SamePolicy(Candidate, Empty));
    }
    FRunPveDifficultyState Candidate;
    const FRunPveDifficultyState Empty;
    TestFalse(TEXT("An invalid difficulty CSV cannot publish an otherwise valid health CSV."), RunPveDifficulty::LoadFromStrings(RulesCsv.Replace(TEXT("0.80"), TEXT("0")), HealthCsv, Candidate, Error));
    TestTrue(TEXT("Failure in the first input also leaves the output empty."), RunPveDifficultyTests::SamePolicy(Candidate, Empty));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveHealthCurveRosterTest, "ProjectA.Run.PveDifficulty.HealthCurve.SeededRosterAndPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveHealthCurveRosterTest::RunTest(const FString& Parameters)
{
    FString Stats;
    FString Weights;
    FString Levels;
    if (!TestTrue(TEXT("The production monster inputs are available for deterministic composition checks."), FFileHelper::LoadFileToString(Stats, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_STATS.csv"))) && FFileHelper::LoadFileToString(Weights, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_SPAWN_PROBABILITIES.csv"))) && FFileHelper::LoadFileToString(Levels, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_ENCOUNTERS.csv"))))) return false;
    FText Error;
    TSet<FName> FirstMonsters;
    for (int32 PartySize = 1; PartySize <= 4; ++PartySize)
    {
        for (int32 Seed = 0; Seed < 16; ++Seed)
        {
            FRunTargetState State;
            State.SchemaVersion = 1;
            State.Groups = GetDefault<UTargetRunDefinitionDataAsset>()->Groups;
            if (!TestTrue(TEXT("Every sampled seed and supported party size freezes a real eligible roster."), RunLevelDesign::LoadFromStrings(Stats, Weights, Levels, Seed, PartySize, State, Error) && RunPveDifficulty::LoadFromStrings(RunPveDifficultyTests::Csv(), RunPveDifficultyTests::HealthCsv(), State.PveDifficulty, Error))) return false;
            const FRunTargetState Before = State;
            TestEqual(TEXT("The first encounter contains the single monster used by the requested starting endpoint."), State.Groups[0].EnemyRoster.Num(), 1);
            FirstMonsters.Add(State.Groups[0].EnemyRoster[0].MonsterId);
            for (int32 GroupIndex = 0; GroupIndex < 10; ++GroupIndex)
            {
                const TArray<FRunMonsterDefinition>& Base = State.Groups[GroupIndex].EnemyRoster;
                float MaximumBaseHP = 0.f;
                for (const FRunMonsterDefinition& Monster : Base) MaximumBaseHP = FMath::Max(MaximumBaseHP, Monster.MaxHP);
                TArray<FRunPveDifficultyOffer> Offers;
                if (!TestTrue(TEXT("Every seeded group exposes all three frozen previews."), RunPveDifficulty::BuildOffers(State, GroupIndex * 2, Offers, Error) && Offers.Num() == 3)) return false;
                for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
                {
                    const FRunPveDifficultyOffer& Offer = Offers[Difficulty];
                    const float Reference = static_cast<float>(RunPveDifficultyTests::ReferenceHP[GroupIndex][Difficulty]);
                    if (!TestEqual(TEXT("HP balancing preserves the selected roster size."), Offer.EnemyRoster.Num(), Base.Num())) return false;
                    float MaximumHP = 0.f;
                    float TotalHP = 0.f;
                    for (int32 Slot = 0; Slot < Base.Num(); ++Slot)
                    {
                        const float Expected = static_cast<float>(FMath::CeilToFloat(static_cast<double>(Reference) * Base[Slot].MaxHP / MaximumBaseHP));
                        TestEqual(TEXT("Every species preserves its frozen relative HP with upward integer rounding."), Offer.EnemyRoster[Slot].MaxHP, Expected);
                        FRunMonsterDefinition Comparable = Offer.EnemyRoster[Slot];
                        Comparable.MaxHP = Base[Slot].MaxHP;
                        Comparable.Speed = Base[Slot].Speed;
                        TestTrue(TEXT("Health normalization preserves identity tags skills AP SAP and movement."), FRunMonsterDefinition::StaticStruct()->CompareScriptStruct(&Comparable, &Base[Slot], 0));
                        MaximumHP = FMath::Max(MaximumHP, Offer.EnemyRoster[Slot].MaxHP);
                        TotalHP += Offer.EnemyRoster[Slot].MaxHP;
                    }
                    TestEqual(TEXT("The strongest monster reaches the reference exactly regardless of seed or party size."), MaximumHP, Reference);
                    TestEqual(TEXT("Preview total HP includes all normalized monsters."), Offer.TotalEnemyHP, TotalHP);
                    FRunTargetState Selected = State;
                    Selected.PveDifficulty.SelectedTags.Init(Offer.DifficultyTag, GroupIndex + 1);
                    TArray<FRunMonsterDefinition> Actual;
                    TArray<int32> Gold;
                    if (!TestTrue(TEXT("The selected battle resolves the same normalized values as the card."), RunPveDifficulty::Resolve(Selected, GroupIndex * 2, Actual, Gold, Error) && Actual.Num() == Offer.EnemyRoster.Num())) return false;
                    for (int32 Slot = 0; Slot < Actual.Num(); ++Slot) TestTrue(TEXT("The full preview monster payload equals the spawn definition."), FRunMonsterDefinition::StaticStruct()->CompareScriptStruct(&Actual[Slot], &Offer.EnemyRoster[Slot], 0));
                }
            }
            TestTrue(TEXT("Previews and resolution preserve the original seed roster growth snapshots and unselected policy."), FRunTargetState::StaticStruct()->CompareScriptStruct(&State, &Before, 0));
        }
    }
    TestTrue(TEXT("Seed samples exercise more than one first-monster species."), FirstMonsters.Num() > 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPveHealthCurveSaveTest, "ProjectA.Run.PveDifficulty.HealthCurve.FrozenSaveLegacyAndOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPveHealthCurveSaveTest::RunTest(const FString& Parameters)
{
    RunPveDifficultyTests::FFixture Fixture;
    FText Error;
    if (!Fixture.Initialize(Error)) return false;
    Fixture.State.PveDifficulty = FRunPveDifficultyState();
    if (!RunPveDifficulty::LoadFromStrings(RunPveDifficultyTests::Csv(), RunPveDifficultyTests::HealthCsv(), Fixture.State.PveDifficulty, Error)) return false;
    FTargetRunGroup& Group = Fixture.State.Groups[0];
    Group.EnemyRoster[0].MaxHP = 41.f;
    FRunMonsterDefinition Stronger = Group.EnemyRoster[0];
    Stronger.MonsterId = TEXT("StrongerSecondSlot");
    Stronger.MaxHP = 125.f;
    Group.EnemyRoster.Add(Stronger);
    Group.EnemyClasses.Add(Stronger.UnitClass);
    FRunMonsterDefinition EqualStrongest = Stronger;
    EqualStrongest.MonsterId = TEXT("EqualStrongestThirdSlot");
    Group.EnemyRoster.Add(EqualStrongest);
    Group.EnemyClasses.Add(EqualStrongest.UnitClass);
    Fixture.State.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetLowTag());
    TArray<FRunMonsterDefinition> Roster;
    TArray<int32> Gold;
    if (!TestTrue(TEXT("The strongest slot may follow the first monster and ties share the exact target."), RunPveDifficulty::Resolve(Fixture.State, 0, Roster, Gold, Error) && Roster.Num() == 3 && Roster[0].MaxHP == 17.f && Roster[1].MaxHP == 50.f && Roster[2].MaxHP == 50.f)) return false;
    FRunTargetState Reordered = Fixture.State;
    Swap(Reordered.Groups[0].EnemyRoster[0], Reordered.Groups[0].EnemyRoster[1]);
    Swap(Reordered.Groups[0].EnemyClasses[0], Reordered.Groups[0].EnemyClasses[1]);
    TestTrue(TEXT("Reordering the same roster never changes a species' normalized HP."), RunPveDifficulty::Resolve(Reordered, 0, Roster, Gold, Error) && Roster[0].MaxHP == 50.f && Roster[1].MaxHP == 17.f && Roster[2].MaxHP == 50.f);
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->TargetRun = Fixture.State;
    Save->TargetRun.PveDifficulty.Rules[0].ReferenceHPByGroup[0] = 51.f;
    Save->TargetRun.PveDifficulty.Rules[0].HPScale = 1.25f;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Health policy version and every reference serialize through native SaveGame."), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The native save restores its frozen curve."), Restored.Get())) return false;
    TestTrue(TEXT("Continue uses saved HP references without reloading CSV or applying the legacy multiplier twice."), RunPveDifficultyTests::SamePolicy(Restored->TargetRun.PveDifficulty, Save->TargetRun.PveDifficulty) && RunPveDifficulty::Validate(Restored->TargetRun, Fixture.Progress(ERunPhase::Combat, 0), Error) && RunPveDifficulty::Resolve(Restored->TargetRun, 0, Roster, Gold, Error) && Roster[0].MaxHP == 17.f && Roster[1].MaxHP == 51.f && Roster[2].MaxHP == 51.f);
    const TArray<FRunMonsterDefinition> BeforeRoster = Roster;
    const TArray<int32> BeforeGold = Gold;
    for (int32 InvalidCase = 0; InvalidCase < 8; ++InvalidCase)
    {
        FRunTargetState Invalid = Fixture.State;
        FRunPveDifficultyState& Policy = Invalid.PveDifficulty;
        if (InvalidCase == 0) Policy.HealthCurveVersion = -1;
        else if (InvalidCase == 1) Policy.HealthCurveVersion = 2;
        else if (InvalidCase == 2) Policy.HealthCurveVersion = 0;
        else if (InvalidCase == 3) Policy.Rules[0].ReferenceHPByGroup.Pop();
        else if (InvalidCase == 4) Policy.Rules[0].ReferenceHPByGroup[0] = std::numeric_limits<float>::quiet_NaN();
        else if (InvalidCase == 5) Policy.Rules[0].ReferenceHPByGroup[0] = 50.5f;
        else if (InvalidCase == 6) Policy.Rules[0].ReferenceHPByGroup[1] = 50.f;
        else Policy.Rules[0].ReferenceHPByGroup[0] = 100.f;
        TestFalse(TEXT("Malformed versions payloads fractional values and inverted curves cannot continue."), RunPveDifficulty::Validate(Invalid, Fixture.Progress(ERunPhase::Combat, 0), Error));
        TestFalse(TEXT("Malformed saved curves cannot resolve a combat."), RunPveDifficulty::Resolve(Invalid, 0, Roster, Gold, Error));
        TestTrue(TEXT("Rejected resolution preserves previously resolved caller outputs."), Roster.Num() == BeforeRoster.Num() && Roster[0].MaxHP == BeforeRoster[0].MaxHP && Roster[1].MaxHP == BeforeRoster[1].MaxHP && Gold == BeforeGold);
    }
    FRunTargetState Legacy = Fixture.State;
    Legacy.PveDifficulty = FRunPveDifficultyState();
    if (!RunPveDifficulty::LoadFromString(RunPveDifficultyTests::Csv(), Legacy.PveDifficulty, Error)) return false;
    Legacy.PveDifficulty.SelectedTags.Add(RunPveDifficulty::GetHighTag());
    TestTrue(TEXT("Version-zero difficulty saves retain exact legacy multiplication and acquire no current curve."), Legacy.PveDifficulty.HealthCurveVersion == 0 && Legacy.PveDifficulty.Rules[2].ReferenceHPByGroup.IsEmpty() && RunPveDifficulty::Validate(Legacy, Fixture.Progress(ERunPhase::Combat, 0), Error) && RunPveDifficulty::Resolve(Legacy, 0, Roster, Gold, Error) && Roster[0].MaxHP == 54.f && Roster[1].MaxHP == 163.f && Roster[2].MaxHP == 163.f);
    return true;
}

#endif
