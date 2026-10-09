#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Game/Run/RunPveDifficulty.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatArenaEnvironmentProfileTest, "ProjectA.Run.PveDifficulty.EnvironmentProfilesAndRebase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatArenaEnvironmentProfileTest::RunTest(const FString& Parameters)
{
    const TArray<FCombatArenaEnvironmentProfile>& Profiles = CombatArenaEnvironment::GetProfiles();
    const TArray<FGameplayTag> Difficulties = {RunPveDifficulty::GetLowTag(), RunPveDifficulty::GetMediumTag(), RunPveDifficulty::GetHighTag()};
    const TArray<FName> ExpectedIds = {TEXT("MeadowBloom"), TEXT("DungeonStone"), TEXT("IceCitadel")};
    if (!TestEqual(TEXT("The native route offers three distinct authored environments."), Profiles.Num(), 3)) return false;
    TestNull(TEXT("Legacy empty IDs retain the original arena instead of acquiring a new profile."), CombatArenaEnvironment::Find(NAME_None));
    TestNull(TEXT("Unknown saved environment IDs cannot select an arbitrary fallback."), CombatArenaEnvironment::Find(TEXT("UnknownArena")));
    TSet<FName> SeenIds;
    for (int32 Index = 0; Index < Profiles.Num(); ++Index)
    {
        const FCombatArenaEnvironmentProfile& Profile = Profiles[Index];
        TestEqual(TEXT("Environment IDs match the existing placed region tags."), Profile.ArenaId, ExpectedIds[Index]);
        TestEqual(TEXT("The source tag resolves exactly the authored region."), Profile.SourceRegionTag, Profile.ArenaId);
        TestFalse(TEXT("Two difficulty profiles never share one environment."), SeenIds.Contains(Profile.ArenaId));
        SeenIds.Add(Profile.ArenaId);
        TestTrue(TEXT("Names and descriptions are available without loading assets."), !Profile.DisplayName.IsEmpty() && !Profile.Description.IsEmpty());
        for (int32 Other = 0; Other < Difficulties.Num(); ++Other)
        {
            FGameplayTagContainer Tags;
            Tags.AddTag(Difficulties[Other]);
            TestEqual(TEXT("Each authored query matches exactly one displayed difficulty."), Profile.DifficultyQuery.Matches(Tags), Other == Index);
        }
        const FTransform Original(FRotator(0.f, 37.f, 0.f), Profile.SourceOffset + FVector(-300.f, 400.f, 1.f), FVector(2.f, 3.f, 0.5f));
        const FTransform Rebased = CombatArenaEnvironment::RebaseTransform(Original, Profile);
        TestTrue(TEXT("Rebasing changes only the region offset and retains the ground height."), Rebased.GetLocation().Equals(FVector(-300.f, 400.f, 1.f)));
        TestTrue(TEXT("Original mesh rotation and scale survive rebasing."), Rebased.GetRotation().Equals(Original.GetRotation()) && Rebased.GetScale3D().Equals(Original.GetScale3D()));
        TestTrue(TEXT("The authored source transform remains unchanged."), Original.GetLocation().Equals(Profile.SourceOffset + FVector(-300.f, 400.f, 1.f)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatArenaEnvironmentClearanceTest, "ProjectA.Run.PveDifficulty.EnvironmentGroundAndCameraClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatArenaEnvironmentClearanceTest::RunTest(const FString& Parameters)
{
    const FBox Clearance(FVector(-790.f, -190.f, 3.f), FVector(190.f, 990.f, 600.f));
    const FVector Camera(-300.f, -1000.f, 1500.f);
    const TArray<FVector> Samples = {FVector(-300.f, 400.f, 150.f)};
    const FBox GroundPlane(FVector(-5300.f, -4100.f, 1.f), FVector(4700.f, 4900.f, 1.f));
    const FBox StoneFloor(FVector(-1500.f, -600.f, -17.6f), FVector(-1100.f, -200.f, 1.f));
    TestTrue(TEXT("The original meadow and ice planes are retained at Z=1."), CombatArenaEnvironment::ShouldKeepBounds(GroundPlane, Clearance, Camera, Samples));
    TestTrue(TEXT("The original stone floor top at Z=1 is retained without lifting its volume."), CombatArenaEnvironment::ShouldKeepBounds(StoneFloor, Clearance, Camera, Samples));
    TestTrue(TEXT("Tiny floating point error on a planar ground does not remove its surface."), CombatArenaEnvironment::IsGroundBounds(FBox(FVector(-1.f, -1.f, 1.00001f), FVector(1.f, 1.f, 1.00001f))));
    TestFalse(TEXT("A tall prop inside the formation is rejected."), CombatArenaEnvironment::ShouldKeepBounds(FBox(FVector(-400.f, 300.f, 0.f), FVector(-200.f, 500.f, 400.f)), Clearance, Camera, Samples));
    TestFalse(TEXT("A prop outside the grid that blocks the camera is rejected."), CombatArenaEnvironment::ShouldKeepBounds(FBox(FVector(-450.f, -750.f, 900.f), FVector(-150.f, -450.f, 1300.f)), Clearance, Camera, Samples));
    TestFalse(TEXT("A ceiling above the units cannot cover the elevated camera view."), CombatArenaEnvironment::ShouldKeepBounds(FBox(FVector(-2000.f, -2000.f, 900.f), FVector(2000.f, 2000.f, 1000.f)), Clearance, Camera, Samples));
    TestTrue(TEXT("A visible backdrop outside the combat volume and camera rays remains."), CombatArenaEnvironment::ShouldKeepBounds(FBox(FVector(1000.f, 1600.f, 0.f), FVector(1200.f, 1800.f, 400.f)), Clearance, Camera, Samples));
    TestFalse(TEXT("Missing bounds cannot be treated as valid decoration."), CombatArenaEnvironment::ShouldKeepBounds(FBox(ForceInit), Clearance, Camera, Samples));
    TestFalse(TEXT("Without camera samples the filter fails closed."), CombatArenaEnvironment::ShouldKeepBounds(GroundPlane, Clearance, Camera, {}));
    TestFalse(TEXT("Non-finite camera coordinates cannot bypass the filter."), CombatArenaEnvironment::ShouldKeepBounds(GroundPlane, Clearance, FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0), Samples));
    return true;
}

#endif
