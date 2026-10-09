#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace RunPveDifficultyTests
{
    FString Csv()
    {
        return TEXT("난이도,난이도 태그,HP 배율,속도 배율,골드 배율,비고\n하,Encounter.Combat.PvE.Difficulty.Low,0.80,0.90,0.75,하 단계\n중,Encounter.Combat.PvE.Difficulty.Medium,1,1,1,중 단계\n상,Encounter.Combat.PvE.Difficulty.High,1.30,1.10,1.50,상 단계\n");
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
        if (Left.SchemaVersion != Right.SchemaVersion || Left.SelectedTags != Right.SelectedTags || Left.Rules.Num() != Right.Rules.Num()) return false;
        for (int32 Index = 0; Index < Left.Rules.Num(); ++Index)
        {
            const FRunPveDifficultyRule& A = Left.Rules[Index];
            const FRunPveDifficultyRule& B = Right.Rules[Index];
            if (A.DifficultyTag != B.DifficultyTag || A.DisplayName.ToString() != B.DisplayName.ToString() || A.HPScale != B.HPScale || A.SpeedScale != B.SpeedScale || A.GoldScale != B.GoldScale) return false;
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
        TestTrue(TEXT("A failed initial load leaves schema zero entirely empty."), !Error.IsEmpty() && Candidate.SchemaVersion == 0 && Candidate.Rules.IsEmpty() && Candidate.SelectedTags.IsEmpty());
    }
    FRunPveDifficultyState Disk;
    TestTrue(TEXT("The packaged source CSV uses the same strict three-row contract."), RunPveDifficulty::Load(Disk, Error) && RunPveDifficultyTests::SamePolicy(Disk, Before));
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

#endif
