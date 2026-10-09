#include "Game/Encounter/EncounterDungeonRoute.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEncounterDungeonRoute::AEncounterDungeonRoute()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = false;
    SetActorEnableCollision(false);
    RouteRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot"));
    SetRootComponent(RouteRoot);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("RouteCamera"));
    Camera->SetupAttachment(RouteRoot);
    Camera->SetFieldOfView(80.f);
    Camera->SetConstraintAspectRatio(false);
    Camera->bOverrideAspectRatioAxisConstraint = true;
    Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);
    Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
    Ceiling = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ceiling"));
    Mortar = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mortar"));
    WallBlocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallBlocks"));
    Pillars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Pillars"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Cube.Object;
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars};
    for (UInstancedStaticMeshComponent* Shape : Shapes)
    {
        Shape->SetupAttachment(RouteRoot);
        Shape->SetStaticMesh(CubeMesh);
        Shape->SetMobility(EComponentMobility::Movable);
        Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shape->SetGenerateOverlapEvents(false);
        Shape->SetCanEverAffectNavigation(false);
        Shape->SetCastShadow(true);
    }
}

void AEncounterDungeonRoute::AddBlock(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size)
{
    Component->AddInstance(FTransform(FQuat::Identity, Center, Size / 100.f));
}

void AEncounterDungeonRoute::ApplyMaterial(UInstancedStaticMeshComponent* Component, const FLinearColor& Color)
{
    Component->SetMaterial(0, RouteMaterial);
    if (RouteMaterial)
    {
        if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0)) Material->SetVectorParameterValue(TEXT("Tint"), Color);
    }
}

void AEncounterDungeonRoute::AddLight(const FVector& Position)
{
    UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
    Light->SetupAttachment(RouteRoot);
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetRelativeLocation(Position);
    Light->SetIntensityUnits(ELightUnits::Lumens);
    Light->SetUseInverseSquaredFalloff(true);
    Light->SetIntensity(3600.f);
    Light->SetAttenuationRadius(1150.f);
    Light->SetLightColor(FLinearColor(1.f, 0.65f, 0.34f));
    Light->SetCastShadows(false);
    Light->SetVisibility(bPresentationVisible);
    AddInstanceComponent(Light);
    Light->RegisterComponent();
    Lights.Add(Light);
}

bool AEncounterDungeonRoute::ConfigureLayout(int32 InLayoutVariant)
{
    if (InLayoutVariant < 0 || InLayoutVariant >= EncounterDungeonLayout::VariantCount || !GetWorld() || GetNetMode() == NM_DedicatedServer) return false;
    if (LayoutVariant == InLayoutVariant)
    {
        PrepareGeometry(RouteMaterial);
        return bGeometryPrepared;
    }
    // Replace only scenery owned by this cached route; never alter the authored NPC templates.
    // 캐시된 통로가 소유한 장식만 교체하며 작성된 NPC 원본은 변경하지 않습니다.
    StopTravel();
    DestroyPresentedStage();
    ClearGeometry();
    LayoutVariant = InLayoutVariant;
    ResetAtJunction();
    return bGeometryPrepared;
}

void AEncounterDungeonRoute::SetPresentationVisible(bool bVisible)
{
    // Hide prewarmed lights explicitly, including components created after this call.
    // 이후 생성하는 컴포넌트를 포함해 미리 준비한 구간의 조명까지 명시적으로 숨깁니다.
    bPresentationVisible = bVisible;
    SetActorHiddenInGame(!bVisible);
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->SetVisibility(bVisible);
    for (UPointLightComponent* Light : Lights)
    {
        if (IsValid(Light) && Light->GetOwner() == this) Light->SetVisibility(bVisible);
    }
    if (IsValid(PresentedStage) && PresentedStage->GetOwner() == this) PresentedStage->SetPresentationVisible(bVisible);
}

void AEncounterDungeonRoute::ClearGeometry()
{
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->ClearInstances();
    for (UPointLightComponent* Light : Lights)
    {
        if (IsValid(Light) && Light->GetOwner() == this) Light->DestroyComponent();
    }
    Lights.Reset();
    bGeometryPrepared = false;
}

void AEncounterDungeonRoute::PrepareGeometry(UMaterialInterface* Material)
{
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return;
    if (!Material && !RouteMaterial)
    {
        for (TActorIterator<AEncounterPrototypeStage> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It) && It->GetOwner() != this && It->PrototypeMaterial)
            {
                Material = It->PrototypeMaterial;
                break;
            }
        }
    }
    if (!bGeometryPrepared || (Material && RouteMaterial != Material))
    {
        if (Material) RouteMaterial = Material;
        ApplyMaterial(Floor, FLinearColor(0.21f, 0.19f, 0.16f));
        ApplyMaterial(Ceiling, FLinearColor(0.14f, 0.13f, 0.12f));
        ApplyMaterial(Mortar, FLinearColor(0.055f, 0.05f, 0.04f));
        ApplyMaterial(WallBlocks, FLinearColor(0.29f, 0.265f, 0.22f));
        ApplyMaterial(Pillars, FLinearColor(0.23f, 0.205f, 0.17f));
    }
    if (bGeometryPrepared) return;

    const TSet<FIntPoint> Cells = EncounterDungeonLayout::GetFloorCells(LayoutVariant);
    const float CellSize = EncounterDungeonLayout::CellSize;
    const float WallHeight = EncounterDungeonLayout::WallHeight;
    const FIntPoint Neighbors[] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};
    TSet<FIntPoint> PillarCorners;
    for (const FIntPoint& Cell : Cells)
    {
        const FVector Center(Cell.X * CellSize, Cell.Y * CellSize, 0.f);
        AddBlock(Floor, Center + FVector(0.f, 0.f, -10.f), FVector(CellSize, CellSize, 20.f));
        AddBlock(Ceiling, Center + FVector(0.f, 0.f, WallHeight + 10.f), FVector(CellSize, CellSize, 20.f));
        for (const FIntPoint& Neighbor : Neighbors)
        {
            if (Cells.Contains(Cell + Neighbor)) continue;
            const FVector Normal(Neighbor.X, Neighbor.Y, 0.f);
            const FVector Tangent(-Neighbor.Y, Neighbor.X, 0.f);
            const bool bAlongY = Neighbor.X != 0;
            const FVector WallCenter = Center + Normal * (CellSize * 0.5f + 16.f) + FVector(0.f, 0.f, WallHeight * 0.5f);
            AddBlock(Mortar, WallCenter, bAlongY ? FVector(32.f, CellSize, WallHeight) : FVector(CellSize, 32.f, WallHeight));

            // Short alternating masonry joints reveal depth without adding gameplay collision.
            // 짧고 엇갈린 석재 이음으로 깊이를 표현하며 게임플레이 충돌은 추가하지 않습니다.
            for (int32 Row = 0; Row < 4; ++Row)
            {
                const float BlockHeight = WallHeight / 4.f;
                const float Offset = Row % 2 == 0 ? 0.f : CellSize / 8.f;
                for (int32 Column = 0; Column < 5; ++Column)
                {
                    const float Start = FMath::Max(-CellSize * 0.5f, -CellSize * 0.5f + Column * CellSize / 4.f - Offset);
                    const float End = FMath::Min(CellSize * 0.5f, -CellSize * 0.5f + (Column + 1) * CellSize / 4.f - Offset);
                    if (End - Start <= 4.f) continue;
                    const FVector BlockCenter = Center + Normal * (CellSize * 0.5f + 15.f) + Tangent * ((Start + End) * 0.5f) + FVector(0.f, 0.f, (Row + 0.5f) * BlockHeight);
                    const FVector BlockSize = bAlongY ? FVector(36.f, End - Start - 4.f, BlockHeight - 4.f) : FVector(End - Start - 4.f, 36.f, BlockHeight - 4.f);
                    AddBlock(WallBlocks, BlockCenter, BlockSize);
                }
            }
            const FIntPoint EdgeCenter(2 * Cell.X + Neighbor.X, 2 * Cell.Y + Neighbor.Y);
            const FIntPoint EdgeTangent(-Neighbor.Y, Neighbor.X);
            PillarCorners.Add(EdgeCenter + EdgeTangent);
            PillarCorners.Add(EdgeCenter - EdgeTangent);
        }
        if ((Cell.X + Cell.Y) % 2 == 0) AddLight(Center + FVector(0.f, 0.f, WallHeight - 80.f));
    }
    for (const FIntPoint& Corner : PillarCorners)
    {
        const FVector Center(Corner.X * CellSize * 0.5f, Corner.Y * CellSize * 0.5f, WallHeight * 0.5f);
        AddBlock(Pillars, Center, FVector(32.f, 32.f, WallHeight));
        AddBlock(Pillars, FVector(Center.X, Center.Y, 24.f), FVector(46.f, 46.f, 48.f));
        AddBlock(Pillars, FVector(Center.X, Center.Y, WallHeight - 24.f), FVector(46.f, 46.f, 48.f));
    }
    bGeometryPrepared = true;
}

AEncounterPrototypeStage* AEncounterDungeonRoute::ConfigureStage(const AEncounterPrototypeStage* Template, int32 Direction)
{
    if (!IsValid(Template) || Template == PresentedStage || !GetWorld() || GetNetMode() == NM_DedicatedServer || Direction < 0 || Direction > 2) return nullptr;
    PrepareGeometry(Template->PrototypeMaterial);
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.ObjectFlags |= RF_Transient;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEncounterPrototypeStage* Stage = GetWorld()->SpawnActor<AEncounterPrototypeStage>(AEncounterPrototypeStage::StaticClass(), EncounterDungeonLayout::GetStageTransform(Direction, LayoutVariant) * GetActorTransform(), Parameters);
    if (!Stage) return nullptr;
    Stage->SetPresentationVisible(bPresentationVisible);
    Stage->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
    Stage->StageId = Template->StageId;
    Stage->StageTitle = Template->StageTitle;
    Stage->RequiredTags = Template->RequiredTags;
    Stage->ExcludedTags = Template->ExcludedTags;
    Stage->Priority = Template->Priority;
    Stage->Tint = Template->Tint;
    Stage->PrototypeMaterial = Template->PrototypeMaterial;
    Stage->VisualStyle = Template->VisualStyle;
    Stage->VisualCatalog = Template->VisualCatalog;
    Stage->bUseLibraryVisuals = Template->bUseLibraryVisuals;
    Stage->RefreshPrototype();
    Stage->Camera->SetRelativeLocation(FVector(-800.f, 0.f, EncounterDungeonLayout::EyeHeight));
    // Preserve the corridor arrival position while aiming through the central shop UI gap.
    // 통로 도착 위치는 유지하고 상점 UI의 중앙 빈 공간을 향해 시선을 맞춥니다.
    Stage->Camera->SetRelativeRotation((FVector(35.f, -170.f, 145.f) - Stage->Camera->GetRelativeLocation()).Rotation());
    Stage->Camera->SetFieldOfView(80.f);
    Stage->StopPresentation();
    StopTravel();
    DestroyPresentedStage();
    PresentedStage = Stage;
    StageDirection = Direction;
    return Stage;
}

void AEncounterDungeonRoute::ResetAtJunction()
{
    StopTravel();
    DestroyPresentedStage();
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return;
    PrepareGeometry(RouteMaterial);
    const TArray<FVector> Path = EncounterDungeonLayout::GetPath(0, LayoutVariant);
    if (Path.IsEmpty()) return;
    Camera->SetRelativeLocation(Path[0]);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    Camera->SetFieldOfView(80.f);
}

bool AEncounterDungeonRoute::StartTravel(int32 Direction)
{
    if (bTraveling || GetNetMode() == NM_DedicatedServer || Direction != StageDirection || !IsValid(PresentedStage) || !PresentedStage->Camera) return false;
    const TArray<FVector> Path = EncounterDungeonLayout::GetPath(Direction, LayoutVariant);
    if (Path.Num() < 2) return false;
    const FVector Arrival = GetActorTransform().InverseTransformPosition(PresentedStage->Camera->GetComponentLocation());
    if (!Arrival.Equals(Path.Last(), 1.f)) return false;
    TArray<FEncounterDungeonTravelSegment> Segments;
    FQuat CurrentRotation = FQuat::Identity;
    float TotalDistance = 0.f;
    int32 TurnCount = 0;
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const FVector Delta = Path[Index] - Path[Index - 1];
        if (Path[Index - 1].ContainsNaN() || Path[Index].ContainsNaN() || !FMath::IsNearlyZero(Delta.Z) || (!FMath::IsNearlyZero(Delta.X) && !FMath::IsNearlyZero(Delta.Y)) || Delta.IsNearlyZero()) return false;
        const FQuat Facing = Delta.Rotation().Quaternion();
        if (!CurrentRotation.Equals(Facing, 0.001f))
        {
            Segments.Add({Path[Index - 1], Path[Index - 1], CurrentRotation, Facing, 0.f});
            ++TurnCount;
        }
        Segments.Add({Path[Index - 1], Path[Index], Facing, Facing, static_cast<float>(Delta.Size())});
        TotalDistance += static_cast<float>(Delta.Size());
        CurrentRotation = Facing;
    }
    const FQuat FinalRotation = GetActorQuat().Inverse() * PresentedStage->Camera->GetComponentQuat();
    if (!CurrentRotation.Equals(FinalRotation, 0.001f))
    {
        Segments.Add({Path.Last(), Path.Last(), CurrentRotation, FinalRotation, 0.f});
        ++TurnCount;
    }
    const float TurnDuration = TurnCount > 0 ? FMath::Min(0.18f, 1.2f / TurnCount) : 0.f;
    const float MoveDuration = 2.8f - TurnCount * TurnDuration;
    for (FEncounterDungeonTravelSegment& Segment : Segments)
    {
        Segment.Duration = Segment.Start.Equals(Segment.End) ? TurnDuration : MoveDuration * Segment.Duration / TotalDistance;
    }
    StopTravel();
    TravelSegments = MoveTemp(Segments);
    Camera->SetRelativeLocation(Path[0]);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    Camera->SetFieldOfView(80.f);
    bTraveling = true;
    SetActorTickEnabled(true);
    return true;
}

bool AEncounterDungeonRoute::IsTraveling() const
{
    return bTraveling;
}

AEncounterPrototypeStage* AEncounterDungeonRoute::GetPresentedStage() const
{
    return IsValid(PresentedStage) ? PresentedStage.Get() : nullptr;
}

void AEncounterDungeonRoute::StopTravel()
{
    bTraveling = false;
    SetActorTickEnabled(false);
    TravelSegments.Reset();
    TravelSegmentIndex = 0;
    SegmentSeconds = 0.f;
}

void AEncounterDungeonRoute::FinishAtStage()
{
    StopTravel();
    if (!IsValid(PresentedStage) || !PresentedStage->Camera) return;
    Camera->SetWorldLocationAndRotation(PresentedStage->Camera->GetComponentLocation(), PresentedStage->Camera->GetComponentQuat());
    Camera->SetFieldOfView(PresentedStage->Camera->FieldOfView);
}

void AEncounterDungeonRoute::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bTraveling || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
    if (!IsValid(PresentedStage))
    {
        StopTravel();
        OnTravelFinished.Broadcast();
        return;
    }
    float RemainingSeconds = DeltaSeconds;
    while (TravelSegments.IsValidIndex(TravelSegmentIndex))
    {
        const FEncounterDungeonTravelSegment& Segment = TravelSegments[TravelSegmentIndex];
        const float Step = FMath::Min(RemainingSeconds, Segment.Duration - SegmentSeconds);
        SegmentSeconds += Step;
        RemainingSeconds -= Step;
        const float Alpha = FMath::Clamp(SegmentSeconds / Segment.Duration, 0.f, 1.f);
        const float SmoothedAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
        Camera->SetRelativeLocation(FMath::Lerp(Segment.Start, Segment.End, SmoothedAlpha));
        Camera->SetRelativeRotation(FQuat::Slerp(Segment.StartRotation, Segment.EndRotation, SmoothedAlpha));
        if (SegmentSeconds < Segment.Duration) break;
        ++TravelSegmentIndex;
        SegmentSeconds = 0.f;
        if (RemainingSeconds <= 0.f) break;
    }
    if (!TravelSegments.IsValidIndex(TravelSegmentIndex))
    {
        FinishAtStage();
        OnTravelFinished.Broadcast();
    }
}

void AEncounterDungeonRoute::DestroyPresentedStage()
{
    if (IsValid(PresentedStage) && PresentedStage->GetOwner() == this) PresentedStage->Destroy();
    PresentedStage = nullptr;
    StageDirection = INDEX_NONE;
}

void AEncounterDungeonRoute::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopTravel();
    OnTravelFinished.Clear();
    DestroyPresentedStage();
    Super::EndPlay(EndPlayReason);
}
