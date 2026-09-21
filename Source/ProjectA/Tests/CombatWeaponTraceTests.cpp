#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Combat/Library/CombatWeaponTraceLibrary.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace CombatWeaponTraceTests
{
    // Exercise real query shapes without loading authored maps or starting gameplay.
    // 제작 맵 로드나 게임 시작 없이 실제 쿼리 충돌체를 검증합니다.
    struct FFixture
    {
        TStrongObjectPtr<UWorld> World;
        TArray<FCombatRoundUnitView> Units;

        FFixture()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
        }

        ~FFixture()
        {
            if (World.IsValid()) World->DestroyWorld(false);
        }

        AUnitBase* AddUnit(int32 UnitId, FVector Location, bool bEnemy)
        {
            if (!World.IsValid()) return nullptr;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnitBase* Unit = World->SpawnActor<AUnitBase>(Location, FRotator::ZeroRotator, Params);
            if (!Unit) return nullptr;
            Unit->SetTeam(bEnemy ? ETeam::Enemy : ETeam::Player);
            Unit->GetAttributeSet()->InitMaxHP(100.f);
            Unit->GetAttributeSet()->InitHP(100.f);
            Unit->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
            Capsule->SetCapsuleSize(10.f, 25.f);
            Capsule->SetCollisionObjectType(ECC_Pawn);
            Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Capsule->SetCollisionResponseToAllChannels(ECR_Block);
            FCombatRoundUnitView& View = Units.AddDefaulted_GetRef();
            View.UnitId = UnitId;
            View.Unit = Unit;
            View.bEnemy = bEnemy;
            View.HP = 100.f;
            return Unit;
        }

        UBoxComponent* AddWall()
        {
            AActor* Actor = World.IsValid() ? World->SpawnActor<AActor>() : nullptr;
            if (!Actor) return nullptr;
            UBoxComponent* Wall = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Wall);
            Wall->SetBoxExtent(FVector(5.f, 200.f, 100.f));
            Wall->SetCollisionObjectType(ECC_WorldStatic);
            Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Wall->SetCollisionResponseToAllChannels(ECR_Block);
            Wall->RegisterComponent();
            Actor->SetActorLocation(FVector(70.f, 0.f, 100.f));
            return Wall;
        }
    };

    CombatWeaponTrace::FBladePose Blade(float X)
    {
        return {FVector(X, -100.f, 100.f), FVector(X, 100.f, 100.f)};
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatWeaponTraceContactsTest, "ProjectA.Combat.WeaponTrace.PhysicalContacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWeaponTraceContactsTest::RunTest(const FString& Parameters)
{
    using namespace CombatWeaponTraceTests;
    const TCHAR* Labels[] = {TEXT("The blade middle hits without endpoint contact"), TEXT("A target inside forward reach misses when the blade does not touch"), TEXT("A target crossed between poses is hit"), TEXT("Collision-disabled capsules are excluded"), TEXT("Friendly units are excluded"), TEXT("A blocking wall stops contact behind it"), TEXT("Dead roster entries are excluded"), TEXT("Units outside this combat roster are excluded")};
    for (int32 Case = 0; Case < UE_ARRAY_COUNT(Labels); ++Case)
    {
        FFixture Fixture;
        AUnitBase* Source = Fixture.AddUnit(0, FVector(0.f, 0.f, 100.f), false);
        AUnitBase* Target = Fixture.AddUnit(1, FVector(100.f, 0.f, 100.f), Case != 4);
        if (!TestNotNull(TEXT("The source exists"), Source) || !TestNotNull(TEXT("The target exists"), Target)) return false;
        CombatWeaponTrace::FBladePose Previous = Blade(100.f);
        CombatWeaponTrace::FBladePose Current = Previous;
        if (Case == 1) Previous = Current = {FVector(0.f, 80.f, 100.f), FVector(160.f, 80.f, 100.f)};
        if (Case == 2 || Case == 5)
        {
            Previous = Blade(20.f);
            Current = Blade(150.f);
        }
        if (Case == 3) Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (Case == 5 && !TestNotNull(TEXT("The wall exists"), Fixture.AddWall())) return false;
        if (Case == 6)
        {
            Fixture.Units[1].HP = 0.f;
            Target->GetAttributeSet()->SetHP(0.f);
            Target->Die();
            Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        if (Case == 7) Fixture.Units.RemoveAt(1);
        AUnitBase* Hit = CombatWeaponTrace::FindFirstHit(Fixture.World.Get(), Source, Fixture.Units, Previous, Current, 4.f);
        TestTrue(Labels[Case], Hit == (Case == 0 || Case == 2 ? Target : nullptr));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatWeaponTraceOrderingTest, "ProjectA.Combat.WeaponTrace.FirstContactAndTieOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWeaponTraceOrderingTest::RunTest(const FString& Parameters)
{
    using namespace CombatWeaponTraceTests;
    FFixture Fixture;
    AUnitBase* Source = Fixture.AddUnit(0, FVector(0.f, 0.f, 100.f), false);
    AUnitBase* HigherId = Fixture.AddUnit(9, FVector(100.f, 0.f, 100.f), true);
    AUnitBase* LowerId = Fixture.AddUnit(2, FVector(100.f, 0.f, 100.f), true);
    if (!TestNotNull(TEXT("The source exists"), Source) || !TestNotNull(TEXT("Both overlapping enemies exist"), HigherId) || !TestNotNull(TEXT("The lower-ID enemy exists"), LowerId)) return false;
    const CombatWeaponTrace::FBladePose Previous = Blade(20.f);
    const CombatWeaponTrace::FBladePose Current = Blade(150.f);
    TestTrue(TEXT("Equal-time contact returns only the lower-ID enemy"), CombatWeaponTrace::FindFirstHit(Fixture.World.Get(), Source, Fixture.Units, Previous, Current, 4.f) == LowerId);
    Fixture.Units.Swap(1, 2);
    TestTrue(TEXT("Roster ordering does not change equal-time selection"), CombatWeaponTrace::FindFirstHit(Fixture.World.Get(), Source, Fixture.Units, Previous, Current, 4.f) == LowerId);
    HigherId->SetActorLocation(FVector(60.f, 0.f, 100.f));
    TestTrue(TEXT("Earlier contact wins before the lower-ID target"), CombatWeaponTrace::FindFirstHit(Fixture.World.Get(), Source, Fixture.Units, Previous, Current, 4.f) == HigherId);
    CombatWeaponTrace::FBladePose Invalid = Previous;
    Invalid.Base.X = std::numeric_limits<double>::quiet_NaN();
    TestNull(TEXT("Nonfinite blade coordinates are rejected"), CombatWeaponTrace::FindFirstHit(Fixture.World.Get(), Source, Fixture.Units, Invalid, Current, 4.f));
    TestNull(TEXT("Nonfinite trace radii are rejected"), CombatWeaponTrace::FindFirstHit(Fixture.World.Get(), Source, Fixture.Units, Previous, Current, std::numeric_limits<float>::quiet_NaN()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatWeaponTraceDefinitionTest, "ProjectA.Combat.WeaponTrace.DefinitionValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWeaponTraceDefinitionTest::RunTest(const FString& Parameters)
{
    FCombatRoundSkill Skill;
    Skill.SkillId = TEXT("WeaponTraceFixture");
    Skill.bUseWeaponTrace = true;
    Skill.WeaponComponentName = TEXT("Sword");
    Skill.WeaponBaseSocket = TEXT("BladeBase");
    Skill.WeaponTipSocket = TEXT("BladeTip");
    TestFalse(TEXT("Weapon tracing requires a montage"), CombatRoundRules::IsValidSkill(Skill));
    TStrongObjectPtr<UAnimMontage> Montage(NewObject<UAnimMontage>());
    Skill.CastMontage = Montage.Get();
    TestTrue(TEXT("A complete weapon trace definition passes structural validation"), CombatRoundRules::IsValidSkill(Skill));
    Skill.WeaponTipSocket = NAME_None;
    TestFalse(TEXT("The blade tip socket is required"), CombatRoundRules::IsValidSkill(Skill));
    Skill.WeaponTipSocket = Skill.WeaponBaseSocket;
    TestFalse(TEXT("Distinct blade endpoints are required"), CombatRoundRules::IsValidSkill(Skill));
    Skill.WeaponTipSocket = TEXT("BladeTip");
    Skill.WeaponTraceDuration = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite swing windows are rejected"), CombatRoundRules::IsValidSkill(Skill));
    return true;
}

#endif
