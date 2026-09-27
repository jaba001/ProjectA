#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Round/CombatPlanValidator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSkillFriendlyPlanningTest, "ProjectA.Combat.CatalogSkills.FriendlyTargetPlanning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatalogSkillFriendlyPlanningTest::RunTest(const FString& Parameters)
{
    CombatPlanValidation::FState State;
    FCombatRoundSkill Skill;
    Skill.SkillId = TEXT("CatalogHeal");
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.Approach = ECombatRoundApproach::None;
    Skill.TargetRule = ESkillTargetRule::AllyUnit;
    Skill.bUseEffectCollision = true;
    Skill.TargetLoss = ECombatRoundTargetLoss::Cancel;
    State.Skills.Add(Skill);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        CombatPlanValidation::FUnit& Unit = State.Units.AddDefaulted_GetRef();
        Unit.UnitId = Index;
        Unit.bAlive = true;
        Unit.bEnemy = Index == 2;
        Unit.AP = 1;
        Unit.SkillIds.Add(Skill.SkillId);
    }
    FCombatRoundCommand Command;
    Command.UnitId = 0;
    Command.SkillId = Skill.SkillId;
    Command.TargetUnitId = 1;
    FText Error;
    TestTrue(TEXT("An ally can be selected without an enemy target or a ground tile"), CombatPlanValidation::ValidateCommand(State, Command, Error));
    Command.TargetUnitId = 0;
    TestTrue(TEXT("Self healing is a valid ally target"), CombatPlanValidation::ValidateCommand(State, Command, Error));
    Command.TargetUnitId = 2;
    TestFalse(TEXT("An ally skill rejects an enemy"), CombatPlanValidation::ValidateCommand(State, Command, Error));
    Command.TargetUnitId = 1;
    State.Units[1].bAlive = false;
    TestFalse(TEXT("A healing plan cannot resurrect a dead ally"), CombatPlanValidation::ValidateCommand(State, Command, Error));
    State.Units[1].bAlive = true;
    State.Units[0].AP = 0;
    TestFalse(TEXT("Friendly targeting preserves the AP validation"), CombatPlanValidation::ValidateCommand(State, Command, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSkillLegacyProfileTest, "ProjectA.Combat.CatalogSkills.LegacyAndCollisionProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatalogSkillLegacyProfileTest::RunTest(const FString& Parameters)
{
    FCombatRoundSkill Skill;
    Skill.SkillId = TEXT("ExistingGroundAttack");
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.Approach = ECombatRoundApproach::None;
    TestFalse(TEXT("Legacy ground definitions retain tile targeting"), CombatRoundRules::UsesUnitTarget(Skill));
    TestTrue(TEXT("The legacy profile remains valid"), CombatRoundRules::IsValidSkill(Skill));
    Skill.bUseEffectCollision = true;
    Skill.TargetRule = ESkillTargetRule::EnemyTile;
    TestFalse(TEXT("New area attacks use tile targeting"), CombatRoundRules::UsesUnitTarget(Skill));
    Skill.TargetRule = ESkillTargetRule::AllyUnit;
    TestTrue(TEXT("New support effects use unit targeting"), CombatRoundRules::UsesUnitTarget(Skill));
    Skill.Kind = ECombatRoundSkillKind::Projectile;
    TestFalse(TEXT("Area collision cannot bypass first-hit projectile behavior"), CombatRoundRules::IsValidSkill(Skill));
    Skill.Kind = ECombatRoundSkillKind::Melee;
    Skill.EffectDuration = 0.f;
    TestFalse(TEXT("An active effect must have a bounded positive duration"), CombatRoundRules::IsValidSkill(Skill));
    Skill.EffectDuration = 0.5f;
    Skill.bUseMeleeAreaCollision = true;
    TestFalse(TEXT("Two simultaneous melee hit sources are rejected"), CombatRoundRules::IsValidSkill(Skill));
    return true;
}

#endif
