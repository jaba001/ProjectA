#include "Game/Encounter/EncounterDungeonLayout.h"

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction)
{
    return GetPath(Direction, 0);
}

TArray<FVector> EncounterDungeonLayout::GetPath(int32 Direction, int32 LayoutVariant)
{
    if (LayoutVariant < 0 || LayoutVariant >= VariantCount) return {};
    if (Direction < 0 || Direction >= 3) return {};
    const FVector Door = GetChoiceDoorCenter(Direction) + FVector(0.f, 0.f, EyeHeight);
    const FVector DoorForward = GetChoiceDoorRotation(Direction).GetForwardVector();
    const FVector Approach = Door - DoorForward * EntranceDoorApproachDistance;
    const FVector Beyond = Door + DoorForward * ChoiceDoorPassDistance;
    if (Direction == 1) return {FVector(0.f, 0.f, EyeHeight), Approach, Beyond, FVector(ChoiceDoorDistance + (3 + (LayoutVariant & 1) + ((LayoutVariant >> 2) & 1)) * CellSize, 0.f, EyeHeight)};
    // Only doorway crossing is diagonal; each side corridor resumes a rectangular ninety-degree route.
    // 문 통과 구간만 대각선이며 좌우 통로는 직사각형의 90도 경로로 이어집니다.
    const FVector Advance = Beyond + FVector(CellSize * (0.5f + 0.5f * (LayoutVariant & 1)), 0.f, 0.f);
    const FVector Corner(Advance.X, (Direction - 1) * (4 + ((LayoutVariant >> 1) & 1)) * CellSize, EyeHeight);
    return {FVector(0.f, 0.f, EyeHeight), Approach, Beyond, Advance, Corner, Corner + FVector((1 + ((LayoutVariant >> 2) & 1)) * CellSize, 0.f, 0.f)};
}

FQuat EncounterDungeonLayout::GetBranchRotation(int32 Direction)
{
    if (Direction < 0 || Direction >= 3) return FQuat::Identity;
    // Room and corridor axes stay independent of the angled doorway facades.
    // 방과 통로의 축은 기울어진 입구 문 방향과 독립적으로 유지합니다.
    return FQuat::Identity;
}

FVector EncounterDungeonLayout::GetChoiceDoorCenter(int32 Direction)
{
    if (Direction < 0 || Direction >= 3) return FVector::ZeroVector;
    return FVector(ChoiceDoorDistance, (Direction - 1) * ChoiceDoorSpacing, 0.f);
}

FQuat EncounterDungeonLayout::GetChoiceDoorRotation(int32 Direction)
{
    if (Direction < 0 || Direction >= 3) return FQuat::Identity;
    return FRotator(0.f, (Direction - 1) * 45.f, 0.f).Quaternion();
}

TArray<FEncounterDungeonFloorRegion> EncounterDungeonLayout::GetFloorRegions(int32 LayoutVariant)
{
    if (LayoutVariant < 0 || LayoutVariant >= VariantCount) return {};
    TArray<FEncounterDungeonFloorRegion> Regions;
    const float LobbyFront = GetPath(0, LayoutVariant)[3].X + CellSize * 0.5f;
    const float LobbyBack = -1.5f * CellSize;
    Regions.Add({FTransform(FVector((LobbyFront + LobbyBack) * 0.5f, 0.f, 0.f)), FVector2D((LobbyFront - LobbyBack) * 0.5f, 900.f)});
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        const TArray<FVector> Path = GetPath(Direction, LayoutVariant);
        for (int32 Index = Direction == 1 ? 3 : 4; Index < Path.Num(); ++Index)
        {
            const FVector Delta = Path[Index] - Path[Index - 1];
            const FVector Middle = (Path[Index] + Path[Index - 1]) * 0.5f;
            Regions.Add({FTransform(FVector(Middle.X, Middle.Y, 0.f)), FVector2D((FMath::Abs(Delta.X) + CellSize) * 0.5f, (FMath::Abs(Delta.Y) + CellSize) * 0.5f)});
        }
        const FVector Arrival(Path.Last().X, Path.Last().Y, 0.f);
        Regions.Add({FTransform(Arrival + FVector(1.5f * CellSize, 0.f, 0.f)), FVector2D(2.f * CellSize, 1.5f * CellSize)});
        Regions.Add({FTransform(Arrival + FVector(4.f * CellSize, 0.f, 0.f)), FVector2D(CellSize * 0.5f, CellSize * 0.5f), true});
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
    const FVector Normal = Region.Transform.TransformVectorNoScale(FVector(LocalNormal.X, LocalNormal.Y, 0.f));
    const FVector Edge = Region.Transform.TransformPosition(FVector(LocalNormal.X * (Region.HalfExtent.X + 0.5f), LocalNormal.Y * (Region.HalfExtent.Y + 0.5f), 0.f));
    const double HalfLength = FMath::Abs(LocalNormal.X) > 0.f ? Region.HalfExtent.Y : Region.HalfExtent.X;
    TArray<FVector2D> Spans = {FVector2D(-HalfLength, HalfLength)};
    // Exclude internal walls where the lobby, corridors and rooms connect.
    // 진입 공간·통로·방이 연결되는 곳의 내부 벽을 제외합니다.
    for (int32 OtherIndex = 0; OtherIndex < Regions.Num(); ++OtherIndex)
    {
        if (OtherIndex == RegionIndex) continue;
        const FEncounterDungeonFloorRegion& Other = Regions[OtherIndex];
        FVector Origin = Other.Transform.InverseTransformPosition(Edge);
        const FVector Delta = Other.Transform.InverseTransformVectorNoScale(Tangent);
        const FVector BoundaryOrigin = Other.Transform.InverseTransformPosition(Edge - Normal * 0.5f);
        const FVector OtherNormal = Other.Transform.InverseTransformVectorNoScale(Normal);
        // Keep one copy of coincident outer walls instead of treating both outside faces as internal seams.
        // 일치하는 바깥 벽을 모두 내부 연결부로 취급하지 않고 한 사본만 유지합니다.
        const bool bSameXEdge = FMath::IsNearlyEqual(FMath::Abs(OtherNormal.X), 1.0) && FMath::Abs(BoundaryOrigin.X - OtherNormal.X * Other.HalfExtent.X) < 0.01f;
        const bool bSameYEdge = FMath::IsNearlyEqual(FMath::Abs(OtherNormal.Y), 1.0) && FMath::Abs(BoundaryOrigin.Y - OtherNormal.Y * Other.HalfExtent.Y) < 0.01f;
        if (OtherIndex < RegionIndex && (bSameXEdge || bSameYEdge)) Origin = BoundaryOrigin;
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
        if (!ClipAxis(Origin.X, Delta.X, Other.HalfExtent.X + 0.01f) || !ClipAxis(Origin.Y, Delta.Y, Other.HalfExtent.Y + 0.01f)) continue;
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
        return NSLOCTEXT("EncounterDungeon", "Left", "왼쪽 문");
    case 1:
        return NSLOCTEXT("EncounterDungeon", "Straight", "직진");
    case 2:
        return NSLOCTEXT("EncounterDungeon", "Right", "오른쪽 문");
    default:
        return FText::GetEmpty();
    }
}
