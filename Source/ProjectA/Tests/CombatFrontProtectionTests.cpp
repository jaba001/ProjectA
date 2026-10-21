#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Round/CombatPlanValidator.h"
#include "GAS/CombatGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFrontProtectionTest, "ProjectA.CombatRound.FrontProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatFrontProtectionTest::RunTest(const FString& Parameters)
{
    for (bool bEnemyTarget : { false, true })
    {
        CombatPlanValidation::FState State;
        FCombatRoundSkill Skill;
        Skill.SkillId = TEXT("Test.Melee");
        Skill.EffectTags.AddTag(ProjectACombatTags::Skill_Attack_Melee);
        State.Skills.Add(Skill);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            CombatPlanValidation::FUnit& Unit = State.Units.AddDefaulted_GetRef();
            Unit.UnitId = Index;
            Unit.bAlive = true;
            Unit.bEnemy = Index == 0 ? !bEnemyTarget : bEnemyTarget;
            Unit.AP = Unit.SAP = 10;
            Unit.SkillIds.Add(Skill.SkillId);
        }
        State.Units[0].HomeCoord = FIntPoint(0, bEnemyTarget ? 1 : 2);
        State.Units[1].HomeCoord = FIntPoint(1, bEnemyTarget ? 3 : 0);
        State.Units[2].HomeCoord = FIntPoint(1, bEnemyTarget ? 2 : 1);
        FCombatRoundCommand Command;
        Command.UnitId = 0;
        Command.SkillId = Skill.SkillId;
        Command.TargetUnitId = 1;
        FText Error;
        TestFalse(TEXT("Melee cannot select the protected rear unit on either team"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        TestFalse(TEXT("Rejected selection explains the restriction"), Error.IsEmpty());
        Command.TargetUnitId = 2;
        TestTrue(TEXT("The front unit remains selectable"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        Command.TargetUnitId = 1;
        State.Skills[0].EffectTags.Reset();
        TestTrue(TEXT("Execution kind alone does not impose melee protection"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Skills[0].Kind = ECombatRoundSkillKind::Projectile;
        TestTrue(TEXT("Ranged attacks can select the protected rear unit"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Skills[0].EffectTags = Skill.EffectTags;
        TestFalse(TEXT("The melee tag governs selection independently of execution kind"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Units[2].bAlive = false;
        TestTrue(TEXT("A dead front unit no longer protects the rear"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Units[2].bAlive = true;
        State.Units[2].HomeCoord.X = 2;
        TestTrue(TEXT("A different column does not provide protection"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Units[2].HomeCoord.X = 1;
        State.Units[2].bEnemy = !bEnemyTarget;
        TestTrue(TEXT("An opposing front occupant does not provide protection"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Units[2].bEnemy = bEnemyTarget;
        State.Units[2].bHasMovePlan = true;
        State.Units[2].MoveDestinationCoord = FIntPoint(2, bEnemyTarget ? 2 : 1);
        TestFalse(TEXT("An uncommitted move reservation does not remove protection"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        State.Skills[0].TargetRule = ESkillTargetRule::AllyUnit;
        State.Units[0].bEnemy = bEnemyTarget;
        TestTrue(TEXT("Friendly selection remains available"), CombatPlanValidation::ValidateCommand(State, Command, Error));
        TestFalse(TEXT("Coordinates outside the combat grid are never protected"), CombatPlanValidation::IsProtectedByFront(State, bEnemyTarget, FIntPoint(4, bEnemyTarget ? 3 : 0)));
    }
    return true;
}

#endif
