#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/CombatShoulderCamera.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderCameraYawTest, "ProjectA.Combat.ShoulderCamera.YawContinuityAndReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderCameraYawTest::RunTest(const FString& Parameters)
{
    using namespace CombatShoulderCameraGeometry;
    const float Wrapped = UpdateYaw(179.f, -179.f, 1.f / 60.f, 12.f, 150.f, false);
    TestTrue(TEXT("Crossing the yaw seam takes the short positive path."), FMath::FindDeltaAngleDegrees(179.f, Wrapped) > 0.f && FMath::FindDeltaAngleDegrees(179.f, Wrapped) < 2.f);
    const float ReverseWrapped = UpdateYaw(-179.f, 179.f, 1.f / 60.f, 12.f, 150.f, false);
    TestTrue(TEXT("Crossing the same seam in reverse preserves the negative direction."), FMath::FindDeltaAngleDegrees(-179.f, ReverseWrapped) < 0.f && FMath::FindDeltaAngleDegrees(-179.f, ReverseWrapped) > -2.f);
    const float BeforeReturn = UpdateYaw(35.f, 90.f, 1.f / 60.f, 12.f, 150.f, false);
    float ReturningYaw = BeforeReturn;
    for (int32 Frame = 0; Frame < 120; ++Frame) ReturningYaw = UpdateYaw(ReturningYaw, -90.f, 1.f / 60.f, 12.f, 150.f, true);
    TestEqual(TEXT("The return-facing reversal never orbits the camera, even while initial alignment was incomplete."), ReturningYaw, BeforeReturn);
    const float Resumed = UpdateYaw(ReturningYaw, 90.f, 1.f / 60.f, 12.f, 150.f, false);
    TestTrue(TEXT("Leaving return resumes bounded tracking without a yaw snap."), Resumed > ReturningYaw && Resumed - ReturningYaw <= 150.f / 60.f + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("A long frame cannot turn the camera through half the arena."), FMath::Abs(UpdateYaw(0.f, 180.f, 3.f, 12.f, 150.f, false)) <= 15.f + UE_KINDA_SMALL_NUMBER);
    TestEqual(TEXT("A paused camera cannot drift or rotate."), UpdateYaw(35.f, -90.f, 0.f, 12.f, 150.f, false), 35.f);
    float Approach = 0.f;
    for (int32 Frame = 0; Frame < 180; ++Frame) Approach = UpdateYaw(Approach, 90.f, 1.f / 60.f, 12.f, 150.f, false);
    TestTrue(TEXT("Bounded smoothing still converges to the unit heading."), FMath::IsNearlyEqual(Approach, 90.f, 0.01f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatShoulderCameraDecorationTest, "ProjectA.Combat.ShoulderCamera.DecorationClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatShoulderCameraDecorationTest::RunTest(const FString& Parameters)
{
    using namespace CombatShoulderCameraGeometry;
    const FVector Pivot(0.f, 0.f, 150.f);
    const FVector Camera(-250.f, 65.f, 175.f);
    const FBox Wall(FVector(-160.f, -200.f, 0.f), FVector(-150.f, 200.f, 300.f));
    const float Contact = DecorationHitFraction(Wall, Pivot, Camera, 14.f);
    const FVector CameraCenter = FMath::Lerp(Pivot, Camera, Contact);
    TestTrue(TEXT("A nonphysical wall obstructs the shoulder segment before its endpoint."), Contact > 0.f && Contact < 1.f);
    TestTrue(TEXT("The camera center remains a full probe radius in front of the wall."), FMath::IsNearlyEqual(CameraCenter.X, -136.0, 0.001));
    const FBox FarSideProp(FVector(-160.f, 200.f, 0.f), FVector(-150.f, 300.f, 300.f));
    TestEqual(TEXT("A nearby prop outside the camera corridor does not collapse the arm."), DecorationHitFraction(FarSideProp, Pivot, Camera, 14.f), 1.f);
    const FBox Ground(FVector(-1000.f, -1000.f, -20.f), FVector(1000.f, 1000.f, 1.f));
    TestEqual(TEXT("The ground under the unit does not obstruct a chest-height view."), DecorationHitFraction(Ground, Pivot, Camera, 14.f), 1.f);
    const FBox PivotOverlap(FVector(-10.f, -10.f, 140.f), FVector(10.f, 10.f, 160.f));
    TestEqual(TEXT("An overlapping pivot fails closed instead of moving beyond the obstruction."), DecorationHitFraction(PivotOverlap, Pivot, Camera, 14.f), 0.f);
    TestEqual(TEXT("An empty decoration record never blocks the view."), DecorationHitFraction(FBox(ForceInit), Pivot, Camera, 14.f), 1.f);
    return true;
}

#endif
