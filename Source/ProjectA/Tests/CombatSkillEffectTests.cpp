#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/Round/CombatSkillEffectActor.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

namespace CombatSkillEffectTests
{
    struct FFixture
    {
        TStrongObjectPtr<UWorld> World;
        TArray<FCombatRoundUnitView> Roster;

        FFixture()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
        }

        ~FFixture()
        {
            if (!World.IsValid()) return;
            for (TActorIterator<ACombatSkillEffectActor> It(World.Get()); It; ++It)
            {
                It->OnImpact.Clear();
                It->OnResolved.Clear();
            }
            World->DestroyWorld(false);
        }

        AUnitBase* AddUnit(FVector Location, ETeam Team, bool bRegister = true)
        {
            if (!World.IsValid()) return nullptr;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnitBase* Unit = World->SpawnActor<AUnitBase>(Location, FRotator::ZeroRotator, Params);
            if (!Unit) return nullptr;
            Unit->SetTeam(Team);
            Unit->UnitIndex = Roster.Num();
            Unit->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
            Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Unit->GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.f);
            Unit->GetAttributeSet()->InitHP(100.f);
            if (bRegister)
            {
                FCombatRoundUnitView Entry;
                Entry.Unit = Unit;
                Entry.UnitId = Unit->UnitIndex;
                Roster.Add(Entry);
            }
            return Unit;
        }

        bool AddWall(FVector Location)
        {
            AActor* Wall = World.IsValid() ? World->SpawnActor<AActor>(Location, FRotator::ZeroRotator) : nullptr;
            if (!Wall) return false;
            UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
            Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(10.f, 500.f, 200.f));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent();
            Wall->SetActorLocation(Location);
            return true;
        }

        FCombatRoundSkill Skill() const
        {
            FCombatRoundSkill Result;
            Result.bUseEffectCollision = true;
            Result.bTargetOnly = false;
            Result.EffectOffset = FVector(100.f, 0.f, 0.f);
            Result.EffectHalfExtent = FVector(500.f, 150.f, 100.f);
            Result.EffectDuration = 0.5f;
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectMultiHitTest, "ProjectA.Combat.EffectCollision.MultiHitWallRosterAndLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectMultiHitTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* First = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Second = Fixture.AddUnit(FVector(250.f, 80.f, 100.f), ETeam::Enemy);
    AUnitBase* Blocked = Fixture.AddUnit(FVector(450.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Ally = Fixture.AddUnit(FVector(180.f, -60.f, 100.f), ETeam::Player);
    AUnitBase* Outsider = Fixture.AddUnit(FVector(230.f, -80.f, 100.f), ETeam::Enemy, false);
    if (!Source || !First || !Second || !Blocked || !Ally || !Outsider || !Fixture.AddWall(FVector(350.f, 0.f, 100.f))) return false;
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!TestNotNull(TEXT("Effect actor exists"), Effect)) return false;
    TMap<AUnitBase*, int32> Hits;
    int32 Resolutions = 0;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
    Effect->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
    Effect->InitializeEffect(Source, First, First->GetActorLocation(), Fixture.Skill(), Fixture.Roster);
    Effect->AdvanceEffect(0.1f);
    Effect->AdvanceEffect(0.1f);
    TestEqual(TEXT("First touched enemy is hit once"), Hits.FindRef(First), 1);
    TestEqual(TEXT("Second touched enemy is independently hit once"), Hits.FindRef(Second), 1);
    TestEqual(TEXT("Wall occludes a capsule inside the broad volume"), Hits.FindRef(Blocked), 0);
    TestEqual(TEXT("Enemy-only effect excludes allies"), Hits.FindRef(Ally), 0);
    TestEqual(TEXT("An unregistered encounter unit is excluded"), Hits.FindRef(Outsider), 0);
    Effect->AdvanceEffect(1.f);
    Effect->AdvanceEffect(1.f);
    TestTrue(TEXT("Effect resolves at the authored duration"), Effect->HasResolved());
    TestEqual(TEXT("Completion is broadcast once"), Resolutions, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectOffsetOcclusionTest, "ProjectA.Combat.EffectCollision.OffsetBeamPreservesVisibleContacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectOffsetOcclusionTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Visible = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Blocked = Fixture.AddUnit(FVector(450.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Visible || !Blocked || !Fixture.AddWall(FVector(300.f, 0.f, 100.f))) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.Kind = ECombatRoundSkillKind::Melee;
    Skill.EffectHalfExtent = FVector(400.f, 50.f, 80.f);
    for (float CenterX : {300.f, 400.f})
    {
        Skill.EffectOffset = FVector(CenterX, 0.f, 0.f);
        ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
        if (!Effect) return false;
        TMap<AUnitBase*, int32> Hits;
        int32 Resolutions = 0;
        Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
        Effect->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
        Effect->InitializeEffect(Source, Visible, Visible->GetActorLocation(), Skill, Fixture.Roster);
        TestFalse(TEXT("A center inside or beyond the wall does not cancel an exposed beam"), Effect->HasResolved());
        TestEqual(TEXT("The enemy before the wall receives the initial hit"), Hits.FindRef(Visible), 1);
        TestEqual(TEXT("The enemy beyond the wall remains occluded"), Hits.FindRef(Blocked), 0);
        Effect->AdvanceEffect(0.1f);
        Effect->AdvanceEffect(1.f);
        TestEqual(TEXT("The visible enemy is hit once throughout the effect lifetime"), Hits.FindRef(Visible), 1);
        TestEqual(TEXT("A broad offset volume never bypasses the wall"), Hits.FindRef(Blocked), 0);
        TestEqual(TEXT("The offset beam resolves once at its authored duration"), Resolutions, 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectOffsetTravelTest, "ProjectA.Combat.EffectCollision.OffsetTravelStopsAtEmissionWall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectOffsetTravelTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Visible = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Blocked = Fixture.AddUnit(FVector(450.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Visible || !Blocked || !Fixture.AddWall(FVector(300.f, 0.f, 100.f))) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.Kind = ECombatRoundSkillKind::Melee;
    Skill.EffectHalfExtent = FVector(50.f);
    Skill.EffectTravel = FVector(500.f, 0.f, 0.f);
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Effect) return false;
    TMap<AUnitBase*, int32> Hits;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
    Effect->InitializeEffect(Source, Visible, Visible->GetActorLocation(), Skill, Fixture.Roster);
    TestEqual(TEXT("The distant enemy is outside the initial offset volume"), Hits.FindRef(Visible), 0);
    Source->SetActorLocation(FVector(600.f, 0.f, 100.f));
    Effect->AdvanceEffect(0.5f);
    TestEqual(TEXT("Travel uses the captured origin after the caster moves"), Hits.FindRef(Visible), 1);
    TestEqual(TEXT("The offset volume cannot hit through the emission wall"), Hits.FindRef(Blocked), 0);
    TestTrue(TEXT("The traveling emission origin stops before the wall"), Effect->GetActorLocation().X - Skill.EffectOffset.X < 300.f);
    TestTrue(TEXT("The effect center retains its authored offset from the stopped origin"), Effect->GetActorLocation().X > 300.f);
    TestTrue(TEXT("The moving effect resolves when its emission origin reaches the wall"), Effect->HasResolved());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectGroundOcclusionTest, "ProjectA.Combat.EffectCollision.GroundSupportPreservesDestinationAndWalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectGroundOcclusionTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Visible = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Blocked = Fixture.AddUnit(FVector(450.f, 0.f, 100.f), ETeam::Player);
    if (!Source || !Visible || !Blocked || !Fixture.AddWall(FVector(300.f, 0.f, 100.f))) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.TargetRule = ESkillTargetRule::AllyUnit;
    Skill.bTargetOnly = true;
    Skill.EffectOffset = FVector::ZeroVector;
    Skill.EffectHalfExtent = FVector(75.f);
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Effect) return false;
    TMap<AUnitBase*, int32> Hits;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
    Effect->InitializeEffect(Source, Visible, Visible->GetActorLocation(), Skill, Fixture.Roster);
    TestTrue(TEXT("Ground support remains centered on the chosen ally"), Effect->GetActorLocation().Equals(Visible->GetActorLocation()));
    TestEqual(TEXT("An exposed ally receives support at its destination"), Hits.FindRef(Visible), 1);
    TestEqual(TEXT("Ground support does not move back to the caster origin"), Hits.FindRef(Source), 0);
    Effect->AdvanceEffect(1.f);
    ACombatSkillEffectActor* Occluded = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Occluded) return false;
    Occluded->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
    Occluded->InitializeEffect(Source, Blocked, Blocked->GetActorLocation(), Skill, Fixture.Roster);
    TestTrue(TEXT("A wall still prevents relocating ground support to the far side"), Occluded->HasResolved());
    TestEqual(TEXT("The ally behind the wall receives no support"), Hits.FindRef(Blocked), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectSupportTest, "ProjectA.Combat.EffectCollision.SupportSelfTargetAndLiveTags", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectSupportTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Ally = Fixture.AddUnit(FVector(60.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Enemy = Fixture.AddUnit(FVector(80.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Ally || !Enemy) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.TargetRule = ESkillTargetRule::AllyUnit;
    Skill.bTargetOnly = true;
    Skill.EffectOffset = FVector::ZeroVector;
    TestTrue(TEXT("Ally policy includes the caster"), CombatSkillExecution::IsValidEffectTarget(Source, Source, Skill));
    TestFalse(TEXT("Ally policy excludes enemies"), CombatSkillExecution::IsValidEffectTarget(Source, Enemy, Skill));
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Effect) return false;
    TMap<AUnitBase*, int32> Hits;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
    Effect->InitializeEffect(Source, Source, Source->GetActorLocation(), Skill, Fixture.Roster);
    Effect->AdvanceEffect(1.f);
    TestEqual(TEXT("Self support touches the caster once"), Hits.FindRef(Source), 1);
    TestEqual(TEXT("Single-target support excludes an overlapping ally"), Hits.FindRef(Ally), 0);
    TestEqual(TEXT("Support never touches an overlapping enemy"), Hits.FindRef(Enemy), 0);

    Skill.bTargetOnly = false;
    const FGameplayTag RequiredTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage")));
    Skill.TargetRequiredTags.AddTag(RequiredTag);
    ACombatSkillEffectActor* Gated = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Gated) return false;
    int32 GatedHits = 0;
    Gated->OnImpact.AddLambda([&GatedHits](AUnitBase*, AUnitBase*, float) { ++GatedHits; });
    Gated->InitializeEffect(Source, Ally, Ally->GetActorLocation(), Skill, Fixture.Roster);
    TestEqual(TEXT("Missing target tags prevent an initial hit"), GatedHits, 0);
    Ally->GetAbilitySystemComponent()->AddLooseGameplayTag(RequiredTag);
    Gated->AdvanceEffect(0.1f);
    TestEqual(TEXT("A live tag change admits an overlapping ally"), GatedHits, 1);
    Gated->AdvanceEffect(1.f);
    TestEqual(TEXT("Admitted target still receives only one hit"), GatedHits, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectSweepTest, "ProjectA.Combat.EffectCollision.SweptTravelCasterDeathAndInvalidData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectSweepTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Target = Fixture.AddUnit(FVector(250.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Target) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.EffectOffset = FVector::ZeroVector;
    Skill.EffectHalfExtent = FVector(10.f);
    Skill.EffectTravel = FVector(500.f, 0.f, 0.f);
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Effect) return false;
    int32 Hits = 0;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
    Effect->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
    Effect->AdvanceEffect(0.5f);
    TestEqual(TEXT("A large simulation step still sweeps through the target"), Hits, 1);

    ACombatSkillEffectActor* Cancelled = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Cancelled) return false;
    int32 CancelledHits = 0;
    Cancelled->OnImpact.AddLambda([&CancelledHits](AUnitBase*, AUnitBase*, float) { ++CancelledHits; });
    Cancelled->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
    Source->Die();
    TestFalse(TEXT("The caster has entered the authoritative death state"), Source->IsUnitAlive());
    Cancelled->AdvanceEffect(0.5f);
    TestTrue(TEXT("Caster death terminates an attached skill effect"), Cancelled->HasResolved());
    TestEqual(TEXT("Caster death does not apply remaining effect contacts"), CancelledHits, 0);

    AUnitBase* LivingSource = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    if (!LivingSource) return false;
    Skill.EffectDuration = 0.f;
    ACombatSkillEffectActor* Invalid = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Invalid) return false;
    int32 Resolutions = 0;
    Invalid->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
    Invalid->InitializeEffect(LivingSource, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
    Invalid->AdvanceEffect(1.f);
    TestTrue(TEXT("Invalid duration resolves during initialization"), Invalid->HasResolved());
    TestEqual(TEXT("Early initialization failure signals completion once"), Resolutions, 1);
    return true;
}

#endif
