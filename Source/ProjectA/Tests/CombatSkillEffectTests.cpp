#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/Round/CombatChainEffectActor.h"
#include "Combat/Round/CombatSkillEffectActor.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "Tests/CombatChainTestHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "Sound/SoundWave.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

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

        bool AddWall(FVector Location, UBoxComponent** OutWall = nullptr)
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
            if (OutWall) *OutWall = Box;
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
    Gated->AdvanceEffect(0.f);
    TestEqual(TEXT("An unclocked zero-step recheck still admits a newly eligible overlapping ally"), GatedHits, 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectHitDelayTest, "ProjectA.Combat.EffectCollision.DelayedContactsAndTravel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectHitDelayTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Departed = Fixture.AddUnit(FVector(100.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Arrived = Fixture.AddUnit(FVector(700.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Distant = Fixture.AddUnit(FVector(350.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Departed || !Arrived || !Distant) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.EffectHitDelaySeconds = 0.5f;
    Skill.EffectDuration = 1.f;
    Skill.EffectHalfExtent = FVector(20.f);
    Skill.EffectTravel = FVector(400.f, 0.f, 0.f);
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Effect) return false;
    TMap<AUnitBase*, int32> Hits;
    int32 Resolutions = 0;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase* Target, float) { ++Hits.FindOrAdd(Target); });
    Effect->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
    Effect->InitializeEffect(Source, Departed, Departed->GetActorLocation(), Skill, Fixture.Roster);
    const FVector Origin = Effect->GetActorLocation();
    TestTrue(TEXT("Initialization places the effect without applying delayed contacts"), Origin.Equals(FVector(100.f, 0.f, 100.f)) && Hits.IsEmpty());
    Effect->AdvanceEffect(0.25f);
    TestTrue(TEXT("The lead-in neither moves nor hits nor resolves"), Effect->GetActorLocation().Equals(Origin) && Hits.IsEmpty() && !Effect->HasResolved());
    Departed->SetActorLocation(FVector(100.f, 500.f, 100.f));
    Arrived->SetActorLocation(Origin);
    Effect->AdvanceEffect(0.5f);
    TestTrue(TEXT("A step crossing activation moves for only its active quarter-second"), Effect->GetActorLocation().Equals(FVector(200.f, 0.f, 100.f)));
    TestEqual(TEXT("A target that left during the lead-in receives no retroactive hit"), Hits.FindRef(Departed), 0);
    TestEqual(TEXT("The activation sweep begins with current overlapping targets"), Hits.FindRef(Arrived), 1);
    TestEqual(TEXT("Discarded lead-in time cannot extend the sweep to a distant target"), Hits.FindRef(Distant), 0);
    Effect->AdvanceEffect(0.25f);
    TestEqual(TEXT("Later active travel reaches the distant target"), Hits.FindRef(Distant), 1);
    TestFalse(TEXT("Lead-in time does not shorten the authored active duration"), Effect->HasResolved());
    Effect->AdvanceEffect(0.5f);
    Effect->AdvanceEffect(1.f);
    TestEqual(TEXT("The first eligible target remains limited to one hit"), Hits.FindRef(Arrived), 1);
    TestEqual(TEXT("The later eligible target remains limited to one hit"), Hits.FindRef(Distant), 1);
    TestTrue(TEXT("Travel finishes after delay plus the complete active duration"), Effect->HasResolved() && Effect->GetActorLocation().Equals(FVector(500.f, 0.f, 100.f)));
    TestEqual(TEXT("Delayed completion is broadcast once"), Resolutions, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectPresentationClockTest, "ProjectA.Combat.EffectCollision.PresentationClockBoundsDelayedTravel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectPresentationClockTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Target = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Target) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.EffectHitDelaySeconds = 0.25f;
    Skill.EffectOffset = FVector::ZeroVector;
    Skill.EffectHalfExtent = FVector(10.f);
    Skill.EffectTravel = FVector(400.f, 0.f, 0.f);
    ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Effect) return false;
    int32 Hits = 0;
    Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
    Effect->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster, 10.0);
    const FVector Origin = Effect->GetActorLocation();
    for (int32 Index = 0; Index < 50; ++Index) Effect->AdvanceEffect(0.01f, 10.0);
    TestTrue(TEXT("Creation-frame simulation debt cannot consume the lead-in"), Effect->GetActorLocation().Equals(Origin) && Hits == 0 && !Effect->HasResolved());
    Effect->AdvanceEffect(0.5f, 10.125);
    Effect->AdvanceEffect(0.5f, 10.125);
    TestTrue(TEXT("Repeated steps at the same presentation time remain inside the lead-in"), Effect->GetActorLocation().Equals(Origin) && Hits == 0 && !Effect->HasResolved());
    Effect->AdvanceEffect(0.5f, 10.25);
    TestTrue(TEXT("Reaching the activation boundary does not consume active travel"), Effect->GetActorLocation().Equals(Origin) && Hits == 0);
    Effect->AdvanceEffect(0.5f, 10.5);
    TestTrue(TEXT("Only elapsed presentation time advances the active sweep"), Effect->GetActorLocation().Equals(FVector(200.f, 0.f, 100.f)) && Hits == 1 && !Effect->HasResolved());
    Effect->AdvanceEffect(0.5f, 10.5);
    TestFalse(TEXT("Repeated simulation debt cannot expire an active effect"), Effect->HasResolved());
    Effect->AdvanceEffect(0.5f, 10.75);
    TestTrue(TEXT("The effect resolves once presentation reaches its complete lifetime"), Effect->HasResolved() && Hits == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectDelayedGuardsTest, "ProjectA.Combat.EffectCollision.DelayedLiveTagsWallsAndDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectDelayedGuardsTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Target = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Target) return false;
    FCombatRoundSkill Skill = Fixture.Skill();
    Skill.EffectHitDelaySeconds = 0.5f;
    const FGameplayTag RequiredTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage")));
    Skill.TargetRequiredTags.AddTag(RequiredTag);
    Target->GetAbilitySystemComponent()->AddLooseGameplayTag(RequiredTag);
    ACombatSkillEffectActor* Gated = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Gated) return false;
    int32 Hits = 0;
    Gated->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
    Gated->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
    Target->GetAbilitySystemComponent()->RemoveLooseGameplayTag(RequiredTag);
    Gated->AdvanceEffect(0.5f);
    TestEqual(TEXT("Activation rechecks target tags instead of retaining pre-delay eligibility"), Hits, 0);
    Target->GetAbilitySystemComponent()->AddLooseGameplayTag(RequiredTag);
    Gated->AdvanceEffect(0.1f);
    TestEqual(TEXT("A newly eligible target can be hit during the active window"), Hits, 1);
    Gated->AdvanceEffect(1.f);

    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.EffectOffset = FVector::ZeroVector;
    ACombatSkillEffectActor* Occluded = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Occluded) return false;
    int32 BlockedHits = 0;
    Occluded->OnImpact.AddLambda([&BlockedHits](AUnitBase*, AUnitBase*, float) { ++BlockedHits; });
    Occluded->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
    if (!Fixture.AddWall(FVector(100.f, 0.f, 100.f))) return false;
    Occluded->AdvanceEffect(0.25f);
    TestFalse(TEXT("A wall does not end the visual lead-in early"), Occluded->HasResolved());
    Occluded->AdvanceEffect(0.25f);
    TestTrue(TEXT("A wall introduced during the lead-in blocks ground placement at activation"), Occluded->HasResolved() && BlockedHits == 0);

    ACombatSkillEffectActor* Cancelled = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
    if (!Cancelled) return false;
    int32 CancelledHits = 0;
    Cancelled->OnImpact.AddLambda([&CancelledHits](AUnitBase*, AUnitBase*, float) { ++CancelledHits; });
    Cancelled->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
    Source->Die();
    Cancelled->AdvanceEffect(0.1f);
    TestTrue(TEXT("Caster death during the lead-in cancels all future contacts"), Cancelled->HasResolved() && CancelledHits == 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillEffectInvalidTimingTest, "ProjectA.Combat.EffectCollision.RejectsInvalidDirectTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillEffectInvalidTimingTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Target = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    if (!Source || !Target) return false;
    for (const bool bDelay : {false, true})
    {
        for (float Invalid : {-1.f, 10.5f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            FCombatRoundSkill Skill = Fixture.Skill();
            if (bDelay) Skill.EffectHitDelaySeconds = Invalid;
            else Skill.EffectDuration = Invalid;
            ACombatSkillEffectActor* Effect = Fixture.World->SpawnActor<ACombatSkillEffectActor>();
            if (!Effect) return false;
            int32 Resolutions = 0;
            int32 Hits = 0;
            Effect->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
            Effect->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
            Effect->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster);
            Effect->AdvanceEffect(20.f);
            TestTrue(TEXT("Direct initialization rejects invalid timing without hits or unresolved actors"), Effect->HasResolved() && Resolutions == 1 && Hits == 0);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatChainSelectionGasTest, "ProjectA.Combat.Chain.NearestUnhitTagsOriginalCasterAndLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatChainSelectionGasTest::RunTest(const FString& Parameters)
{
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* First = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* HigherId = Fixture.AddUnit(FVector(350.f, 50.f, 100.f), ETeam::Enemy);
    AUnitBase* LowerId = Fixture.AddUnit(FVector(350.f, -50.f, 100.f), ETeam::Enemy);
    AUnitBase* Untagged = Fixture.AddUnit(FVector(205.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Blocked = Fixture.AddUnit(FVector(210.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Ally = Fixture.AddUnit(FVector(215.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* Fourth = Fixture.AddUnit(FVector(450.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Outsider = Fixture.AddUnit(FVector(201.f, 0.f, 100.f), ETeam::Enemy, false);
    if (!Source || !First || !HigherId || !LowerId || !Untagged || !Blocked || !Ally || !Fourth || !Outsider) return false;
    HigherId->UnitIndex = Fixture.Roster[2].UnitId = 9;
    LowerId->UnitIndex = Fixture.Roster[3].UnitId = 2;
    FCombatRoundSkill Skill = CombatChainTests::Skill();
    const FGameplayTag Required = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"));
    const FGameplayTag QueryTag = ProjectACombatTags::Data_Heal;
    const FGameplayTag Excluded = ProjectACombatTags::Data_Shield;
    Skill.SourceRequiredTags.AddTag(QueryTag);
    Skill.TargetRequiredTags.AddTag(Required);
    Skill.TargetBlockedTags.AddTag(Excluded);
    Skill.TargetTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(QueryTag);
    Source->GetAbilitySystemComponent()->AddLooseGameplayTag(QueryTag);
    for (AUnitBase* Unit : {First, HigherId, LowerId, Untagged, Blocked, Ally, Fourth, Outsider}) Unit->GetAbilitySystemComponent()->AddLooseGameplayTag(Required);
    for (AUnitBase* Unit : {First, HigherId, LowerId, Blocked, Ally, Fourth, Outsider}) Unit->GetAbilitySystemComponent()->AddLooseGameplayTag(QueryTag);
    Blocked->GetAbilitySystemComponent()->AddLooseGameplayTag(Excluded);
    TArray<int32> HitIds;
    bool bOriginalCaster = true;
    bool bAppliedThroughGas = true;
    bool bOriginalGasContext = true;
    int32 AppliedEffects = 0;
    const FDelegateHandle Applied = Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.AddLambda([&](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
    {
        bOriginalGasContext &= Spec.GetContext().GetOriginalInstigator() == Source && Spec.GetContext().GetSourceObject() == Source;
        FGameplayTagContainer Tags;
        Spec.GetAllAssetTags(Tags);
        bOriginalGasContext &= Tags.HasTag(ProjectACombatTags::Skill_Shape_Chain) && Tags.HasTag(ProjectACombatTags::Skill_Effect_Damage);
        ++AppliedEffects;
    });
    CombatSkillExecution::FReleaseContext Context;
    Context.Owner = Source;
    Context.Source = Source;
    Context.Target = First;
    Context.AimLocation = First->GetActorLocation();
    Context.PresentationTime = 0.0;
    ACombatChainEffectActor* Chain = nullptr;
    int32 ImmediateHits = 0;
    const CombatSkillExecution::FReleaseResult Released = CombatSkillExecution::Release(Context, Fixture.Roster, Skill, [&ImmediateHits](AUnitBase*) { ++ImmediateHits; }, [](ACombatRoundProjectile*) {}, [&](ACombatSkillEffectActor* Effect)
    {
        Chain = Cast<ACombatChainEffectActor>(Effect);
        Effect->OnImpact.AddLambda([&](AUnitBase* OriginalSource, AUnitBase* Target, float Power)
        {
            bOriginalCaster &= OriginalSource == Source;
            HitIds.Add(Target->UnitIndex);
            FCombatRoundSkill HitSkill = Skill;
            HitSkill.Power = Power;
            bAppliedThroughGas &= CombatSkillExecution::ApplyEffect(OriginalSource, Target, HitSkill);
        });
    });
    if (!TestTrue(TEXT("The tagged executor creates a managed chain actor"), Released.bSucceeded && Chain)) return false;
    Chain->AdvanceEffect(0.125f, 0.125);
    TestTrue(TEXT("The first impact chooses the lower-ID nearest unhit enemy before the next delay"), Chain->GetChainRuntimeData().TargetUnitId == LowerId->UnitIndex && HitIds.Num() == 1);
    Chain->AdvanceEffect(0.125f, 0.25);
    Chain->AdvanceEffect(0.125f, 0.375);
    Chain->AdvanceEffect(1.f, 1.375);
    TestTrue(TEXT("Equal-distance ordering is independent of roster insertion and targets are never repeated"), HitIds == TArray<int32>({First->UnitIndex, LowerId->UnitIndex, HigherId->UnitIndex}));
    TestTrue(TEXT("The total target limit includes the first enemy and resolves the chain"), Chain->HasResolved() && Chain->GetChainRuntimeData().HitUnitIds.Num() == 3);
    TestTrue(TEXT("All jumps retain the original source actor and actual GAS context and tags"), bOriginalCaster && bAppliedThroughGas && bOriginalGasContext && AppliedEffects == 3);
    TestEqual(TEXT("The first enemy receives the authored power"), First->GetAttributeSet()->GetHP(), 80.f);
    TestEqual(TEXT("The second enemy receives one cumulative multiplier"), LowerId->GetAttributeSet()->GetHP(), 90.f);
    TestEqual(TEXT("The third enemy receives two cumulative multipliers"), HigherId->GetAttributeSet()->GetHP(), 95.f);
    TestTrue(TEXT("Owned tags team roster and maximum-target filtering preserve excluded HP"), Untagged->GetAttributeSet()->GetHP() == 100.f && Blocked->GetAttributeSet()->GetHP() == 100.f && Ally->GetAttributeSet()->GetHP() == 100.f && Outsider->GetAttributeSet()->GetHP() == 100.f && Fourth->GetAttributeSet()->GetHP() == 100.f);
    TestEqual(TEXT("Effect-managed release does not invoke the immediate hit path"), ImmediateHits, 0);
    Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.Remove(Applied);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatChainAuthoredSkillsTest, "ProjectA.Combat.Chain.AuthoredFiveSkillsFourTargetsAndAttenuation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatChainAuthoredSkillsTest::RunTest(const FString& Parameters)
{
    for (const FString& Theme : {FString(TEXT("Bramble")), FString(TEXT("Electric")), FString(TEXT("Energy")), FString(TEXT("Fire")), FString(TEXT("Magic"))})
    {
        const FString Name = TEXT("DA_DrGame_LinkChainVFX_Link_") + Theme;
        const FString Path = TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/___LinkChainVFX/") + Name + TEXT(".") + Name;
        TStrongObjectPtr<USkillDefinitionDataAsset> Asset(LoadObject<USkillDefinitionDataAsset>(nullptr, *Path));
        FCombatRoundSkill Skill;
        FText Error;
        if (!TestTrue(TEXT("The actual saved chain DataAsset resolves: ") + Theme, Asset.IsValid() && Asset->ResolveRoundSkill(Skill, Error))) return false;
        const FCombatRoundSkill Before = Asset->RoundDefinition;
        if (!TestTrue(TEXT("The selected authored values reach runtime without a fixture override"), CombatRoundRules::UsesChain(Skill) && Skill.Chain.MaxTargets == 4 && FMath::IsNearlyEqual(Skill.Chain.JumpDistance, 600.f) && FMath::IsNearlyEqual(Skill.Chain.JumpIntervalSeconds, 0.15f) && FMath::IsNearlyEqual(Skill.Chain.DamageMultiplierPerJump, 0.8f))) return false;
        CombatSkillEffectTests::FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
        TArray<AUnitBase*> Enemies;
        for (float Distance : {300.f, 900.f, 1200.f, 1500.f, 1800.f}) Enemies.Add(Fixture.AddUnit(FVector(Distance, 0.f, 100.f), ETeam::Enemy));
        if (!Source || Enemies.Contains(nullptr)) return false;
        TArray<int32> Hits;
        int32 Resolutions = 0;
        int32 AppliedEffects = 0;
        bool bOriginalGasContext = true;
        bool bApplied = true;
        const FDelegateHandle Applied = Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.AddLambda([&](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
        {
            FGameplayTagContainer Tags;
            Spec.GetAllAssetTags(Tags);
            bOriginalGasContext &= Spec.GetContext().GetOriginalInstigator() == Source && Spec.GetContext().GetSourceObject() == Source && Tags.HasAll(Skill.EffectTags);
            ++AppliedEffects;
        });
        CombatSkillExecution::FReleaseContext Context;
        Context.Owner = Source;
        Context.Source = Source;
        Context.Target = Enemies[0];
        Context.AimLocation = Enemies[0]->GetActorLocation();
        Context.PresentationTime = 0.0;
        ACombatChainEffectActor* Chain = nullptr;
        const CombatSkillExecution::FReleaseResult Released = CombatSkillExecution::Release(Context, Fixture.Roster, Skill, [](AUnitBase*) {}, [](ACombatRoundProjectile*) {}, [&](ACombatSkillEffectActor* Effect)
        {
            Chain = Cast<ACombatChainEffectActor>(Effect);
            Effect->OnResolved.AddLambda([&](ACombatSkillEffectActor*) { ++Resolutions; });
            Effect->OnImpact.AddLambda([&](AUnitBase* OriginalSource, AUnitBase* Target, float Power)
            {
                bOriginalGasContext &= OriginalSource == Source;
                Hits.Add(Target->UnitIndex);
                FCombatRoundSkill Impact = Skill;
                Impact.Power = Power;
                bApplied &= CombatSkillExecution::ApplyEffect(OriginalSource, Target, Impact);
            });
        });
        if (!TestTrue(TEXT("The authored tagged skill selects the real chain executor"), Released.bSucceeded && Chain)) return false;
        double Time = Skill.EffectHitDelaySeconds - 0.01;
        Chain->AdvanceEffect(static_cast<float>(Time), Time);
        TestTrue(TEXT("The original first-impact delay remains in force"), Hits.IsEmpty());
        Time += 0.02;
        Chain->AdvanceEffect(0.02f, Time);
        TestEqual(TEXT("The first original target is hit once after its authored delay"), Hits.Num(), 1);
        for (int32 Index = 1; Index < 4; ++Index)
        {
            Time += 0.14;
            Chain->AdvanceEffect(0.14f, Time);
            TestEqual(TEXT("A subsequent hit cannot precede the selected 0.15-second interval"), Hits.Num(), Index);
            Time += 0.02;
            Chain->AdvanceEffect(0.02f, Time);
            TestEqual(TEXT("The next nearest unhit enemy receives exactly one delayed hit"), Hits.Num(), Index + 1);
        }
        Chain->AdvanceEffect(10.f, Time + 10.0);
        TestTrue(TEXT("The first target counts toward four and the fifth reachable enemy remains untouched"), Hits == TArray<int32>({Enemies[0]->UnitIndex, Enemies[1]->UnitIndex, Enemies[2]->UnitIndex, Enemies[3]->UnitIndex}) && Enemies[4]->GetAttributeSet()->GetHP() == 100.f);
        for (int32 Index = 0; Index < 4; ++Index) TestTrue(TEXT("The actual GAS HP uses cumulative authored attenuation"), FMath::IsNearlyEqual(Enemies[Index]->GetAttributeSet()->GetHP(), 100.f - Skill.Power * FMath::Pow(0.8f, static_cast<float>(Index)), 0.001f));
        TestTrue(TEXT("The exact 600cm jump boundary, original GAS context and single completion survive actual authored content"), bApplied && bOriginalGasContext && AppliedEffects == 4 && Resolutions == 1 && Chain->HasResolved());
        TestTrue(TEXT("Loading and exercising the profile never mutates its authored DataAsset"), FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Before, &Asset->RoundDefinition, 0));
        Source->GetAbilitySystemComponent()->OnGameplayEffectAppliedDelegateToTarget.Remove(Applied);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatChainFirstWindowClockTest, "ProjectA.Combat.Chain.FirstVolumeOcclusionWindowAndPresentationClock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatChainFirstWindowClockTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 2; ++Case)
    {
        CombatSkillEffectTests::FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
        AUnitBase* Target = Fixture.AddUnit(FVector(Case == 0 ? 900.f : 200.f, 0.f, 100.f), ETeam::Enemy);
        ACombatChainEffectActor* Chain = Fixture.World->SpawnActor<ACombatChainEffectActor>();
        if (!Source || !Target || !Chain) return false;
        UBoxComponent* Wall = nullptr;
        if (Case == 1 && !Fixture.AddWall(FVector(100.f, 0.f, 100.f), &Wall)) return false;
        int32 Hits = 0;
        Chain->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
        Chain->InitializeEffect(Source, Target, Target->GetActorLocation(), CombatChainTests::Skill(), Fixture.Roster, 0.0);
        Chain->AdvanceEffect(0.125f, 0.125);
        TestTrue(TEXT("A target outside the first volume or behind cover remains pending within its active window"), Hits == 0 && !Chain->HasResolved());
        if (Case == 0) Target->SetActorLocation(FVector(200.f, 0.f, 100.f));
        else Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Chain->AdvanceEffect(0.125f, 0.25);
        TestTrue(TEXT("The original first capsule query can hit a target that enters or emerges later"), Hits == 1 && Chain->HasResolved());
    }
    CombatSkillEffectTests::FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
    AUnitBase* First = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
    AUnitBase* Second = Fixture.AddUnit(FVector(350.f, 0.f, 100.f), ETeam::Enemy);
    ACombatChainEffectActor* Chain = Fixture.World->SpawnActor<ACombatChainEffectActor>();
    if (!Source || !First || !Second || !Chain) return false;
    int32 Hits = 0;
    Chain->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
    Chain->InitializeEffect(Source, First, First->GetActorLocation(), CombatChainTests::Skill(), Fixture.Roster, 10.0);
    for (int32 Step = 0; Step < 50; ++Step) Chain->AdvanceEffect(0.01f, 10.0);
    TestEqual(TEXT("Creation-frame simulation debt cannot consume the initial lead-in"), Hits, 0);
    Chain->AdvanceEffect(5.f, 10.125);
    TestEqual(TEXT("The first presentation boundary resolves only the first target"), Hits, 1);
    for (int32 Step = 0; Step < 50; ++Step) Chain->AdvanceEffect(0.01f, 10.125);
    TestTrue(TEXT("A new segment cannot spend old simulation debt in the same presentation frame"), Hits == 1 && !Chain->HasResolved());
    Chain->AdvanceEffect(5.f, 10.25);
    TestTrue(TEXT("The next target waits for its own presentation interval and then resolves"), Hits == 2 && Chain->HasResolved());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatChainDelayedGuardsTest, "ProjectA.Combat.Chain.PendingTargetDeathRangeWallTagsAndCasterDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatChainDelayedGuardsTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 5; ++Case)
    {
        CombatSkillEffectTests::FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
        AUnitBase* First = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
        AUnitBase* Pending = Fixture.AddUnit(FVector(400.f, 0.f, 100.f), ETeam::Enemy);
        ACombatChainEffectActor* Chain = Fixture.World->SpawnActor<ACombatChainEffectActor>();
        if (!Source || !First || !Pending || !Chain) return false;
        FCombatRoundSkill Skill = CombatChainTests::Skill();
        const FGameplayTag Required = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"));
        Skill.TargetRequiredTags.AddTag(Required);
        First->GetAbilitySystemComponent()->AddLooseGameplayTag(Required);
        Pending->GetAbilitySystemComponent()->AddLooseGameplayTag(Required);
        int32 Hits = 0;
        int32 Resolutions = 0;
        Chain->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
        Chain->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
        Chain->InitializeEffect(Source, First, First->GetActorLocation(), Skill, Fixture.Roster, 0.0);
        Chain->AdvanceEffect(0.125f, 0.125);
        if (!TestTrue(TEXT("The eligible second enemy is captured as a pending segment"), Hits == 1 && Chain->GetChainRuntimeData().TargetUnitId == Pending->UnitIndex)) return false;
        if (Case == 0) Pending->Die();
        else if (Case == 1) Pending->SetActorLocation(FVector(2000.f, 0.f, 100.f));
        else if (Case == 2 && !Fixture.AddWall(FVector(300.f, 0.f, 100.f))) return false;
        else if (Case == 3) Pending->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Required);
        else if (Case == 4) Source->Die();
        Chain->AdvanceEffect(0.125f, 0.25);
        Chain->AdvanceEffect(5.f, 5.25);
        TestTrue(TEXT("Changed pending eligibility cancels subsequent hits without rerouting or duplicate completion"), Chain->HasResolved() && Hits == 1 && Resolutions == 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatChainDeadPreviousPositionTest, "ProjectA.Combat.Chain.DeadOrDestroyedPreviousTargetRetainsContactPosition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatChainDeadPreviousPositionTest::RunTest(const FString& Parameters)
{
    for (bool bDestroyPrevious : {false, true})
    {
        CombatSkillEffectTests::FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
        AUnitBase* First = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
        AUnitBase* Second = Fixture.AddUnit(FVector(400.f, 0.f, 100.f), ETeam::Enemy);
        ACombatChainEffectActor* Chain = Fixture.World->SpawnActor<ACombatChainEffectActor>();
        if (!Source || !First || !Second || !Chain) return false;
        FCombatRoundSkill Skill = CombatChainTests::Skill();
        Skill.Power = 120.f;
        Skill.Chain.MaxTargets = 2;
        Skill.Chain.JumpDistance = 250.f;
        Skill.Chain.DamageMultiplierPerJump = 1.f;
        int32 Hits = 0;
        bool bOriginalCaster = true;
        bool bApplied = true;
        Chain->OnImpact.AddLambda([&](AUnitBase* OriginalSource, AUnitBase* Target, float Power)
        {
            bOriginalCaster &= OriginalSource == Source;
            FCombatRoundSkill HitSkill = Skill;
            HitSkill.Power = Power;
            bApplied &= CombatSkillExecution::ApplyEffect(OriginalSource, Target, HitSkill);
            ++Hits;
            if (Target == First)
            {
                if (bDestroyPrevious) First->Destroy();
                else First->SetActorLocation(FVector(10000.f, 0.f, 100.f));
            }
        });
        Chain->InitializeEffect(Source, First, First->GetActorLocation(), Skill, Fixture.Roster, 0.0);
        Chain->AdvanceEffect(0.125f, 0.125);
        TestTrue(TEXT("A lethal or destroyed previous enemy retains its pre-damage origin rather than corpse movement"), Hits == 1 && Chain->GetChainRuntimeData().SegmentSourcePosition.Equals(FVector(200.f, 0.f, 100.f)) && Chain->GetChainRuntimeData().TargetUnitId == Second->UnitIndex);
        Chain->AdvanceEffect(0.125f, 0.25);
        TestTrue(TEXT("The next enemy is reached from the saved position with the original caster's GAS damage"), Hits == 2 && bOriginalCaster && bApplied && !Second->IsUnitAlive() && Chain->HasResolved());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatChainAuthoredSoundBudgetTest, "ProjectA.Combat.Chain.AuthoredSoundCleanupBudgetDoesNotExtendHits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatChainAuthoredSoundBudgetTest::RunTest(const FString& Parameters)
{
    for (float SoundLimit : {3.f, 10.f, 60.f})
    {
        CombatSkillEffectTests::FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(FVector(0.f, 0.f, 100.f), ETeam::Player);
        AUnitBase* Target = Fixture.AddUnit(FVector(200.f, 0.f, 100.f), ETeam::Enemy);
        ACombatChainEffectActor* Chain = Fixture.World->SpawnActor<ACombatChainEffectActor>();
        TStrongObjectPtr<USoundWave> Sound(NewObject<USoundWave>(GetTransientPackage(), NAME_None, RF_Transient));
        if (!Source || !Target || !Chain || !Sound.IsValid()) return false;
        FCombatRoundSkill Skill = CombatChainTests::Skill();
        Skill.Vfx.Sound = Sound.Get();
        Skill.Vfx.SoundMaxDuration = SoundLimit;
        int32 Hits = 0;
        int32 Resolutions = 0;
        Chain->OnImpact.AddLambda([&Hits](AUnitBase*, AUnitBase*, float) { ++Hits; });
        Chain->OnResolved.AddLambda([&Resolutions](ACombatSkillEffectActor*) { ++Resolutions; });
        Chain->InitializeEffect(Source, Target, Target->GetActorLocation(), Skill, Fixture.Roster, 0.0);
        Chain->AdvanceEffect(0.125f, 0.125);
        // The audio-disabled native fixture checks the real actor cleanup deadline, without claiming waveform playback or listening.
        // 사운드 비활성 네이티브 fixture로 실제 액터 정리 시한을 확인하며 파형 재생·청취 결과로 기록하지 않습니다.
        TestTrue(TEXT("Authored audio cleanup keeps the base five-second floor and longer finite sound limits after collision resolution"), Chain->HasResolved() && FMath::IsNearlyEqual(Chain->GetLifeSpan(), FMath::Max(5.f, SoundLimit)));
        Chain->AdvanceEffect(60.f, 60.125);
        TestTrue(TEXT("A longer cosmetic sound budget never extends authoritative hits or completion callbacks"), Hits == 1 && Resolutions == 1 && Chain->GetChainRuntimeData().HitUnitIds.Num() == 1);
    }
    return true;
}

#endif
