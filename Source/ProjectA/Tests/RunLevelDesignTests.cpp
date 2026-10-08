#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Run/RunLevelDesign.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
    struct FLevelDesignCsvFixture
    {
        FString Stats = TEXT("몬스터 ID,몬스터 이름,Blueprint 클래스 경로,분류 태그,최대 HP,AP,SAP,속도,이동거리(타일),스킬 경로,공격 피해(참고),공격 선딜(초·참고),비고\n");
        FString Weights = TEXT("몬스터 ID,출현 가중치,전체 후보 기준 확률(%),활성 여부,비고\n");
        FString Levels = TEXT("묶음,구간 이름,필수 태그,제외 태그,필수 선봉 태그,기준 몬스터 수,HP 배율,속도 배율,골드 최소,골드 최대,아군 HP 성장,아군 속도 성장,승리 휴식 회복량,Snapshot 기준 인원,Snapshot HP,Snapshot 속도,설계 의도\n");
        TArray<FString> StatsRows;
        TArray<FString> WeightRows;
        TArray<FString> LevelRows;

        FLevelDesignCsvFixture()
        {
            const TCHAR* Ids[] = {TEXT("ScoutA"), TEXT("ScoutB"), TEXT("ScoutC"), TEXT("Brute"), TEXT("Boss"), TEXT("Zero"), TEXT("Disabled")};
            const TCHAR* Percentages[] = {TEXT("6.666667"), TEXT("13.333333"), TEXT("20"), TEXT("26.666667"), TEXT("33.333333"), TEXT("0"), TEXT("0")};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ids); ++Index)
            {
                FString Tags = Index == 2 ? TEXT("Monster.Role.Common|Monster.Region.Swamp") : TEXT("Monster.Role.Common|Monster.Region.Forest");
                if (Index == 3) Tags += TEXT("|Monster.Role.Brute");
                if (Index == 4) Tags += TEXT("|Monster.Role.Boss");
                if (Index == 6) Tags += TEXT("|Monster.Development");
                const FString Row = FString::Printf(TEXT("%s,시험 %s,/Game/User_JeHoon/Validation/T12/BP_%s.BP_%s_C,%s,90,2,1,5,1,/Game/User_JeHoon/Validation/T12/DA_Attack.DA_Attack,50,0.3,fixture\n"), Ids[Index], Ids[Index], Ids[Index], Ids[Index], *Tags);
                StatsRows.Add(Row);
                Stats += Row;
                const int32 Weight = Index < 5 ? Index + 1 : Index == 5 ? 0 : 100;
                const FString WeightRow = FString::Printf(TEXT("%s,%d,%s,%d,fixture\n"), Ids[Index], Weight, Percentages[Index], Index == 6 ? 0 : 1);
                WeightRows.Add(WeightRow);
                Weights += WeightRow;
            }
            for (int32 Index = 0; Index < 10; ++Index)
            {
                const TCHAR* Leader = Index == 4 ? TEXT("Monster.Role.Brute") : Index == 9 ? TEXT("Monster.Role.Boss") : TEXT("");
                const int32 Count = 1 + Index / 3;
                const FString Row = FString::Printf(TEXT("%d,구간 %d,Monster.Role.Common|Monster.Region.Forest,Monster.Development,%s,%d,1.25,1.2,3,7,5,1,25,%d,85,7,fixture\n"), Index + 1, Index + 1, Leader, Count, Count);
                LevelRows.Add(Row);
                Levels += Row;
            }
        }

        FRunTargetState EmptyTarget() const
        {
            FRunTargetState State;
            State.SchemaVersion = 1;
            State.Groups.SetNum(10);
            const FName Classes[] = {TEXT("Warrior"), TEXT("Mage"), TEXT("Archer"), TEXT("Rogue")};
            for (int32 GroupIndex = 0; GroupIndex < State.Groups.Num(); ++GroupIndex)
            {
                FTargetRunGroup& Group = State.Groups[GroupIndex];
                Group.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Run.Content.Development")));
                Group.Opponent.SnapshotId = FName(*FString::Printf(TEXT("FixtureSnapshot_%d"), GroupIndex));
                for (int32 Index = 0; Index < UE_ARRAY_COUNT(Classes); ++Index)
                {
                    FPartySnapshotMember& Member = Group.Opponent.Members.AddDefaulted_GetRef();
                    Member.MemberId = FName(*FString::Printf(TEXT("Member_%d"), Index));
                    Member.ClassId = Classes[Index];
                    Member.CharacterName = TEXT("Fixture");
                    Member.FormationSlot = Index;
                    Member.SkillIds.Add(TEXT("DefaultAttack"));
                }
            }
            return State;
        }

        bool Load(FRunTargetState& State, int32 PartySize, int32 Seed, FText& Error) const
        {
            return RunLevelDesign::LoadFromStrings(Stats, Weights, Levels, Seed, PartySize, State, Error);
        }
    };

    bool SameLevelDesignTarget(const FRunTargetState& Left, const FRunTargetState& Right)
    {
        return FRunTargetState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLevelDesignStrictCsvTest, "ProjectA.Run.LevelDesign.StrictCsvJoinsAndAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunLevelDesignStrictCsvTest::RunTest(const FString& Parameters)
{
    const FLevelDesignCsvFixture Fixture;
    FRunTargetState Valid = Fixture.EmptyTarget();
    FText Error;
    if (!TestTrue(TEXT("Synthetic paths support pure CSV validation without loading their assets"), Fixture.Load(Valid, 4, 12345, Error) && RunLevelDesign::Validate(Valid, Error))) return false;
    const auto Reject = [this, &Fixture, &Error](const TCHAR* Label, const FLevelDesignCsvFixture& Invalid, int32 PartySize = 4)
    {
        FRunTargetState Candidate = Fixture.EmptyTarget();
        Candidate.EncounterSeed = 7654;
        const FRunTargetState Before = Candidate;
        TestFalse(Label, Invalid.Load(Candidate, PartySize, 90210, Error));
        TestTrue(TEXT("Rejected CSV returns an explanation and leaves the complete initial target unchanged"), !Error.IsEmpty() && SameLevelDesignTarget(Candidate, Before));
    };
    FLevelDesignCsvFixture Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT("최대 HP"), TEXT("WrongHeader"));
    Reject(TEXT("Unexpected stats header is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT(",90,2,1,5,1,"), TEXT(",,2,1,5,1,"));
    Reject(TEXT("Missing required HP is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT(",90,2,1,5,1,"), TEXT(",-1,2,1,5,1,"));
    Reject(TEXT("Negative HP is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT(",90,2,1,5,1,"), TEXT(",90,1.5,1,5,1,"));
    Reject(TEXT("Fractional AP is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT(",90,2,1,5,1,"), TEXT(",90,2,1,nan,1,"));
    Reject(TEXT("Nonfinite speed is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT("Monster.Region.Forest"), TEXT("Monster.UnknownFixture"));
    Reject(TEXT("Unregistered content tags are rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT("Monster.Region.Forest"), TEXT("Monster.Role.Common"));
    Reject(TEXT("Duplicate tags are rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats += Fixture.StatsRows[0];
    Reject(TEXT("Duplicate monster IDs and classes are rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(TEXT("BP_ScoutB.BP_ScoutB_C"), TEXT("BP_ScoutA.BP_ScoutA_C"));
    Reject(TEXT("Distinct IDs cannot alias the same monster class"), Invalid);
    Invalid = Fixture;
    Invalid.Stats.ReplaceInline(*Fixture.StatsRows[0], TEXT(""));
    Reject(TEXT("A weight without its monster definition is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(*Fixture.WeightRows[0], TEXT(""));
    Reject(TEXT("A monster without its weight row is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(TEXT("ScoutA,1,6.666667,1"), TEXT("Ghost,1,6.666667,1"));
    Reject(TEXT("An unknown weight ID is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights += Fixture.WeightRows[0];
    Reject(TEXT("Duplicate weight IDs are rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(TEXT("ScoutA,1,6.666667,1"), TEXT("ScoutA,-1,6.666667,1"));
    Reject(TEXT("Negative weight is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(TEXT("ScoutA,1,6.666667,1"), TEXT("ScoutA,nan,6.666667,1"));
    Reject(TEXT("Nonfinite weight is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(TEXT("ScoutA,1,6.666667,1"), TEXT("ScoutA,1,99,1"));
    Reject(TEXT("An incorrect displayed normalization is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(TEXT("ScoutA,1,6.666667,1"), TEXT("ScoutA,1,6.666667,2"));
    Reject(TEXT("An unsupported enabled flag is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Weights.ReplaceInline(TEXT(",1,fixture\n"), TEXT(",0,fixture\n"));
    Reject(TEXT("An entirely disabled pool is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Levels.ReplaceInline(*Fixture.LevelRows.Last(), TEXT(""));
    Reject(TEXT("All ten level rows are required"), Invalid);
    Invalid = Fixture;
    Invalid.Levels += Fixture.LevelRows.Last();
    Reject(TEXT("An extra or duplicate level row is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Levels.ReplaceInline(TEXT(",1.25,1.2,3,7,"), TEXT(",0,1.2,3,7,"));
    Reject(TEXT("A zero HP multiplier is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Levels.ReplaceInline(TEXT(",1.25,1.2,3,7,"), TEXT(",1.25,1.2,8,7,"));
    Reject(TEXT("An inverted gold range is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Levels.ReplaceInline(TEXT(",3,7,5,1,25,"), TEXT(",3,7,5,1,nan,"));
    Reject(TEXT("Nonfinite rest HP is rejected"), Invalid);
    Invalid = Fixture;
    Invalid.Levels.ReplaceInline(TEXT(",,1,1.25,"), TEXT(",,5,1.25,"));
    Reject(TEXT("A baseline roster cannot exceed four slots"), Invalid);
    Invalid = Fixture;
    Invalid.Levels.ReplaceInline(TEXT("Monster.Role.Brute"), TEXT("Monster.Role.Skirmisher"));
    Reject(TEXT("A required leader without a positive eligible candidate is rejected"), Invalid);
    Reject(TEXT("Zero original party members are rejected"), Fixture, 0);
    Reject(TEXT("Five original party members are rejected"), Fixture, 5);
    const FRunTargetState BeforeReload = Valid;
    TestFalse(TEXT("CSV loading cannot replace an already frozen level policy"), Fixture.Load(Valid, 4, 67890, Error));
    TestTrue(TEXT("Refusing CSV replacement preserves the original seed catalog rules and lineups"), SameLevelDesignTarget(Valid, BeforeReload));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLevelDesignSelectionTest, "ProjectA.Run.LevelDesign.PartyScalingTagsAndSeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunLevelDesignSelectionTest::RunTest(const FString& Parameters)
{
    const FLevelDesignCsvFixture Fixture;
    FText Error;
    for (int32 PartySize = 1; PartySize <= 4; ++PartySize)
    {
        FRunTargetState State = Fixture.EmptyTarget();
        FRunTargetState SameSeed = Fixture.EmptyTarget();
        if (!TestTrue(TEXT("Every supported original party size can freeze a complete seeded Run"), Fixture.Load(State, PartySize, 271828, Error) && Fixture.Load(SameSeed, PartySize, 271828, Error) && RunLevelDesign::Validate(State, Error))) return false;
        TestTrue(TEXT("The same inputs, seed and original party size reproduce the full frozen state"), SameLevelDesignTarget(State, SameSeed));
        TestEqual(TEXT("The original party size is stored independently of subsequent casualties"), State.LevelDesign.PartySize, PartySize);
        for (int32 GroupIndex = 0; GroupIndex < State.Groups.Num(); ++GroupIndex)
        {
            const FTargetRunGroup& Group = State.Groups[GroupIndex];
            const FRunLevelRule& Rule = State.LevelDesign.Rules[GroupIndex];
            const int32 ExpectedCount = (Rule.EnemyCount * PartySize + 3) / 4;
            TestEqual(TEXT("Roster size uses the rounded-up fraction of the four-person baseline"), Group.EnemyRoster.Num(), ExpectedCount);
            TestEqual(TEXT("Snapshot size uses the same original-party scaling"), Group.Opponent.Members.Num(), (Rule.SnapshotCount * PartySize + 3) / 4);
            TestEqual(TEXT("Enemy class slots and frozen stat slots stay aligned"), Group.EnemyClasses.Num(), ExpectedCount);
            TSet<FName> Ids;
            TSet<FSoftClassPath> Classes;
            for (int32 Slot = 0; Slot < Group.EnemyRoster.Num(); ++Slot)
            {
                const FRunMonsterDefinition& Enemy = Group.EnemyRoster[Slot];
                const FRunMonsterDefinition* Base = State.LevelDesign.Catalog.FindByPredicate([&Enemy](const FRunMonsterDefinition& Candidate) { return Candidate.MonsterId == Enemy.MonsterId; });
                if (!TestNotNull(TEXT("Every selected identity resolves to its frozen catalog"), Base)) return false;
                TestTrue(TEXT("Selections obey the common tag query and positive weight"), Base->Weight > 0.f && Rule.EnemyQuery.Matches(Base->Tags));
                TestTrue(TEXT("One encounter never repeats an identity or class"), !Ids.Contains(Enemy.MonsterId) && !Classes.Contains(Enemy.UnitClass));
                TestTrue(TEXT("Zero-weight disabled and positive-weight wrong-region definitions never enter a lineup"), Enemy.MonsterId != TEXT("Zero") && Enemy.MonsterId != TEXT("Disabled") && Enemy.MonsterId != TEXT("ScoutC"));
                TestTrue(TEXT("Class and skill remain those of the selected catalog definition"), Enemy.UnitClass == Base->UnitClass && Group.EnemyClasses[Slot] == Enemy.UnitClass && Enemy.Skill == Base->Skill);
                TestEqual(TEXT("Version one rounds scaled HP upward after applying the original-party coefficient"), Enemy.MaxHP, FMath::CeilToFloat(Base->MaxHP * Rule.HPScale * (0.4f + 0.15f * PartySize)));
                TestEqual(TEXT("Speed scaling is applied only to the frozen copy"), Enemy.Speed, Base->Speed * Rule.SpeedScale);
                TestTrue(TEXT("AP SAP and movement remain authored values"), Enemy.AP == Base->AP && Enemy.SAP == Base->SAP && Enemy.MoveRange == Base->MoveRange);
                Ids.Add(Enemy.MonsterId);
                Classes.Add(Enemy.UnitClass);
            }
            if (!Rule.LeaderQuery.IsEmpty()) TestTrue(TEXT("The first slot satisfies the required leader query even for a one-person party"), !Group.EnemyRoster.IsEmpty() && Rule.LeaderQuery.Matches(Group.EnemyRoster[0].Tags));
            for (const FPartySnapshotMember& Member : Group.Opponent.Members) TestTrue(TEXT("Snapshot members receive the frozen per-level HP and speed"), Member.Stats.MaxHP == Rule.SnapshotHP && Member.Stats.CurrentHP == Rule.SnapshotHP && Member.Stats.Speed == Rule.SnapshotSpeed);
            TestTrue(TEXT("Frozen growth and gold endpoints match the level policy"), Group.MaxHPGrowth == Rule.MaxHPGrowth && Group.SpeedGrowth == Rule.SpeedGrowth && Group.GoldChoices.Num() == 3 && Group.GoldChoices[0] == Rule.GoldMin && Group.GoldChoices.Last() == Rule.GoldMax);
        }
        const FRunTargetState BeforeBuild = State;
        TestTrue(TEXT("Rebuilding from frozen inputs is deterministic and idempotent"), RunLevelDesign::Build(State, Error) && SameLevelDesignTarget(State, BeforeBuild));
    }
    FRunTargetState Exhausted = Fixture.EmptyTarget();
    if (!Fixture.Load(Exhausted, 4, 42, Error)) return false;
    for (FRunMonsterDefinition& Monster : Exhausted.LevelDesign.Catalog) if (Monster.MonsterId != TEXT("Brute") && Monster.MonsterId != TEXT("Boss")) Monster.Weight = 0.f;
    const FRunTargetState BeforeExhaustion = Exhausted;
    TestFalse(TEXT("Three- and four-slot encounters reject exhausted weighted pools"), RunLevelDesign::Build(Exhausted, Error));
    TestTrue(TEXT("A failed rebuild cannot partially replace earlier groups"), SameLevelDesignTarget(Exhausted, BeforeExhaustion));
    FRunTargetState SeedBaseline = Fixture.EmptyTarget();
    if (!Fixture.Load(SeedBaseline, 4, 0, Error)) return false;
    bool bSeedChangesRoster = false;
    for (int32 Seed = 1; Seed <= 8; ++Seed)
    {
        FRunTargetState DifferentSeed = Fixture.EmptyTarget();
        if (!Fixture.Load(DifferentSeed, 4, Seed, Error)) return false;
        TestEqual(TEXT("The supplied seed is frozen exactly"), DifferentSeed.LevelDesign.Seed, Seed);
        for (int32 GroupIndex = 0; GroupIndex < DifferentSeed.Groups.Num(); ++GroupIndex) bSeedChangesRoster |= DifferentSeed.Groups[GroupIndex].EnemyClasses != SeedBaseline.Groups[GroupIndex].EnemyClasses;
    }
    TestTrue(TEXT("Distinct fixed seeds affect selection rather than being stored as unused metadata"), bSeedChangesRoster);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLevelDesignSavedStateTest, "ProjectA.Run.LevelDesign.FrozenSerializationLegacyAndTampering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunLevelDesignSavedStateTest::RunTest(const FString& Parameters)
{
    const FLevelDesignCsvFixture Fixture;
    FText Error;
    FRunTargetState Legacy = Fixture.EmptyTarget();
    const FRunTargetState BeforeLegacy = Legacy;
    TestTrue(TEXT("Version zero keeps legacy groups with empty additional policy and rosters"), RunLevelDesign::Validate(Legacy, Error) && SameLevelDesignTarget(Legacy, BeforeLegacy));
    Legacy.LevelDesign.Seed = 1;
    TestFalse(TEXT("Version zero cannot conceal extra policy metadata"), RunLevelDesign::Validate(Legacy, Error));
    Legacy = BeforeLegacy;
    Legacy.Groups[0].EnemyRoster.AddDefaulted();
    TestFalse(TEXT("Version zero cannot contain a versioned monster roster"), RunLevelDesign::Validate(Legacy, Error));
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->TargetRun = Fixture.EmptyTarget();
    if (!TestTrue(TEXT("A versioned policy freezes before serialization"), Fixture.Load(Save->TargetRun, 4, 8675309, Error))) return false;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The target policy catalog rules and generated lineups serialize together"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The frozen SaveGame restores"), Restored.Get())) return false;
    TestTrue(TEXT("Validation needs neither source CSV contents nor loadable synthetic assets"), RunLevelDesign::Validate(Restored->TargetRun, Error) && SameLevelDesignTarget(Restored->TargetRun, Save->TargetRun));
    const auto Reject = [this, &Error](FRunTargetState Invalid, const TCHAR* Label)
    {
        const FRunTargetState Before = Invalid;
        TestFalse(Label, RunLevelDesign::Validate(Invalid, Error));
        TestTrue(TEXT("Validation does not repair or mutate a malformed saved policy"), !Error.IsEmpty() && SameLevelDesignTarget(Invalid, Before));
    };
    FRunTargetState Invalid = Restored->TargetRun;
    Invalid.Groups[0].EnemyRoster[0].MaxHP += 0.25f;
    Reject(Invalid, TEXT("A finite fractional HP edit is rejected by exact seeded reconstruction"));
    Invalid = Restored->TargetRun;
    Invalid.Groups[0].EnemyRoster[0].Speed += 0.25f;
    Reject(Invalid, TEXT("A finite fractional speed edit is rejected"));
    Invalid = Restored->TargetRun;
    Invalid.Groups[0].EnemyRoster[0].MaxHP = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite saved combat stats are rejected"), RunLevelDesign::Validate(Invalid, Error));
    Invalid = Restored->TargetRun;
    Invalid.Groups.Last().EnemyRoster[1] = Invalid.Groups.Last().EnemyRoster[0];
    Reject(Invalid, TEXT("A duplicated saved monster is rejected"));
    Invalid = Restored->TargetRun;
    Invalid.Groups[0].EnemyClasses[0] = FSoftClassPath(TEXT("/Game/User_JeHoon/Validation/T12/BP_Changed.BP_Changed_C"));
    Reject(Invalid, TEXT("Class slots cannot diverge from their generated stat roster"));
    Invalid = Restored->TargetRun;
    Invalid.Groups[0].Opponent.Members[0].Stats.MaxHP += 1.f;
    Reject(Invalid, TEXT("A saved Snapshot stat edit is rejected"));
    Invalid = Restored->TargetRun;
    ++Invalid.Groups[0].GoldChoices[0];
    Reject(Invalid, TEXT("Saved reward endpoints must match frozen rules"));
    Invalid = Restored->TargetRun;
    Invalid.LevelDesign.PartySize = 1;
    Reject(Invalid, TEXT("Original party-size tampering cannot silently shrink an already saved roster"));
    Invalid = Restored->TargetRun;
    Invalid.LevelDesign.SchemaVersion = 2;
    Reject(Invalid, TEXT("Unknown policy versions are rejected"));
    return true;
}

#endif
