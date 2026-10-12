#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Encounter/EncounterDungeonLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterDungeonLayoutGeometryTest, "ProjectA.Run.EncounterPresentation.DungeonLayoutGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterDungeonLayoutGeometryTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("The frozen layout policy exposes exactly eight supported variants."), EncounterDungeonLayout::VariantCount, 8);
    for (int32 Variant = 0; Variant < EncounterDungeonLayout::VariantCount; ++Variant)
    {
        const TArray<FEncounterDungeonFloorRegion> Floor = EncounterDungeonLayout::GetFloorRegions(Variant);
        const FVector ArrivalCameraOffset(-800.f, 0.f, EncounterDungeonLayout::EyeHeight);
        TArray<FVector> Destinations;
        TestTrue(TEXT("The dungeon includes floor behind the initial eye-level junction."), EncounterDungeonLayout::HasFloorAt(Floor, FVector(-400.f, 0.f, 0.f)) && EncounterDungeonLayout::HasFloorAt(Floor, FVector::ZeroVector));
        if (!TestTrue(TEXT("The junction is a rectangular foyer with an incoming passage."), Floor.Num() >= 2)) return false;
        TestTrue(TEXT("The foyer front and rear walls match the three choice doors and the single incoming door."), FMath::IsNearlyEqual(Floor[0].Transform.GetLocation().X + Floor[0].HalfExtent.X, static_cast<double>(EncounterDungeonLayout::ChoiceDoorDistance)) && FMath::IsNearlyEqual(Floor[0].Transform.GetLocation().X - Floor[0].HalfExtent.X, static_cast<double>(-EncounterDungeonLayout::CellSize * 0.5f)));
        const TArray<FVector2D> FrontWall = EncounterDungeonLayout::GetExposedWallSpans(Floor, 0, FVector2D(1.f, 0.f));
        const TArray<FVector2D> RearWall = EncounterDungeonLayout::GetExposedWallSpans(Floor, 0, FVector2D(-1.f, 0.f));
        TestEqual(TEXT("The foyer front wall separates exactly three door openings."), FrontWall.Num(), 4);
        TestEqual(TEXT("The foyer rear wall has exactly one centered entrance opening."), RearWall.Num(), 2);
        for (const float Divider : {-EncounterDungeonLayout::ChoiceDoorSpacing * 0.5f, EncounterDungeonLayout::ChoiceDoorSpacing * 0.5f}) TestTrue(TEXT("A stone wall remains between each adjacent pair of choice doorframes."), FrontWall.ContainsByPredicate([Divider](const FVector2D& Span) { return Span.X < Divider && Span.Y > Divider; }));
        for (const FEncounterDungeonFloorRegion& Region : Floor) TestTrue(TEXT("Lobby, corridors and rooms retain axis-aligned walls and floor."), Region.Transform.GetRotation().Equals(FQuat::Identity));
        for (int32 Direction = 0; Direction < 3; ++Direction)
        {
            const TArray<FVector> Path = EncounterDungeonLayout::GetPath(Direction, Variant);
            if (!TestTrue(TEXT("Each direction contains a junction, a forward approach and a branch destination."), Path.Num() >= 3)) return false;
            TestTrue(TEXT("All directions begin at the same eye-level junction."), Path[0].Equals(FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight)));
            const FVector ChoiceDoor = EncounterDungeonLayout::GetChoiceDoorCenter(Direction);
            const FQuat DoorRotation = EncounterDungeonLayout::GetChoiceDoorRotation(Direction);
            TestFalse(TEXT("The front wall leaves the selected doorway open instead of placing a wall through its frame."), FrontWall.ContainsByPredicate([&ChoiceDoor](const FVector2D& Span) { return Span.X < ChoiceDoor.Y && Span.Y > ChoiceDoor.Y; }));
            TestTrue(TEXT("All three choice doors face straight forward on the rectangular layout."), ChoiceDoor.X == EncounterDungeonLayout::ChoiceDoorDistance && DoorRotation.Equals(FQuat::Identity));
            const int32 ApproachIndex = 1;
            TestTrue(TEXT("The camera stops before and passes straight beyond its own doorway."), Path[ApproachIndex].Equals(ChoiceDoor - DoorRotation.GetForwardVector() * EncounterDungeonLayout::EntranceDoorApproachDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight)) && Path[ApproachIndex + 1].Equals(ChoiceDoor + DoorRotation.GetForwardVector() * EncounterDungeonLayout::ChoiceDoorPassDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight)));
            const FVector Forward = EncounterDungeonLayout::GetBranchRotation(Direction).GetForwardVector();
            if (Direction != 1)
            {
                TestTrue(TEXT("The side doorway approach is reached by a single diagonal segment without a lateral detour."), !FMath::IsNearlyZero(Path[1].X - Path[0].X) && !FMath::IsNearlyZero(Path[1].Y - Path[0].Y) && FVector::Distance(Path[0], Path[1]) < FMath::Abs(Path[1].X - Path[0].X) + FMath::Abs(Path[1].Y - Path[0].Y));
                TestTrue(TEXT("Side corridors turn ninety degrees after the straight doorway."), FMath::IsNearlyZero((Path[4] - Path[3]).X) && FMath::IsNearlyEqual(FMath::Abs((Path[4] - Path[3]).GetSafeNormal().Y), 1.0) && FMath::IsNearlyEqual(FVector::DotProduct((Path[4] - Path[3]).GetSafeNormal(), (Path[5] - Path[4]).GetSafeNormal()), 0.0));
            }
            for (int32 Index = 1; Index < Path.Num(); ++Index)
            {
                const FVector Delta = Path[Index] - Path[Index - 1];
                if (!TestTrue(TEXT("Every segment is finite, nonzero and horizontal at eye height."), !Path[Index].ContainsNaN() && Path[Index].Z == EncounterDungeonLayout::EyeHeight && FMath::IsNearlyZero(Delta.Z) && !Delta.IsNearlyZero())) return false;
                if (Index >= 2 || Direction == 1) TestTrue(TEXT("Doorway crossing and corridor motion retain rectangular axes."), FMath::IsNearlyZero(Delta.X) != FMath::IsNearlyZero(Delta.Y));
                if (Direction == 1) TestTrue(TEXT("The straight course never changes its lateral coordinate."), FMath::IsNearlyZero(Path[Index].Y));
                const int32 Samples = FMath::CeilToInt(Delta.Size() / 20.f);
                for (int32 Sample = 0; Sample <= Samples; ++Sample)
                {
                    const FVector Point = FMath::Lerp(Path[Index - 1], Path[Index], static_cast<float>(Sample) / Samples);
                    TestTrue(TEXT("Every arrival sample has floor through the three-door lobby."), EncounterDungeonLayout::HasFloorAt(Floor, Point));
                    for (int32 RegionIndex = 0; RegionIndex < Floor.Num(); ++RegionIndex)
                    {
                        const FEncounterDungeonFloorRegion& Region = Floor[RegionIndex];
                        for (const FVector2D& Normal : {FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f)})
                        {
                            const FVector Tangent = Region.Transform.TransformVectorNoScale(FVector(-Normal.Y, Normal.X, 0.f));
                            const FVector Edge = Region.Transform.TransformPosition(FVector(Normal.X * Region.HalfExtent.X, Normal.Y * Region.HalfExtent.Y, EncounterDungeonLayout::EyeHeight));
                            for (const FVector2D& Span : EncounterDungeonLayout::GetExposedWallSpans(Floor, RegionIndex, Normal)) TestTrue(TEXT("Exposed walls cannot cross the camera route or its walking clearance."), FMath::PointDistToSegment(Point, Edge + Tangent * Span.X, Edge + Tangent * Span.Y) > 80.f);
                        }
                    }
                }
            }
            const FTransform Stage = EncounterDungeonLayout::GetStageTransform(Direction, Variant);
            TestTrue(TEXT("The path ends exactly at the dungeon NPC's configured eye-level camera offset."), Stage.TransformPosition(ArrivalCameraOffset).Equals(Path.Last()));
            const FVector StageLocation = Stage.GetLocation();
            TestTrue(TEXT("Each NPC destination occupies its own room with floor."), EncounterDungeonLayout::HasFloorAt(Floor, StageLocation) && !Destinations.Contains(StageLocation));
            const FVector Entrance = EncounterDungeonLayout::GetDoorCenter(Direction, Variant, false);
            const FVector Exit = EncounterDungeonLayout::GetDoorCenter(Direction, Variant, true);
            TestTrue(TEXT("The entrance lies before arrival and the rear doorway lies beyond the NPC on the branch axis."), FVector::DotProduct(Entrance - Path.Last(), Forward) < 0.f && FVector::DotProduct(Exit - StageLocation, Forward) > 0.f && FVector::CrossProduct(Exit - Entrance, Forward).Size() < 0.01f);
            const TArray<FVector> ExitPath = EncounterDungeonLayout::GetExitPath(Direction, Variant);
            if (!TestTrue(TEXT("Departure starts at the saved arrival pose and crosses the rear doorway at eye height."), ExitPath.Num() >= 2 && ExitPath[0].Equals(Path.Last()) && FVector::DotProduct(ExitPath.Last() - Exit, Forward) > 0.f)) return false;
            const FVector LoopDoor = EncounterDungeonLayout::GetLoopDoorCenter(Direction, Variant);
            TestTrue(TEXT("Departure finishes close to a closed continuation door before any end wall is visible."), ExitPath.Last().Equals(LoopDoor - Forward * EncounterDungeonLayout::DoorCoverDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight)));
            for (int32 Index = 1; Index < ExitPath.Num(); ++Index)
            {
                TestTrue(TEXT("Every departure segment moves straight forward without a sideways detour."), (ExitPath[Index] - ExitPath[Index - 1]).GetSafeNormal().Equals(Forward));
                const int32 Samples = FMath::CeilToInt(FVector::Distance(ExitPath[Index - 1], ExitPath[Index]) / 20.f);
                for (int32 Sample = 0; Sample <= Samples; ++Sample)
                {
                    const FVector Point = FMath::Lerp(ExitPath[Index - 1], ExitPath[Index], static_cast<float>(Sample) / Samples);
                    TestTrue(TEXT("Every sampled departure point has floor and remains at eye height."), Point.Z == EncounterDungeonLayout::EyeHeight && EncounterDungeonLayout::HasFloorAt(Floor, Point));
                }
            }
            Destinations.Add(StageLocation);
        }
        TestEqual(TEXT("The three directions retain three distinct NPC rooms."), Destinations.Num(), 3);
    }
    for (const float AspectRatio : {4.f / 3.f, 16.f / 9.f, 21.f / 9.f})
    {
        const float HalfWidth = FMath::Tan(FMath::DegreesToRadians(EncounterDungeonLayout::ChoiceFieldOfView * 0.5f)) * AspectRatio / (16.f / 9.f);
        for (int32 Direction = 0; Direction < 3; ++Direction)
        {
            const FVector Door = EncounterDungeonLayout::GetChoiceDoorCenter(Direction);
            const FQuat Rotation = EncounterDungeonLayout::GetChoiceDoorRotation(Direction);
            for (const float X : {-30.f, 30.f})
            {
                for (const float Y : {-200.f, 200.f})
                {
                    const FVector Corner = Door + Rotation.RotateVector(FVector(X, Y, 0.f));
                    TestTrue(TEXT("All three doorframes fit the initial view at supported aspect ratios."), FMath::Abs(Corner.Y / Corner.X) < HalfWidth);
                }
            }
        }
    }
    const TArray<FEncounterDungeonFloorRegion> Overlapping = {{FTransform::Identity, FVector2D(400.f, 200.f)}, {FTransform(FVector(200.f, 0.f, 0.f)), FVector2D(400.f, 200.f)}};
    const TArray<FVector2D> FirstTop = EncounterDungeonLayout::GetExposedWallSpans(Overlapping, 0, FVector2D(0.f, 1.f));
    const TArray<FVector2D> SecondTop = EncounterDungeonLayout::GetExposedWallSpans(Overlapping, 1, FVector2D(0.f, 1.f));
    const auto TotalSpan = [](const TArray<FVector2D>& Spans)
    {
        double Total = 0.f;
        for (const FVector2D& Span : Spans) Total += Span.Y - Span.X;
        return Total;
    };
    TestTrue(TEXT("Overlapping aligned regions retain their exterior wall exactly once."), FMath::IsNearlyEqual(TotalSpan(FirstTop) + TotalSpan(SecondTop), 1000.0, 0.1));
    for (const float AspectRatio : {4.f / 3.f, 16.f / 9.f, 21.f / 9.f})
    {
        const float HalfHeight = FMath::Tan(FMath::DegreesToRadians(40.f)) * (EncounterDungeonLayout::DoorCoverDistance + 22.f);
        TestTrue(TEXT("The closed-door seam cover fills the terminal view for supported aspect ratios."), HalfHeight * AspectRatio < 140.f && HalfHeight < FMath::Min(EncounterDungeonLayout::EyeHeight, 300.f - EncounterDungeonLayout::EyeHeight));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterDungeonLayoutDirectionTest, "ProjectA.Run.EncounterPresentation.DungeonDirectionContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterDungeonLayoutDirectionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Server offer index zero is labelled as the left door."), EncounterDungeonLayout::GetDirectionLabel(0), NSLOCTEXT("EncounterDungeon", "Left", "왼쪽 문"));
    TestEqual(TEXT("Server offer index one is labelled straight."), EncounterDungeonLayout::GetDirectionLabel(1), NSLOCTEXT("EncounterDungeon", "Straight", "직진"));
    TestEqual(TEXT("Server offer index two is labelled as the right door."), EncounterDungeonLayout::GetDirectionLabel(2), NSLOCTEXT("EncounterDungeon", "Right", "오른쪽 문"));
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        TestTrue(TEXT("Legacy path calls preserve variant zero exactly."), EncounterDungeonLayout::GetPath(Direction) == EncounterDungeonLayout::GetPath(Direction, 0));
        TestTrue(TEXT("Legacy NPC placement preserves variant zero exactly."), EncounterDungeonLayout::GetStageTransform(Direction).Equals(EncounterDungeonLayout::GetStageTransform(Direction, 0)));
    }
    const TArray<FEncounterDungeonFloorRegion> LegacyFloor = EncounterDungeonLayout::GetFloorRegions();
    const TArray<FEncounterDungeonFloorRegion> VariantZeroFloor = EncounterDungeonLayout::GetFloorRegions(0);
    TestEqual(TEXT("Default floor calls preserve the variant-zero region count."), LegacyFloor.Num(), VariantZeroFloor.Num());
    for (int32 Index = 0; Index < LegacyFloor.Num(); ++Index) TestTrue(TEXT("Default floor calls preserve every variant-zero region."), LegacyFloor[Index].Transform.Equals(VariantZeroFloor[Index].Transform) && LegacyFloor[Index].HalfExtent.Equals(VariantZeroFloor[Index].HalfExtent));
    for (const int32 InvalidVariant : {-1, EncounterDungeonLayout::VariantCount, MAX_int32})
    {
        TestTrue(TEXT("An invalid saved layout cannot create scenery or route points."), EncounterDungeonLayout::GetFloorRegions(InvalidVariant).IsEmpty() && EncounterDungeonLayout::GetPath(0, InvalidVariant).IsEmpty());
        TestTrue(TEXT("An invalid saved layout cannot place a stage."), EncounterDungeonLayout::GetStageTransform(0, InvalidVariant).Equals(FTransform::Identity));
        TestTrue(TEXT("An invalid saved layout cannot create a door departure path."), EncounterDungeonLayout::GetExitPath(0, InvalidVariant).IsEmpty());
    }
    for (const int32 InvalidDirection : {-1, 3, MAX_int32})
    {
        TestTrue(TEXT("An invalid direction cannot create a route or display an offer label."), EncounterDungeonLayout::GetPath(InvalidDirection).IsEmpty() && EncounterDungeonLayout::GetDirectionLabel(InvalidDirection).IsEmpty());
        TestTrue(TEXT("An invalid direction cannot offset an NPC into an unrelated room."), EncounterDungeonLayout::GetStageTransform(InvalidDirection).Equals(FTransform::Identity));
        TestTrue(TEXT("An invalid direction cannot create a door departure path."), EncounterDungeonLayout::GetExitPath(InvalidDirection, 0).IsEmpty());
    }
    return true;
}

#endif
