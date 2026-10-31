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
        View.Phase = ECombatRoundPhase::Resolving;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            FCombatRoundUnitView& Unit = View.Units.AddDefaulted_GetRef();
            Unit.UnitId = Index + 1;
            Unit.OwnerSlot = Index < 2 ? 1 : 2;
            Unit.bEnemy = Index == 3;
            Unit.HP = 100.f;
            Unit.Command.UnitId = Unit.UnitId;
            Unit.bReady = true;
            Unit.ActionPhase = ECombatRoundActionPhase::Casting;
        }
        return View;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusOwnershipTest, "ProjectA.Combat.ShoulderCamera.LocalOwnershipAndReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusOwnershipTest::RunTest(const FString& Parameters)
{
    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    View.Phase = ECombatRoundPhase::Planning;
    View.Units[0].bReady = false;
    View.Units[1].bReady = false;
    TestEqual(TEXT("Unacknowledged Ready never starts a shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Units[0].bReady = true;
    View.Units[1].bReady = true;
    TestEqual(TEXT("Server-acknowledged Ready retains the tactical camera."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Phase = ECombatRoundPhase::Resolving;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Waiting;
    TestEqual(TEXT("The start delay retains the tactical camera."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Casting;
    TestEqual(TEXT("The selected action starts the shoulder camera."), Focus.Update(View, 1, 2), 2);
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
    TestEqual(TEXT("Action start establishes one character for this action."), Focus.Update(View, 1, 2), 2);
    View.Phase = ECombatRoundPhase::Resolving;
    View.Units[1].bHasMovePlan = true;
    const TArray<ECombatRoundActionPhase> ActivePhases = {ECombatRoundActionPhase::Approaching, ECombatRoundActionPhase::Casting, ECombatRoundActionPhase::Recovery};
    FCombatRoundSkill Melee;
    Melee.Kind = ECombatRoundSkillKind::Melee;
    for (const ECombatRoundActionPhase Phase : ActivePhases)
    {
        View.Units[1].ActionPhase = Phase;
        TestEqual(TEXT("SAP waiting, approach, attack and animation recovery retain the selected character."), Focus.Update(View, 1, 1), 2);
        TestFalse(TEXT("A melee hit cannot end the shot before animation recovery finishes."), Focus.FinishAtReturnPoint(View.Units[1], &Melee));
    }
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Returning;
    TestTrue(TEXT("Melee return ends the shot before any travel home."), Focus.FinishAtReturnPoint(View.Units[1], &Melee));
    TestEqual(TEXT("The selected returning melee unit cannot restart the shot."), Focus.Update(View, 1, 1), INDEX_NONE);
    View.Units[0].ActionPhase = ECombatRoundActionPhase::Casting;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Complete;
    TestEqual(TEXT("The selected character completing returns before another owned action settles."), Focus.Update(View, 1, 1), INDEX_NONE);
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Waiting;
    TestEqual(TEXT("A finished shot cannot restart or switch characters during the same resolution."), Focus.Update(View, 1, 1), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusImpactTest, "ProjectA.Combat.ShoulderCamera.RangedAndMagicImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusImpactTest::RunTest(const FString& Parameters)
{
    const TArray<ECombatRoundSkillKind> Kinds = {ECombatRoundSkillKind::Projectile, ECombatRoundSkillKind::GroundAttack};
    for (const ECombatRoundSkillKind Kind : Kinds)
    {
        FCombatRoundView View = CombatShoulderFocusTests::MakeView();
        FCombatShoulderFocus Focus;
        FCombatRoundSkill Skill;
        Skill.Kind = Kind;
        TestEqual(TEXT("The ranged or magic shot starts with its accepted owner."), Focus.Update(View, 1, 2), 2);
        View.Phase = ECombatRoundPhase::Resolving;
        View.Units[1].ActionPhase = ECombatRoundActionPhase::Recovery;
        View.Units[1].bAwaitingSkillImpact = true;
        TestFalse(TEXT("Release and recovery do not substitute for an impact."), Focus.FinishAtReturnPoint(View.Units[1], &Skill));
        View.Units[1].ActionPhase = ECombatRoundActionPhase::Complete;
        TestEqual(TEXT("A finished cast keeps the shot while its projectile or effect is pending."), Focus.Update(View, 1, 1), 2);
        View.Units[0].bSkillImpactSettled = true;
        TestFalse(TEXT("Another owned unit's impact cannot end this shot."), Focus.FinishAtReturnPoint(View.Units[0], &Skill));
        View.Units[1].bSkillImpactSettled = true;
        View.Units[1].bAwaitingSkillImpact = false;
        TestEqual(TEXT("An impact after cast completion returns without waiting for other actions."), Focus.Update(View, 1, 1), INDEX_NONE);
        View.Units[1].ActionPhase = ECombatRoundActionPhase::Recovery;
        TestEqual(TEXT("Later chain hits cannot reopen the same shot."), Focus.Update(View, 1, 1), INDEX_NONE);

        Focus.Reset();
        View.Units[1].bSkillImpactSettled = false;
        View.Units[1].bAwaitingSkillImpact = true;
        View.Units[1].ActionPhase = ECombatRoundActionPhase::Casting;
        TestEqual(TEXT("A fresh shot begins at the cast before impact."), Focus.Update(View, 1, 2), 2);
        View.Units[1].ActionPhase = ECombatRoundActionPhase::Recovery;
        TestEqual(TEXT("Recovery preserves a shot that already began."), Focus.Update(View, 1, 2), 2);
        View.Units[1].bSkillImpactSettled = true;
        View.Units[1].bAwaitingSkillImpact = false;
        TestTrue(TEXT("The first impact ends the shot even while the cast animation is recovering."), Focus.FinishAtReturnPoint(View.Units[1], &Skill));
        TestEqual(TEXT("The remaining cast animation cannot restart the camera."), Focus.Update(View, 1, 2), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderFocusMissTest, "ProjectA.Combat.ShoulderCamera.MissCancellationAndLateReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderFocusMissTest::RunTest(const FString& Parameters)
{
    FCombatRoundView View = CombatShoulderFocusTests::MakeView();
    FCombatShoulderFocus Focus;
    FCombatRoundSkill Skill;
    Skill.Kind = ECombatRoundSkillKind::Projectile;
    TestEqual(TEXT("A pending shot begins with the selected owner."), Focus.Update(View, 1, 2), 2);
    View.Phase = ECombatRoundPhase::Resolving;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Recovery;
    View.Units[1].bSkillImpactSettled = true;
    TestTrue(TEXT("A final miss or expired effect ends the shot without an enemy hit."), Focus.FinishAtReturnPoint(View.Units[1], &Skill));
    TestEqual(TEXT("A missed shot cannot attach to another owned action."), Focus.Update(View, 1, 1), INDEX_NONE);
    Focus.Reset();
    View.Units[1].bSkillImpactSettled = false;
    View.Units[1].bAwaitingSkillImpact = true;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Cancelled;
    TestEqual(TEXT("Cancellation returns even if an effect is still pending."), Focus.Update(View, 1, 2), INDEX_NONE);
    Focus.Reset();
    Skill.Kind = ECombatRoundSkillKind::Melee;
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Returning;
    TestEqual(TEXT("A late returning action never starts a shoulder shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    TestTrue(TEXT("A late melee return is rejected before a shoulder camera is spawned."), Focus.FinishAtReturnPoint(View.Units[1], &Skill));
    TestEqual(TEXT("Late return cannot switch to another participant's action."), Focus.Update(View, 1, 3), INDEX_NONE);
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
    TestEqual(TEXT("Action start follows the first selected character."), Focus.Update(View, 1, 1), 1);
    View.Phase = ECombatRoundPhase::Planning;
    View.Units[0].bReady = false;
    View.Units[1].bReady = false;
    ++View.PlanRevision;
    TestEqual(TEXT("Acknowledged Ready cancellation restores the overview."), Focus.Update(View, 1, 2), INDEX_NONE);
    TestEqual(TEXT("Editing the preferred unit while not ready cannot start another shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Units[0].bReady = true;
    View.Units[1].bReady = true;
    ++View.PlanRevision;
    TestEqual(TEXT("A new Ready still retains the tactical camera."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Phase = ECombatRoundPhase::Resolving;
    TestEqual(TEXT("The newly selected character is followed when its action starts."), Focus.Update(View, 1, 2), 2);
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
    TestEqual(TEXT("A client first observing a waiting action stays tactical."), Focus.Update(View, 1, 2), INDEX_NONE);
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Casting;
    TestEqual(TEXT("The client enters the selected shot once casting starts."), Focus.Update(View, 1, 2), 2);
    Focus.Reset();
    View.Units[1].ActionPhase = ECombatRoundActionPhase::Complete;
    TestEqual(TEXT("A pass already complete before the first camera update does not flash a shot."), Focus.Update(View, 1, 2), INDEX_NONE);
    TestEqual(TEXT("A completed pass does not switch to another owned active unit."), Focus.Update(View, 1, 1), INDEX_NONE);
    return true;
}

#endif
