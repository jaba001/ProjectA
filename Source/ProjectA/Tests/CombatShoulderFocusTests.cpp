#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/CombatShoulderFocus.h"

namespace CombatShoulderFocusTests
{
    FCombatRoundView MakeView()
    {
        FCombatRoundView View;
        View.CombatId = FGuid(1, 2, 3, 4);
        View.RoundNumber = 1;
        View.Phase = ECombatRoundPhase::Planning;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            FCombatRoundUnitView& Unit = View.Units.AddDefaulted_GetRef();
            Unit.UnitId = Index + 1;
            Unit.OwnerSlot = Index < 2 ? 1 : 2;
            Unit.bEnemy = Index == 3;
            Unit.HP = 100.f;
            Unit.Command.UnitId = Unit.UnitId;
            Unit.bReady = true;
        }
        return View;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusOwnershipTest, "ProjectA.Combat.ShoulderCamera.LocalOwnershipAndReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusOwnershipTest::RunTest(const FString& Parameters)
{
    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    View.Units[0].bReady = false;
    View.Units[1].bReady = false;
    TestEqual(TEXT("Unacknowledged Ready never starts a shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Units[0].bReady = true;
    View.Units[1].bReady = true;
    TestEqual(TEXT("Server-acknowledged Ready follows the preferred owned unit."), Focus.Update(View, 1, 2), 2);
    TestEqual(TEXT("Changing the preferred unit during an accepted shot keeps the same character."), Focus.Update(View, 1, 1), 2);

    Focus.Reset();
    TestEqual(TEXT("Another participant's preferred unit falls back to a living owned character."), Focus.Update(View, 1, 3), 1);
    Focus.Reset();
    View.Units[3].OwnerSlot = 1;
    TestEqual(TEXT("Enemy status excludes a unit even if its slot matches."), Focus.Update(View, 1, 4), 1);
    Focus.Reset();
    View.Units[0].HP = 0.f;
    TestEqual(TEXT("A dead preferred unit cannot become the initial focus."), Focus.Update(View, 1, 1), 2);
    Focus.Reset();
    View.Units[1].HP = 0.f;
    TestEqual(TEXT("No living owned character leaves the overview active."), Focus.Update(View, 1, 3), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusActionLifecycleTest, "ProjectA.Combat.ShoulderCamera.ActionLifecycleAndSingleCharacter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusActionLifecycleTest::RunTest(const FString& Parameters)
{
    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    TestEqual(TEXT("Ready establishes one character for this action."), Focus.Update(View, 1, 2), 2);
    View.Phase = ECombatRoundPhase::Resolving;
    View.Units[1].bHasMovePlan = true;
    const TArray<ECombatRoundActionPhase> ActivePhases = {ECombatRoundActionPhase::Waiting, ECombatRoundActionPhase::Approaching, ECombatRoundActionPhase::Casting, ECombatRoundActionPhase::Recovery, ECombatRoundActionPhase::Returning};
    for (const ECombatRoundActionPhase Phase : ActivePhases)
    {
        View.Units[1].ActionPhase = Phase;
        TestEqual(TEXT("SAP waiting, attack, animation recovery and return retain the selected character."), Focus.Update(View, 1, 1), 2);
    }
    View.Units[0].ActionPhase = ECombatRoundActionPhase::Casting;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Complete;
    TestEqual(TEXT("The selected character completing returns before another owned action settles."), Focus.Update(View, 1, 1), INDEX_NONE);
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Waiting;
    TestEqual(TEXT("A finished shot cannot restart or switch characters during the same resolution."), Focus.Update(View, 1, 1), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusFailureTest, "ProjectA.Combat.ShoulderCamera.CancelDeathAndMissingFocus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusFailureTest::RunTest(const FString& Parameters)
{
    for (int32 Failure = 0; Failure < 5; ++Failure)
    {
        FCombatRoundView View = CombatShoulderFocusTests::MakeView();
        FCombatShoulderFocus Focus;
        TestEqual(TEXT("Each failure begins with the accepted selected character."), Focus.Update(View, 1, 2), 2);
        View.Phase = ECombatRoundPhase::Resolving;
        View.Units[0].ActionPhase = ECombatRoundActionPhase::Casting;
        if (Failure == 0) View.Units[1].ActionPhase = ECombatRoundActionPhase::Cancelled;
        if (Failure == 1) View.Units[1].HP = 0.f;
        if (Failure == 2) View.Units.RemoveAt(1);
        if (Failure == 3) View.Units[1].OwnerSlot = 2;
        if (Failure == 4) View.Units[1].bEnemy = true;
        TestEqual(TEXT("Cancelled, dead, missing or no longer owned focus returns to the overview."), Focus.Update(View, 1, 1), INDEX_NONE);
        TestEqual(TEXT("Other owned active characters cannot replace a failed shot."), Focus.Update(View, 1, 1), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusReadyCancellationTest, "ProjectA.Combat.ShoulderCamera.ReadyCancelAndResubmit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusReadyCancellationTest::RunTest(const FString& Parameters)
{
    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    TestEqual(TEXT("Ready begins on the first selected character."), Focus.Update(View, 1, 1), 1);
    View.Units[0].bReady = false;
    View.Units[1].bReady = false;
    ++View.PlanRevision;
    TestEqual(TEXT("Acknowledged Ready cancellation restores the overview."), Focus.Update(View, 1, 2), INDEX_NONE);
    TestEqual(TEXT("Editing the preferred unit while not ready cannot start another shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Units[0].bReady = true;
    View.Units[1].bReady = true;
    ++View.PlanRevision;
    TestEqual(TEXT("A new Ready in the same round may follow the newly selected character."), Focus.Update(View, 1, 2), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusSessionTest, "ProjectA.Combat.ShoulderCamera.RoundCombatAndOwnerBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusSessionTest::RunTest(const FString& Parameters)
{
    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    TestEqual(TEXT("The initial session chooses the preferred local character."), Focus.Update(View, 1, 1), 1);
    Focus.Finish();
    TestEqual(TEXT("A controller-ended shot cannot restart while the same Ready remains active."), Focus.Update(View, 1, 2), INDEX_NONE);
    ++View.RoundNumber;
    TestEqual(TEXT("The next round can choose another local character."), Focus.Update(View, 1, 2), 2);
    Focus.Finish();
    View.CombatId = FGuid(5, 6, 7, 8);
    TestEqual(TEXT("A new combat resets the completed shot even if its round number matches."), Focus.Update(View, 1, 1), 1);
    TestEqual(TEXT("A changed owner slot resolves only that participant's character."), Focus.Update(View, 2, 3), 3);
    TestEqual(TEXT("An unbound controller never follows any character."), Focus.Update(View, 0, 3), INDEX_NONE);
    TestEqual(TEXT("Rebinding creates a fresh shot under the new owner."), Focus.Update(View, 1, 2), 2);

    Focus.Reset();
    View.CombatId.Invalidate();
    TestEqual(TEXT("An invalid combat identity cannot start a shot."), Focus.Update(View, 1, 1), INDEX_NONE);
    View.CombatId = FGuid(1, 2, 3, 4);
    View.RoundNumber = 0;
    TestEqual(TEXT("An uninitialized round cannot start a shot."), Focus.Update(View, 1, 1), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusStoppedPhaseTest, "ProjectA.Combat.ShoulderCamera.StoppedPhasesAndLateResolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusStoppedPhaseTest::RunTest(const FString& Parameters)
{
    const TArray<ECombatRoundPhase> StoppedPhases = {ECombatRoundPhase::WaitingForPlayers, ECombatRoundPhase::Finished, ECombatRoundPhase::Suspended};
    for (const ECombatRoundPhase Phase : StoppedPhases)
    {
        FCombatRoundView View = CombatShoulderFocusTests::MakeView();
        FCombatShoulderFocus Focus;
        TestEqual(TEXT("A live shot exists before the round stops."), Focus.Update(View, 1, 1), 1);
        View.Phase = Phase;
        TestEqual(TEXT("Stopped and finished phases restore the overview."), Focus.Update(View, 1, 2), INDEX_NONE);
    }

    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    View.Phase = ECombatRoundPhase::Resolving;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Waiting;
    TestEqual(TEXT("A client first observing resolution can still enter the selected character's shot."), Focus.Update(View, 1, 2), 2);
    Focus.Reset();
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Complete;
    TestEqual(TEXT("A pass already complete before the first camera update does not flash a shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    TestEqual(TEXT("A completed pass does not switch to another owned active unit."), Focus.Update(View, 1, 1), INDEX_NONE);
    return true;
}

#endif
