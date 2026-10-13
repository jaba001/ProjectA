#include "Game/Encounter/EncounterMazeLayout.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AEncounterMazeLayout::AEncounterMazeLayout()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetActorEnableCollision(false);
    MazeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MazeRoot"));
    SetRootComponent(MazeRoot);
    Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
    Ceiling = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ceiling"));
    Mortar = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mortar"));
    WallBlocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallBlocks"));
    Pillars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Pillars"));
    Torches = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Torches"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TorchMesh(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_torch_wall_dungeon.SM_PROP_torch_wall_dungeon"));
    // Default surfaces reference the project stone materials; a baked Blueprint may override them per component.
    // 기본 표면은 프로젝트 석재 재질을 참조하며 구운 Blueprint에서 컴포넌트별로 바꿀 수 있습니다.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallMaterial(TEXT("/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials/M_EncounterStoneWall.M_EncounterStoneWall"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorMaterial(TEXT("/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials/M_EncounterStoneFloor.M_EncounterStoneFloor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CeilingMaterial(TEXT("/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials/M_EncounterStoneCeiling.M_EncounterStoneCeiling"));
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars, Torches};
    for (UInstancedStaticMeshComponent* Shape : Shapes)
    {
        Shape->SetupAttachment(MazeRoot);
        Shape->SetStaticMesh(Shape == Torches ? TorchMesh.Object : Cube.Object);
        Shape->SetMobility(EComponentMobility::Movable);
        Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shape->SetGenerateOverlapEvents(false);
        Shape->SetCanEverAffectNavigation(false);
        Shape->SetCastShadow(Shape != Torches);
        Shape->SetLightingChannels(false, true, false);
    }
    Floor->SetMaterial(0, FloorMaterial.Object);
    Ceiling->SetMaterial(0, CeilingMaterial.Object);
    WallBlocks->SetMaterial(0, WallMaterial.Object);
    Pillars->SetMaterial(0, WallMaterial.Object);
}

FTransform AEncounterMazeLayout::MakeTorchTransform(const FEncounterMazeTorchMount& Mount)
{
    const FVector Location = Mount.Center + Mount.Normal * (EncounterDungeonLayout::CellSize * 0.5f - 15.f) + FVector(0.f, 0.f, 220.f);
    return FTransform(FRotationMatrix::MakeFromYZ(-Mount.Normal, FVector::UpVector).ToQuat(), Location);
}

FVector AEncounterMazeLayout::GetFlamePosition(const FEncounterMazeTorchMount& Mount)
{
    return MakeTorchTransform(Mount).GetLocation() - Mount.Normal * 22.f + FVector(0.f, 0.f, 30.f);
}

void AEncounterMazeLayout::SetPresentationVisible(bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    TArray<UPrimitiveComponent*> Primitives;
    GetComponents(Primitives);
    for (UPrimitiveComponent* Primitive : Primitives) Primitive->SetVisibility(bVisible);
}

void AEncounterMazeLayout::ClearInstances()
{
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars, Torches};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->ClearInstances();
    TorchMounts.Reset();
    LayoutVariant = INDEX_NONE;
}

void AEncounterMazeLayout::AddBlock(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, const FQuat& Rotation)
{
    Component->AddInstance(FTransform(Rotation, Center, Size / 100.f));
}

void AEncounterMazeLayout::AddMasonryPanel(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Horizontal, const FVector& Vertical, const FVector& Normal, float Width, float Height, float Thickness)
{
    // Share staggered courses and recessed four-centimeter joints across walls, floors and ceilings.
    // 벽·바닥·천장에 엇갈린 석재 배열과 깊이가 있는 4cm 줄눈을 공통 적용합니다.
    const int32 CourseCount = FMath::Max(1, FMath::CeilToInt(Height / 105.f));
    constexpr float JointWidth = 4.f;
    const float BlockWidth = EncounterDungeonLayout::CellSize / 4.f;
    const int32 ColumnCount = FMath::CeilToInt(Width / BlockWidth);
    const float BlockHeight = Height / CourseCount;
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(Horizontal, Normal).ToQuat();
    for (int32 Row = 0; Row < CourseCount; ++Row)
    {
        const float Offset = Row % 2 == 0 ? 0.f : BlockWidth * 0.5f;
        for (int32 Column = 0; Column <= ColumnCount; ++Column)
        {
            const float Start = FMath::Max(-Width * 0.5f, -Width * 0.5f + Column * BlockWidth - Offset);
            const float End = FMath::Min(Width * 0.5f, -Width * 0.5f + (Column + 1) * BlockWidth - Offset);
            if (End - Start <= JointWidth) continue;
            const FVector BlockCenter = Center + Horizontal * ((Start + End) * 0.5f) + Vertical * ((Row + 0.5f) * BlockHeight - Height * 0.5f);
            const FVector BlockSize(End - Start - JointWidth, BlockHeight - JointWidth, Thickness);
            AddBlock(Component, BlockCenter, BlockSize, Rotation);
        }
    }
}

void AEncounterMazeLayout::AddDoorFrame(const FVector& Center, const FQuat& Rotation)
{
    AddBlock(Pillars, Center + Rotation.RotateVector(FVector(0.f, -170.f, 150.f)), FVector(60.f, 60.f, 300.f), Rotation);
    AddBlock(Pillars, Center + Rotation.RotateVector(FVector(0.f, 170.f, 150.f)), FVector(60.f, 60.f, 300.f), Rotation);
    AddBlock(Pillars, Center + FVector(0.f, 0.f, 360.f), FVector(60.f, 400.f, 120.f), Rotation);
}

bool AEncounterMazeLayout::BuildFromLayout(int32 InLayoutVariant)
{
    if (InLayoutVariant < 0 || InLayoutVariant >= EncounterDungeonLayout::VariantCount) return false;
    ClearInstances();
    LayoutVariant = InLayoutVariant;
    const TArray<FEncounterDungeonFloorRegion> Regions = EncounterDungeonLayout::GetFloorRegions(LayoutVariant);
    const float CellSize = EncounterDungeonLayout::CellSize;
    const float WallHeight = EncounterDungeonLayout::WallHeight;
    const FVector2D Neighbors[] = {FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f)};
    for (int32 RegionIndex = 0; RegionIndex < Regions.Num(); ++RegionIndex)
    {
        const FEncounterDungeonFloorRegion& Region = Regions[RegionIndex];
        const FVector Center = Region.Transform.GetLocation();
        const FVector Forward = Region.Transform.GetRotation().GetForwardVector();
        const FVector Right = Region.Transform.GetRotation().GetRightVector();
        const float Width = Region.HalfExtent.X * 2.f;
        const float Depth = Region.HalfExtent.Y * 2.f;
        // Offset overlapping floor faces by millimeters to avoid flicker at room and corridor joins.
        // 방과 통로의 연결부에서 겹치는 바닥 면의 깜빡임을 막도록 밀리미터 단위로 높이를 분리합니다.
        const float SurfaceOffset = -RegionIndex * 0.1f;
        // Back the joints with opaque slabs recessed two centimeters from the stone faces.
        // The slabs reach under and over the surrounding walls so no coplanar seam can leak the outside sky.
        // 석재 표면보다 2cm 들어간 불투명 판으로 줄눈을 막습니다.
        // 판을 주변 벽 아래·위까지 넓혀 맞닿기만 하는 경계로 바깥 하늘이 새지 않게 합니다.
        constexpr float SealOverlap = 32.f;
        AddBlock(Mortar, Center + FVector(0.f, 0.f, -12.f + SurfaceOffset), FVector(Width + SealOverlap * 2.f, Depth + SealOverlap * 2.f, 20.f), Region.Transform.GetRotation());
        AddBlock(Mortar, Center + FVector(0.f, 0.f, WallHeight + 12.f - SurfaceOffset), FVector(Width + SealOverlap * 2.f, Depth + SealOverlap * 2.f, 20.f), Region.Transform.GetRotation());
        TArray<FVector, TInlineAllocator<16>> WallEnds[4];
        AddMasonryPanel(Floor, Center + FVector(0.f, 0.f, -10.f + SurfaceOffset), Forward, Right, FVector::UpVector, Width, Depth, 20.f);
        AddMasonryPanel(Ceiling, Center + FVector(0.f, 0.f, WallHeight + 10.f - SurfaceOffset), Forward, Right, FVector::UpVector, Width, Depth, 20.f);
        for (int32 NeighborIndex = 0; NeighborIndex < 4; ++NeighborIndex)
        {
            const FVector2D& Neighbor = Neighbors[NeighborIndex];
            const FVector Normal = Region.Transform.TransformVectorNoScale(FVector(Neighbor.X, Neighbor.Y, 0.f));
            const FVector Tangent(-Neighbor.Y, Neighbor.X, 0.f);
            const FVector WallTangent = Region.Transform.TransformVectorNoScale(Tangent);
            const FVector Edge = Region.Transform.TransformPosition(FVector(Neighbor.X * Region.HalfExtent.X, Neighbor.Y * Region.HalfExtent.Y, 0.f));
            const FQuat WallRotation = FRotationMatrix::MakeFromXY(WallTangent, FVector::UpVector).ToQuat();
            for (const FVector2D& Span : EncounterDungeonLayout::GetExposedWallSpans(Regions, RegionIndex, Neighbor))
            {
                const float SpanWidth = Span.Y - Span.X;
                if (SpanWidth <= 4.f) continue;
                const FVector SpanCenter = Edge + WallTangent * ((Span.X + Span.Y) * 0.5f);
                // Extend the wall backing below the floor slab and above the ceiling slab so both junctions overlap.
                // 벽 뒤 판을 바닥 판 아래와 천장 판 위까지 늘려 두 접합부가 겹치게 합니다.
                AddBlock(Mortar, SpanCenter + Normal * 16.f + FVector(0.f, 0.f, WallHeight * 0.5f), FVector(SpanWidth, WallHeight + 44.f, 32.f), WallRotation);
                WallEnds[NeighborIndex].Add(Edge + WallTangent * Span.X);
                WallEnds[NeighborIndex].Add(Edge + WallTangent * Span.Y);
                AddMasonryPanel(WallBlocks, SpanCenter + Normal * 15.f + FVector(0.f, 0.f, WallHeight * 0.5f), WallTangent, FVector::UpVector, Normal, SpanWidth, WallHeight, 36.f);
                for (float Offset = Span.X + 200.f; Offset < Span.Y - 100.f; Offset += 800.f)
                {
                    FEncounterMazeTorchMount& Mount = TorchMounts.AddDefaulted_GetRef();
                    Mount.Center = Edge + WallTangent * Offset - Normal * (CellSize * 0.5f);
                    Mount.Normal = Normal;
                    if (Torches->GetStaticMesh()) Torches->AddInstance(MakeTorchTransform(Mount));
                }
            }
        }
        // Fill the outer square of every corner where two exposed walls actually meet; openings stay clear.
        // 노출된 두 벽이 실제로 만나는 모서리의 바깥 사각 영역만 채우며 통로 개구부는 비워 둡니다.
        const auto EndsAt = [](const TArray<FVector, TInlineAllocator<16>>& Ends, const FVector& Point)
        {
            return Ends.ContainsByPredicate([&Point](const FVector& End) { return FVector::DistSquared2D(End, Point) < 1.f; });
        };
        for (int32 XSide = 0; XSide < 2; ++XSide)
        {
            for (int32 YSide = 2; YSide < 4; ++YSide)
            {
                const FVector Corner = Region.Transform.TransformPosition(FVector(Neighbors[XSide].X * Region.HalfExtent.X, Neighbors[YSide].Y * Region.HalfExtent.Y, 0.f));
                if (!EndsAt(WallEnds[XSide], Corner) || !EndsAt(WallEnds[YSide], Corner)) continue;
                const FVector Outward = Region.Transform.TransformVectorNoScale(FVector(Neighbors[XSide].X, Neighbors[YSide].Y, 0.f));
                AddBlock(Mortar, Corner + Outward * 16.f + FVector(0.f, 0.f, WallHeight * 0.5f), FVector(32.f, 32.f, WallHeight + 44.f), Region.Transform.GetRotation());
            }
        }
    }
    for (int32 Direction = 0; Direction < 3; ++Direction)
    {
        const FQuat Rotation = EncounterDungeonLayout::GetBranchRotation(Direction);
        AddDoorFrame(EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, false), Rotation);
        AddDoorFrame(EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, true), Rotation);
        AddDoorFrame(EncounterDungeonLayout::GetLoopDoorCenter(Direction, LayoutVariant), Rotation);
    }
    AddDoorFrame(FVector(-CellSize * 0.5f, 0.f, 0.f));
    for (int32 Direction = 0; Direction < 3; ++Direction) AddDoorFrame(EncounterDungeonLayout::GetChoiceDoorCenter(Direction), EncounterDungeonLayout::GetChoiceDoorRotation(Direction));
    return true;
}
