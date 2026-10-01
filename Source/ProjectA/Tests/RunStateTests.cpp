#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Tests/RunRewardTestHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPartyValidationTest, "ProjectA.VerticalSlice.Run.PartyValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunPartyValidationTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    URunStateSubsystem* RunState = NewObject<URunStateSubsystem>(GameInstance);
    TArray<FRunPartyMember> Members;
    FText Error;
    TestFalse(TEXT("An empty party cannot start"), RunState->InitializeRun(Members, Error));
    TestFalse(TEXT("Validation returns a visible explanation"), Error.IsEmpty());

    FRunPartyMember Member;
    Member.SlotIndex = 2;
    Member.CharacterName = FText::FromString(TEXT("Archer Two"));
    Member.ClassId = TEXT("Archer");
    Member.bCreated = true;
    Members.Add(Member);

    FRunPartyMember EmptySlot;
    EmptySlot.SlotIndex = 0;
    Members.Add(EmptySlot);
    TestTrue(TEXT("One created member and empty slots are valid"), RunState->InitializeRun(Members, Error));
    TestEqual(TEXT("An empty slot is preserved"), RunState->GetPartyMembers()[0].SlotIndex, 0);
    TestFalse(TEXT("The empty slot remains uncreated"), RunState->GetPartyMembers()[0].bCreated);
    TestEqual(TEXT("Created member retains original slot index"), RunState->GetPartyMembers()[1].SlotIndex, 2);
    TestEqual(TEXT("Created member retains selected class"), RunState->GetPartyMembers()[1].ClassId, FName(TEXT("Archer")));
    TestEqual(TEXT("Created member retains name"), RunState->GetPartyMembers()[1].CharacterName.ToString(), FString(TEXT("Archer Two")));

    Members.Add(Member);
    TestFalse(TEXT("Duplicate slots are rejected"), RunState->InitializeRun(Members, Error));
    TestEqual(TEXT("Rejected initialization preserves previous party"), RunState->GetPartyMembers().Num(), 2);
    Members.RemoveAt(2);
    Members[0].ClassId = NAME_None;
    TestFalse(TEXT("Created member without a class is rejected"), RunState->InitializeRun(Members, Error));
    Members[0].ClassId = TEXT("Archer");
    Members[0].CharacterName = FText::FromString(TEXT("  "));
    TestFalse(TEXT("Created member without a readable name is rejected"), RunState->InitializeRun(Members, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunProgressionTest, "ProjectA.VerticalSlice.Run.Progression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunProgressionTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    URunStateSubsystem* RunState = NewObject<URunStateSubsystem>(GameInstance);
    FRunPartyMember Member;
    Member.SlotIndex = 3;
    Member.CharacterName = FText::FromString(TEXT("Mage"));
    Member.ClassId = TEXT("Mage");
    Member.bCreated = true;
    FText Error;
    TestTrue(TEXT("Initialize a new run"), RunState->InitializeRun({ Member }, Error));
    if (!TestEqual(TEXT("A new Run contains ten combat nodes"), RunState->GetNodes().Num(), 10)) return false;
    const FName FirstNode = RunState->GetNodes()[0].NodeId;
    const FName SecondNode = RunState->GetNodes()[1].NodeId;
    TestFalse(TEXT("A locked node cannot be skipped to"), RunState->BeginEncounter(SecondNode));
    TestTrue(TEXT("The first node starts preparation"), RunState->BeginEncounter(FirstNode));
    TestFalse(TEXT("Duplicate node start is rejected"), RunState->BeginEncounter(FirstNode));
    TestTrue(TEXT("Preparation failure returns to map"), RunState->AbortEncounter());
    TestTrue(TEXT("Failed preparation can be retried"), RunState->BeginEncounter(FirstNode));
    int32 ShopVisits = 0;
    for (int32 Index = 0; Index < RunState->GetNodes().Num(); ++Index)
    {
        const FName Node = RunState->GetNodes()[Index].NodeId;
        TestEqual(TEXT("Node identifiers retain their ordered combat numbering"), Node, FName(*FString::Printf(TEXT("Combat_%02d"), Index + 1)));
        if (Index > 0 && !TestTrue(TEXT("The next unlocked encounter can start"), RunState->BeginEncounter(Node))) return false;
        for (int32 Other = 0; Other < RunState->GetNodes().Num(); ++Other) TestFalse(TEXT("Preparing prevents every node from starting again"), RunState->CanStartNode(RunState->GetNodes()[Other].NodeId));
        if (!TestTrue(TEXT("Prepared combat starts"), RunState->MarkCombatStarted())) return false;
        RunState->UpdatePartyMemberHP(3, 87.0f);
        if (!TestTrue(TEXT("Each victory reaches its own result"), RunState->CompleteEncounter(ECombatResult::Victory))) return false;
        TestEqual(TEXT("Victory advances exactly one node"), RunState->GetCompletedNodes().Num(), Index + 1);
        TestFalse(TEXT("Duplicate combat result is rejected"), RunState->CompleteEncounter(ECombatResult::Victory));
        TestFalse(TEXT("A completed result cannot be aborted"), RunState->AbortEncounter());
        TestEqual(TEXT("Remaining HP is stored by original slot"), RunState->GetPartyMembers()[0].CurrentHP, 87.0f);
        TestFalse(TEXT("Every victory waits for its reward before Continue"), RunState->ContinueRun());
        if (!TestTrue(TEXT("Each victory reward is collected"), RunRewardTests::CollectPendingGoldRewards(RunState))) return false;
        if (!TestTrue(TEXT("Collected rewards permit Continue"), RunState->ContinueRun())) return false;
        if (Index + 1 == RunState->GetNodes().Num())
        {
            TestTrue(TEXT("The tenth victory completes without another shop"), RunState->GetPhase() == ERunPhase::Complete);
            continue;
        }
        const FName NextNode = RunState->GetNodes()[Index + 1].NodeId;
        if (!TestTrue(TEXT("Every nonfinal victory opens a fresh encounter choice"), RunState->GetPhase() == ERunPhase::EncounterChoice)) return false;
        TestTrue(TEXT("The next shop clears the previous selection and completion"), RunState->GetEncounterProgress().SelectedEncounterId.IsNone() && !RunState->GetEncounterProgress().bCompleted && RunState->GetEncounterProgress().AfterCompletedNodeCount == Index + 1);
        TestFalse(TEXT("The next battle cannot bypass a Run encounter"), RunState->BeginEncounter(NextNode));
        TestFalse(TEXT("An encounter cannot be left before selecting a shop"), RunState->LeaveRunEncounter());
        if (!TestTrue(TEXT("A shop can be selected and left"), RunState->SelectRunEncounter(TEXT("Shop_02")) && RunState->LeaveRunEncounter())) return false;
        ++ShopVisits;
        TestTrue(TEXT("Each nonfinal victory returns to the map"), RunState->GetPhase() == ERunPhase::Map);
        for (int32 Other = 0; Other < RunState->GetNodes().Num(); ++Other) TestEqual(TEXT("Only the immediate next node is unlocked"), RunState->CanStartNode(RunState->GetNodes()[Other].NodeId), Other == Index + 1);
        TestFalse(TEXT("Completed nodes cannot be entered again"), RunState->BeginEncounter(Node));
    }
    TestEqual(TEXT("Nine shop visits separate the ten combat nodes"), ShopVisits, 9);
    TestEqual(TEXT("All ten nodes completed"), RunState->GetCompletedNodes().Num(), 10);
    TestTrue(TEXT("Run reaches complete phase"), RunState->GetPhase() == ERunPhase::Complete);
    TestFalse(TEXT("Complete run cannot start another combat"), RunState->BeginEncounter(FirstNode));

    TestTrue(TEXT("New game resets the previous run"), RunState->InitializeRun({ Member }, Error));
    TestEqual(TEXT("New game resets progress"), RunState->GetCompletedNodes().Num(), 0);
    TestTrue(TEXT("Defeat encounter starts"), RunState->BeginEncounter(FirstNode));
    TestTrue(TEXT("Defeat combat starts"), RunState->MarkCombatStarted());
    TestTrue(TEXT("Defeat is recorded"), RunState->CompleteEncounter(ECombatResult::Defeat));
    TestTrue(TEXT("Defeat is terminal"), RunState->GetPhase() == ERunPhase::Defeat);
    TestFalse(TEXT("Defeat cannot continue"), RunState->ContinueRun());
    TestFalse(TEXT("Defeat cannot start another encounter"), RunState->BeginEncounter(FirstNode));
    TestFalse(TEXT("Defeat cannot emit another result"), RunState->CompleteEncounter(ECombatResult::Victory));
    return true;
}

#endif
