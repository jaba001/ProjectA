#include "Game/Encounter/EncounterDungeonLayout.h"

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction)
{
    return GetPath(Direction, 0);
}

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction, int32 LayoutVariant)
{
    if (LayoutVariant < 0 || LayoutVariant >= VariantCount) return {};
    const int32 Approach = 1 + (LayoutVariant & 1);
    if (Direction < 0 || Direction >= 3) return {};
    // Keep side rooms separated while the middle route remains straight at every saved variant.
    // 옆방의 간격을 확보하고 모든 저장 변형에서 가운데 경로를 일직선으로 유지합니다.
    const int32 BranchCells = Direction == 1 ? 3 + ((LayoutVariant >> 2) & 1) : 5 + ((LayoutVariant >> 1) & 1);
    const FVector Junction(Approach * CellSize, 0.f, EyeHeight);
    return {FVector(0.f, 0.f, EyeHeight), Junction, Junction + GetBranchRotation(Direction).RotateVector(FVector(BranchCells * CellSize, 0.f, 0.f))};
}

FQuat EncounterDungeonLayout::GetBranchRotation(int32 Direction)
{
    if (Direction < 0 || Direction >= 3) return FQuat::Identity;
    return FRotator(0.f, (Direction - 1) * 45.f, 0.f).Quaternion();
}

FVector EncounterDungeonLayout::GetChoiceDoorCenter()
{
    return FVector(CellSize * 0.5f, 0.f, 0.f);
}

TArray<FEncounterDungeonFloorRegion> EncounterDungeonLayout::GetFloorRegions(int32 LayoutVariant)
{
    if (LayoutVariant < 0 || LayoutVariant >= VariantCount) return {};
    TArray<FEncounterDungeonFloorRegion> Regions;
    const float Approach = (1 + (LayoutVariant & 1)) * CellSize;
    Regions.Add({FTransform(FVector((Approach - CellSize) * 0.5f, 0.f, 0.f)), FVector2D((Approach + 2.f * CellSize) * 0.5f, CellSize * 0.5f)});
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
        const float BranchLength = FVector::Distance(Path[1], Path.Last());
        const FTransform Branch(GetBranchRotation(Direction), FVector(Approach, 0.f, 0.f));
        const float CorridorHalfLength = (BranchLength - CellSize * 0.5f) * 0.5f;
        Regions.Add({FTransform(Branch.GetRotation(), Branch.TransformPosition(FVector(CorridorHalfLength, 0.f, 0.f))), FVector2D(CorridorHalfLength, CellSize * 0.5f)});
        Regions.Add({FTransform(Branch.GetRotation(), Branch.TransformPosition(FVector(BranchLength + 1.5f * CellSize, 0.f, 0.f))), FVector2D(2.f * CellSize, 1.5f * CellSize)});
        Regions.Add({FTransform(Branch.GetRotation(), Branch.TransformPosition(FVector(BranchLength + 4.f * CellSize, 0.f, 0.f))), FVector2D(CellSize * 0.5f, CellSize * 0.5f), true});
    }
    return Regions;
}

bool EncounterDungeonLayout::HasFloorAt(const TArray<FEncounterDungeonFloorRegion>& Regions, const FVector& Point)
{
    if (Point.ContainsNaN()) return false;
    for (const FEncounterDungeonFloorRegion& Region : Regions)
    {
        const FVector Local = Region.Transform.InverseTransformPosition(Point);
        if (FMath::Abs(Local.X) <= Region.HalfExtent.X + 0.1f && FMath::Abs(Local.Y) <= Region.HalfExtent.Y + 0.1f) return true;
    }
    return false;
}

TArray<FVector2D> EncounterDungeonLayout::GetExposedWallSpans(const TArray<FEncounterDungeonFloorRegion>& Regions, int32 RegionIndex, const FVector2D& LocalNormal)
{
    if (!Regions.IsValidIndex(RegionIndex) || !FMath::IsNearlyEqual(FMath::Abs(LocalNormal.X) + FMath::Abs(LocalNormal.Y), 1.0) || (!FMath::IsNearlyZero(LocalNormal.X) && !FMath::IsNearlyZero(LocalNormal.Y))) return {};
    const FEncounterDungeonFloorRegion& Region = Regions[RegionIndex];
    if (Region.bOpenForwardEdge && LocalNormal.X > 0.f) return {};
    const FVector LocalTangent(-LocalNormal.Y, LocalNormal.X, 0.f);
    const FVector Tangent = Region.Transform.TransformVectorNoScale(LocalTangent);
    const FVector Edge = Region.Transform.TransformPosition(FVector(LocalNormal.X * (Region.HalfExtent.X + 0.5f), LocalNormal.Y * (Region.HalfExtent.Y + 0.5f), 0.f));
    const double HalfLength = FMath::Abs(LocalNormal.X) > 0.f ? Region.HalfExtent.Y : Region.HalfExtent.X;
    TArray<FVector2D> Spans = {FVector2D(-HalfLength, HalfLength)};
    // Clip edges against the whole footprint so diagonal overlaps never create walls across the walking route.
    // 대각선 구간이 겹쳐도 이동 경로에 벽이 생기지 않도록 전체 바닥 영역으로 경계를 잘라냅니다.
    for (int32 OtherIndex = 0; OtherIndex < Regions.Num(); ++OtherIndex)
    {
        if (OtherIndex == RegionIndex) continue;
        const FEncounterDungeonFloorRegion& Other = Regions[OtherIndex];
        const FVector Origin = Other.Transform.InverseTransformPosition(Edge);
        const FVector Delta = Other.Transform.InverseTransformVectorNoScale(Tangent);
        double Start = -HalfLength;
        double End = HalfLength;
        const auto ClipAxis = [&Start, &End](double Position, double Step, double Extent)
        {
            if (FMath::IsNearlyZero(Step)) return FMath::Abs(Position) <= Extent;
            const double First = (-Extent - Position) / Step;
            const double Second = (Extent - Position) / Step;
            Start = FMath::Max(Start, FMath::Min(First, Second));
            End = FMath::Min(End, FMath::Max(First, Second));
            return Start < End;
        };
        if (!ClipAxis(Origin.X, Delta.X, Other.HalfExtent.X + 0.5f) || !ClipAxis(Origin.Y, Delta.Y, Other.HalfExtent.Y + 0.5f)) continue;
        TArray<FVector2D> Remaining;
        for (const FVector2D& Span : Spans)
        {
            if (End <= Span.X || Start >= Span.Y) Remaining.Add(Span);
            else
            {
                if (Start > Span.X) Remaining.Add(FVector2D(Span.X, Start));
                if (End < Span.Y) Remaining.Add(FVector2D(End, Span.Y));
            }
        }
        Spans = MoveTemp(Remaining);
    }
    return Spans;
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
    const FQuat Rotation = GetBranchRotation(Direction);
    return FTransform(Rotation, Path.Last() + Rotation.RotateVector(FVector(2.f * CellSize, 0.f, 0.f)) - FVector(0.f, 0.f, EyeHeight));
}

FVector EncounterDungeonLayout::GetDoorCenter(int32 Direction, int32 LayoutVariant, bool bExit)
{
    const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
    if (Path.IsEmpty()) return FVector::ZeroVector;
    return Path.Last() + GetBranchRotation(Direction).RotateVector(FVector((bExit ? 3.5f : -0.5f) * CellSize, 0.f, 0.f)) - FVector(0.f, 0.f, EyeHeight);
}

FVector EncounterDungeonLayout::GetLoopDoorCenter(int32 Direction, int32 LayoutVariant)
{
    const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
    if (Path.IsEmpty()) return FVector::ZeroVector;
    return Path.Last() + GetBranchRotation(Direction).RotateVector(FVector(4.5f * CellSize, 0.f, 0.f)) - FVector(0.f, 0.f, EyeHeight);
}

TArray<FVector> EncounterDungeonLayout::GetExitPath(int32 Direction, int32 LayoutVariant)
{
    const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
    if (Path.IsEmpty()) return {};
    const FVector Arrival = Path.Last();
    // Walk straight through the rear doorway and conceal the section swap behind the final closed door.
    // 후문을 지나 직진하고 마지막 닫힌 문으로 구간 교환 지점을 가립니다.
    const FVector RearDoor = GetDoorCenter(Direction, LayoutVariant, true) + FVector(0.f, 0.f, EyeHeight);
    const FVector Forward = GetBranchRotation(Direction).GetForwardVector();
    return {Arrival, RearDoor - Forward * 220.f, GetLoopDoorCenter(Direction, LayoutVariant) - Forward * DoorCoverDistance + FVector(0.f, 0.f, EyeHeight)};
}

FText EncounterDungeonLayout::GetDirectionLabel(int32 Index)
{
    switch (Index)
    {
    case 0:
        return NSLOCTEXT("EncounterDungeon", "Left", "왼쪽 45°");
    case 1:
        return NSLOCTEXT("EncounterDungeon", "Straight", "직진");
    case 2:
        return NSLOCTEXT("EncounterDungeon", "Right", "오른쪽 45°");
    default:
        return FText::GetEmpty();
    }
}
