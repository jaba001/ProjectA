#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Encounter/EncounterDungeonLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterDungeonLayoutGeometryTest, "ProjectA.Run.EncounterPresentation.DungeonLayoutGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterDungeonLayoutGeometryTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("The frozen layout policy exposes exactly eight supported variants."), EncounterDungeonLayout::VariantCount, 8);
    for (int32 Variant = 0; Variant < EncounterDungeonLayout::VariantCount; ++Variant)
    {
        const TSet<FIntPoint> Floor = EncounterDungeonLayout::GetFloorCells(Variant);
        const FVector ArrivalCameraOffset(-800.f, 0.f, EncounterDungeonLayout::EyeHeight);
        TSet<FIntPoint> Destinations;
        TestTrue(TEXT("The dungeon includes floor behind the initial eye-level junction."), Floor.Contains(FIntPoint(-1, 0)) && Floor.Contains(FIntPoint(0, 0)));
        for (int32 Direction = 0; Direction < 3; ++Direction)
        {
            const TArray<FVector> Path = EncounterDungeonLayout::GetPath(Direction, Variant);
            if (!TestTrue(TEXT("Each direction contains a junction, a forward approach and a branch destination."), Path.Num() >= 3)) return false;
            TestTrue(TEXT("All directions begin at the same eye-level junction."), Path[0].Equals(FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight)));
            TestTrue(TEXT("All directions initially look and move forward before choosing a corridor."), Path[1].X > Path[0].X && Path[1].Y == Path[0].Y);
            const FVector Branch = Path[2] - Path[1];
            TestTrue(TEXT("Offer index zero turns left, one continues straight and two turns right."), Direction == 0 ? Branch.X == 0.f && Branch.Y < 0.f : Direction == 1 ? Branch.X > 0.f && Branch.Y == 0.f : Branch.X == 0.f && Branch.Y > 0.f);
            for (int32 Index = 1; Index < Path.Num(); ++Index)
            {
                const FVector Delta = Path[Index] - Path[Index - 1];
                const bool bHorizontal = !FMath::IsNearlyZero(Delta.X) && FMath::IsNearlyZero(Delta.Y);
                const bool bVertical = FMath::IsNearlyZero(Delta.X) && !FMath::IsNearlyZero(Delta.Y);
                if (!TestTrue(TEXT("Every finite segment moves along exactly one horizontal grid axis at eye height."), !Path[Index].ContainsNaN() && Path[Index].Z == EncounterDungeonLayout::EyeHeight && FMath::IsNearlyZero(Delta.Z) && (bHorizontal != bVertical))) return false;
                const FIntPoint Start(FMath::RoundToInt(Path[Index - 1].X / EncounterDungeonLayout::CellSize), FMath::RoundToInt(Path[Index - 1].Y / EncounterDungeonLayout::CellSize));
                const FIntPoint End(FMath::RoundToInt(Path[Index].X / EncounterDungeonLayout::CellSize), FMath::RoundToInt(Path[Index].Y / EncounterDungeonLayout::CellSize));
                TestTrue(TEXT("Segment endpoints lie exactly on floor-cell centers."), Path[Index].Equals(FVector(End.X * EncounterDungeonLayout::CellSize, End.Y * EncounterDungeonLayout::CellSize, EncounterDungeonLayout::EyeHeight)));
                const FIntPoint Step(FMath::Sign(End.X - Start.X), FMath::Sign(End.Y - Start.Y));
                const int32 Distance = FMath::Abs(End.X - Start.X) + FMath::Abs(End.Y - Start.Y);
                FIntPoint Cell = Start;
                for (int32 Offset = 0; Offset <= Distance; ++Offset)
                {
                    TestTrue(TEXT("Every traversed cell has authored floor including all turn corners."), Floor.Contains(Cell));
                    Cell += Step;
                }
            }
            const FTransform Stage = EncounterDungeonLayout::GetStageTransform(Direction, Variant);
            TestTrue(TEXT("The path ends exactly at the dungeon NPC's configured eye-level camera offset."), Stage.TransformPosition(ArrivalCameraOffset).Equals(Path.Last()));
            const FVector StageLocation = Stage.GetLocation();
            const FIntPoint StageCell(FMath::RoundToInt(StageLocation.X / EncounterDungeonLayout::CellSize), FMath::RoundToInt(StageLocation.Y / EncounterDungeonLayout::CellSize));
            TestTrue(TEXT("Each NPC destination occupies its own room with a floor cell."), Floor.Contains(StageCell) && !Destinations.Contains(StageCell));
            Destinations.Add(StageCell);
        }
        TestEqual(TEXT("The three directions retain three distinct NPC rooms."), Destinations.Num(), 3);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterDungeonLayoutDirectionTest, "ProjectA.Run.EncounterPresentation.DungeonDirectionContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterDungeonLayoutDirectionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Server offer index zero is labelled left."), EncounterDungeonLayout::GetDirectionLabel(0).ToString(), FString(TEXT("좌회전")));
    TestEqual(TEXT("Server offer index one is labelled straight."), EncounterDungeonLayout::GetDirectionLabel(1).ToString(), FString(TEXT("직진")));
    TestEqual(TEXT("Server offer index two is labelled right."), EncounterDungeonLayout::GetDirectionLabel(2).ToString(), FString(TEXT("우회전")));
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        TestTrue(TEXT("Legacy path calls preserve variant zero exactly."), EncounterDungeonLayout::GetPath(Direction) == EncounterDungeonLayout::GetPath(Direction, 0));
        TestTrue(TEXT("Legacy NPC placement preserves variant zero exactly."), EncounterDungeonLayout::GetStageTransform(Direction).Equals(EncounterDungeonLayout::GetStageTransform(Direction, 0)));
    }
    const TSet<FIntPoint> LegacyFloor = EncounterDungeonLayout::GetFloorCells();
    const TSet<FIntPoint> VariantZeroFloor = EncounterDungeonLayout::GetFloorCells(0);
    TestEqual(TEXT("Legacy floor calls preserve the variant-zero cell count."), LegacyFloor.Num(), VariantZeroFloor.Num());
    for (const FIntPoint& Cell : LegacyFloor) TestTrue(TEXT("Legacy floor calls preserve every variant-zero cell."), VariantZeroFloor.Contains(Cell));
    for (const int32 InvalidVariant : {-1, EncounterDungeonLayout::VariantCount, MAX_int32})
    {
        TestTrue(TEXT("An invalid saved layout cannot create scenery or route points."), EncounterDungeonLayout::GetFloorCells(InvalidVariant).IsEmpty() && EncounterDungeonLayout::GetPath(0, InvalidVariant).IsEmpty());
        TestTrue(TEXT("An invalid saved layout cannot place a stage."), EncounterDungeonLayout::GetStageTransform(0, InvalidVariant).Equals(FTransform::Identity));
    }
    for (const int32 InvalidDirection : {-1, 3, MAX_int32})
    {
        TestTrue(TEXT("An invalid direction cannot create a route or display an offer label."), EncounterDungeonLayout::GetPath(InvalidDirection).IsEmpty() && EncounterDungeonLayout::GetDirectionLabel(InvalidDirection).IsEmpty());
        TestTrue(TEXT("An invalid direction cannot offset an NPC into an unrelated room."), EncounterDungeonLayout::GetStageTransform(InvalidDirection).Equals(FTransform::Identity));
    }
    return true;
}

#endif
