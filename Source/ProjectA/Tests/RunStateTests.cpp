#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"

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
    Member.CharacterName = FText::FromString(TEXT("Hunter Two"));
    Member.ClassId = TEXT("Hunter");
    Member.bCreated = true;
    Members.Add(Member);

    FRunPartyMember EmptySlot;
    EmptySlot.SlotIndex = 0;
    Members.Add(EmptySlot);
    TestTrue(TEXT("One created member and empty slots are valid"), RunState->InitializeRun(Members, Error));
    TestEqual(TEXT("An empty slot is preserved"), RunState->GetPartyMembers()[0].SlotIndex, 0);
    TestFalse(TEXT("The empty slot remains uncreated"), RunState->GetPartyMembers()[0].bCreated);
    TestEqual(TEXT("Created member retains original slot index"), RunState->GetPartyMembers()[1].SlotIndex, 2);
    TestEqual(TEXT("Created member retains selected class"), RunState->GetPartyMembers()[1].ClassId, FName(TEXT("Hunter")));
    TestEqual(TEXT("Created member retains name"), RunState->GetPartyMembers()[1].CharacterName.ToString(), FString(TEXT("Hunter Two")));

    Members.Add(Member);
    TestFalse(TEXT("Duplicate slots are rejected"), RunState->InitializeRun(Members, Error));
    TestEqual(TEXT("Rejected initialization preserves previous party"), RunState->GetPartyMembers().Num(), 2);
    Members.RemoveAt(2);
    Members[0].ClassId = NAME_None;
    TestFalse(TEXT("Created member without a class is rejected"), RunState->InitializeRun(Members, Error));
    Members[0].ClassId = TEXT("Hunter");
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
    Member.CharacterName = FText::FromString(TEXT("Scholar"));
    Member.ClassId = TEXT("Scholar");
    Member.bCreated = true;
    FText Error;
    TestTrue(TEXT("Initialize a new run"), RunState->InitializeRun({ Member }, Error));
    const FName FirstNode = RunState->GetNodes()[0].NodeId;
    const FName SecondNode = RunState->GetNodes()[1].NodeId;
    TestFalse(TEXT("A locked node cannot be skipped to"), RunState->BeginEncounter(SecondNode));
    TestTrue(TEXT("The first node starts preparation"), RunState->BeginEncounter(FirstNode));
    TestFalse(TEXT("Duplicate node start is rejected"), RunState->BeginEncounter(FirstNode));
    TestTrue(TEXT("Preparation failure returns to map"), RunState->AbortEncounter());
    TestTrue(TEXT("Failed preparation can be retried"), RunState->BeginEncounter(FirstNode));
    TestTrue(TEXT("Prepared combat starts"), RunState->MarkCombatStarted());
    RunState->UpdatePartyMemberHP(3, 87.0f);
    TestTrue(TEXT("First victory reaches result"), RunState->CompleteEncounter(ECombatResult::Victory));
    TestFalse(TEXT("Duplicate combat result is rejected"), RunState->CompleteEncounter(ECombatResult::Victory));
    TestFalse(TEXT("A completed result cannot be aborted"), RunState->AbortEncounter());
    TestEqual(TEXT("Remaining HP is stored by original slot"), RunState->GetPartyMembers()[0].CurrentHP, 87.0f);
    TestFalse(TEXT("Next combat waits for Continue"), RunState->CanStartNode(SecondNode));
    TestTrue(TEXT("Victory Continue returns to map"), RunState->ContinueRun());
    TestFalse(TEXT("Completed first node cannot restart"), RunState->CanStartNode(FirstNode));
    TestTrue(TEXT("Second encounter is unlocked"), RunState->BeginEncounter(SecondNode));
    TestTrue(TEXT("Second combat starts"), RunState->MarkCombatStarted());
    TestTrue(TEXT("Second victory is recorded"), RunState->CompleteEncounter(ECombatResult::Victory));
    TestTrue(TEXT("Final Continue finishes run"), RunState->ContinueRun());
    TestEqual(TEXT("All nodes completed"), RunState->GetCompletedNodes().Num(), 2);
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
