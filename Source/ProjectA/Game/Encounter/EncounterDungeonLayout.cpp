#include "Game/Encounter/EncounterDungeonLayout.h"

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction)
{
    return GetPath(Direction, 0);
}

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction, int32 LayoutVariant)
{
    if (LayoutVariant < 0 || LayoutVariant >= VariantCount) return {};
    const int32 Approach = 2 + (LayoutVariant & 1);
    const int32 SideOffset = 4 + ((LayoutVariant >> 1) & 1);
    const int32 SideEnd = Approach + 4 + ((LayoutVariant >> 2) & 1);
    const int32 StraightBend = Approach + 3;
    TArray<FIntPoint> Corners;
    switch (Direction)
    {
    case 0:
        Corners = {{0, 0}, {Approach, 0}, {Approach, -SideOffset}, {SideEnd, -SideOffset}};
        break;
    case 1:
        Corners = {{0, 0}, {Approach, 0}, {StraightBend, 0}, {StraightBend, -1}, {SideEnd + 4, -1}, {SideEnd + 4, 0}, {SideEnd + 6, 0}};
        break;
    case 2:
        Corners = {{0, 0}, {Approach, 0}, {Approach, SideOffset}, {SideEnd, SideOffset}};
        break;
    default:
        return {};
    }

    TArray<FVector> Points;
    for (const FIntPoint& Corner : Corners)
    {
        Points.Add(FVector(Corner.X * CellSize, Corner.Y * CellSize, EyeHeight));
    }
    return Points;
}

TSet<FIntPoint> EncounterDungeonLayout::GetFloorCells()
{
    return GetFloorCells(0);
}

TSet<FIntPoint> EncounterDungeonLayout::GetFloorCells(int32 LayoutVariant)
{
    if (LayoutVariant < 0 || LayoutVariant >= VariantCount) return {};
    TSet<FIntPoint> Cells;
    Cells.Add(FIntPoint(-1, 0));
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
        for (int32 Index = 1; Index < Path.Num(); ++Index)
        {
            FIntPoint Cell(FMath::RoundToInt(Path[Index - 1].X / CellSize), FMath::RoundToInt(Path[Index - 1].Y / CellSize));
            const FIntPoint End(FMath::RoundToInt(Path[Index].X / CellSize), FMath::RoundToInt(Path[Index].Y / CellSize));
            const FIntPoint Step(FMath::Sign(End.X - Cell.X), FMath::Sign(End.Y - Cell.Y));
            Cells.Add(Cell);
            while (Cell != End)
            {
                Cell += Step;
                Cells.Add(Cell);
            }
        }
    }

    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        const FVector Arrival = GetPath(Direction, LayoutVariant).Last();
        const FIntPoint Room(FMath::RoundToInt(Arrival.X / CellSize), FMath::RoundToInt(Arrival.Y / CellSize));
        for (int32 X = Room.X; X <= Room.X + 3; ++X)
        {
            for (int32 Y = Room.Y - 1; Y <= Room.Y + 1; ++Y)
            {
                Cells.Add(FIntPoint(X, Y));
            }
        }
    }
    return Cells;
}

FTransform EncounterDungeonLayout::GetStageTransform(int32 Direction)
{
    return GetStageTransform(Direction, 0);
}

FTransform EncounterDungeonLayout::GetStageTransform(int32 Direction, int32 LayoutVariant)
{
    const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
    if (Path.IsEmpty()) return FTransform::Identity;
    // Keep the fixed NPC camera offset aligned with every variant's corridor endpoint.
    // 고정 NPC 카메라 오프셋을 모든 변형의 통로 도착점과 일치시킵니다.
    return FTransform(Path.Last() + FVector(2.f * CellSize, 0.f, -EyeHeight));
}

FText EncounterDungeonLayout::GetDirectionLabel(int32 Index)
{
    switch (Index)
    {
    case 0:
        return NSLOCTEXT("EncounterDungeon", "Left", "좌회전");
    case 1:
        return NSLOCTEXT("EncounterDungeon", "Straight", "직진");
    case 2:
        return NSLOCTEXT("EncounterDungeon", "Right", "우회전");
    default:
        return FText::GetEmpty();
    }
}
