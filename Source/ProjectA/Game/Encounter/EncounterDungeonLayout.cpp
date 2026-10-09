#include "Game/Encounter/EncounterDungeonLayout.h"

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction)
{
    TArray<FIntPoint> Corners;
    switch (Direction)
    {
    case 0:
        Corners = {{0, 0}, {2, 0}, {2, -4}, {6, -4}};
        break;
    case 1:
        Corners = {{0, 0}, {2, 0}, {5, 0}, {5, -1}, {10, -1}, {10, 0}, {12, 0}};
        break;
    case 2:
        Corners = {{0, 0}, {2, 0}, {2, 4}, {6, 4}};
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
    TSet<FIntPoint> Cells;
    Cells.Add(FIntPoint(-1, 0));
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        const TArray<FVector> Path = GetPath(Direction);
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
        const FIntPoint Room = Direction == 1 ? FIntPoint(12, 0) : FIntPoint(6, Direction == 0 ? -4 : 4);
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
    if (Direction < 0 || Direction > 2)
    {
        return FTransform::Identity;
    }
    const FVector Location = Direction == 1 ? FVector(14.f * CellSize, 0.f, 0.f) : FVector(8.f * CellSize, (Direction == 0 ? -4.f : 4.f) * CellSize, 0.f);
    return FTransform(Location);
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
