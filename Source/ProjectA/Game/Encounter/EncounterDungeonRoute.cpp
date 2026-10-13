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
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
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
    // Keep darkness stable instead of allowing eye adaptation to brighten the underground corridor.
    // 자동 노출이 지하 통로를 밝히지 않도록 어두운 노출을 고정합니다.
    Camera->PostProcessBlendWeight = 1.f;
    Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
    Camera->PostProcessSettings.AutoExposureBias = -0.4f;
    Camera->PostProcessSettings.bOverride_IndirectLightingIntensity = true;
    Camera->PostProcessSettings.IndirectLightingIntensity = 0.08f;
    Camera->PostProcessSettings.bOverride_BloomIntensity = true;
    Camera->PostProcessSettings.BloomIntensity = 0.12f;
    Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
    Camera->PostProcessSettings.VignetteIntensity = 0.18f;
    Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
    Ceiling = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ceiling"));
    Mortar = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mortar"));
    WallBlocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallBlocks"));
    Pillars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Pillars"));
    LeftDoors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftDoors"));
    RightDoors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightDoors"));
    DoorSeals = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorSeals"));
    Torches = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Torches"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Cube.Object;
    // Preserve cookable asset references without rooting editable material graphs during class initialization.
    // 클래스 초기화 중 편집할 재질 그래프를 루트에 고정하지 않고 쿠킹 가능한 에셋 참조를 유지합니다.
    WallMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials/M_EncounterStoneWall.M_EncounterStoneWall")));
    FloorMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials/M_EncounterStoneFloor.M_EncounterStoneFloor")));
    CeilingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials/M_EncounterStoneCeiling.M_EncounterStoneCeiling")));
    TorchMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_torch_wall_dungeon.SM_PROP_torch_wall_dungeon")));
    TorchFlame = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Fantastic_Dungeon_Pack/effects/PS_FX_fire_dungeon_01_Niagara.PS_FX_fire_dungeon_01_Niagara")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> LeftDoor(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/modular/gateway/MOD_Gateway_Door_01_large_door_L.MOD_Gateway_Door_01_large_door_L"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RightDoor(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/modular/gateway/MOD_Gateway_Door_01_large_door_R.MOD_Gateway_Door_01_large_door_R"));
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars, DoorSeals};
    for (UInstancedStaticMeshComponent* Shape : Shapes)
    {
        Shape->SetupAttachment(RouteRoot);
        Shape->SetStaticMesh(CubeMesh);
        Shape->SetMobility(EComponentMobility::Movable);
        Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shape->SetGenerateOverlapEvents(false);
        Shape->SetCanEverAffectNavigation(false);
        Shape->SetCastShadow(true);
        Shape->SetLightingChannels(false, true, false);
    }
    Torches->SetupAttachment(RouteRoot);
    Torches->SetMobility(EComponentMobility::Movable);
    Torches->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Torches->SetGenerateOverlapEvents(false);
    Torches->SetCanEverAffectNavigation(false);
    Torches->SetCastShadow(false);
    Torches->SetLightingChannels(false, true, false);
    if (LeftDoor.Object) DoorSeals->SetMaterial(0, LeftDoor.Object->GetMaterial(0));
    UInstancedStaticMeshComponent* Doors[] = {LeftDoors, RightDoors};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Doors[Index]->SetupAttachment(RouteRoot);
        Doors[Index]->SetStaticMesh(Index == 0 ? LeftDoor.Object : RightDoor.Object);
        Doors[Index]->SetMobility(EComponentMobility::Movable);
        Doors[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Doors[Index]->SetGenerateOverlapEvents(false);
        Doors[Index]->SetCanEverAffectNavigation(false);
        Doors[Index]->SetCastShadow(true);
        Doors[Index]->SetLightingChannels(false, true, false);
    }
}

void AEncounterDungeonRoute::AddDoorway(const FVector& Center, const FQuat& Rotation)
{
    AddBlock(Pillars, Center + Rotation.RotateVector(FVector(0.f, -170.f, 150.f)), FVector(60.f, 60.f, 300.f), Rotation);
    AddBlock(Pillars, Center + Rotation.RotateVector(FVector(0.f, 170.f, 150.f)), FVector(60.f, 60.f, 300.f), Rotation);
    AddBlock(Pillars, Center + FVector(0.f, 0.f, 360.f), FVector(60.f, 400.f, 120.f), Rotation);
    const int32 Index = DoorCenters.Add(Center);
    DoorRotations.Add(Rotation);
    LeftDoors->AddInstance(FTransform::Identity);
    RightDoors->AddInstance(FTransform::Identity);
    SetDoorOpen(Index, 0.f);
}

void AEncounterDungeonRoute::SetDoorOpen(int32 Index, float Alpha)
{
    if (!DoorCenters.IsValidIndex(Index) || !DoorRotations.IsValidIndex(Index)) return;
    UInstancedStaticMeshComponent* Doors[] = {LeftDoors, RightDoors};
    for (int32 Side = 0; Side < 2; ++Side)
    {
        UInstancedStaticMeshComponent* Door = Doors[Side];
        if (!Door->GetStaticMesh()) continue;
        const FBox Bounds = Door->GetStaticMesh()->GetBoundingBox();
        const FVector Size = Bounds.GetSize();
        if (!Bounds.IsValid || Size.GetMin() <= UE_SMALL_NUMBER) continue;
        // Fit the original leaves around their hinges without assuming an imported pivot or modifying assets.
        // 임포트 피벗을 가정하거나 에셋을 변경하지 않고 원본 문짝을 경첩 기준으로 맞춥니다.
        const float Sign = Side == 0 ? 1.f : -1.f;
        const FVector Scale = FVector(136.f, 24.f, 300.f) / Size;
        const FQuat Rotation = FRotator(0.f, 90.f - Sign * 100.f * FMath::Clamp(Alpha, 0.f, 1.f), 0.f).Quaternion();
        const FVector Offset = FVector(Sign * 68.f, 0.f, 150.f) - Bounds.GetCenter() * Scale;
        const FQuat DoorRotation = DoorRotations[Index] * Rotation;
        const FVector Hinge = DoorCenters[Index] + DoorRotations[Index].RotateVector(FVector(0.f, -Sign * 136.f, 0.f));
        Door->UpdateInstanceTransform(Index, FTransform(DoorRotation, Hinge + DoorRotation.RotateVector(Offset), Scale), false, true, true);
    }
    if (Index == EncounterDungeonLayout::LoopEntranceDoorIndex && DoorSeals->GetInstanceCount() > 3)
    {
        // Cover mesh seams at the matching closed-door pose and remove the cover as the next door opens.
        // 일치하는 닫힌 문 구도에서 메시 틈을 가리고 다음 문이 열리면 가림판을 제거합니다.
        const FVector Scale = Alpha > 0.f ? FVector::ZeroVector : FVector(0.04f, 2.8f, 3.f);
        DoorSeals->UpdateInstanceTransform(3, FTransform(FQuat::Identity, DoorCenters[Index] + FVector(20.f, 0.f, 150.f), Scale), false, true, true);
    }
}

void AEncounterDungeonRoute::AddBlock(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, const FQuat& Rotation)
{
    Component->AddInstance(FTransform(Rotation, Center, Size / 100.f));
}

void AEncounterDungeonRoute::ApplyMaterial(UInstancedStaticMeshComponent* Component, const FLinearColor& Color)
{
    Component->SetMaterial(0, RouteMaterial);
    if (RouteMaterial)
    {
        if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0)) Material->SetVectorParameterValue(TEXT("Tint"), Color);
    }
}

void AEncounterDungeonRoute::AddMasonryPanel(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Horizontal, const FVector& Vertical, const FVector& Normal, float Width, float Height, float Thickness)
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

void AEncounterDungeonRoute::AddLight(const FVector& Position)
{
    UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
    Light->SetupAttachment(RouteRoot);
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetRelativeLocation(Position);
    Light->SetIntensityUnits(ELightUnits::Lumens);
    Light->SetUseInverseSquaredFalloff(true);
    Light->SetIntensity(1100.f);
    Light->SetAttenuationRadius(650.f);
    Light->SetLightColor(FLinearColor(1.f, 0.52f, 0.22f));
    Light->SetLightingChannels(false, true, false);
    Light->SetCastShadows(true);
    Light->SetSourceRadius(8.f);
    Light->ShadowResolutionScale = 0.35f;
    Light->MaxDrawDistance = 1400.f;
    Light->MaxDistanceFadeRange = 300.f;
    Light->SetVisibility(bPresentationVisible);
    AddInstanceComponent(Light);
    Light->RegisterComponent();
    Lights.Add(Light);
}

void AEncounterDungeonRoute::AddTorch(const FVector& Center, const FVector& Normal)
{
    // Reference the original wall sconce and flame directly, with only local presentation components.
    // 원본 벽 횃불과 불꽃을 직접 참조하고 로컬 연출 컴포넌트만 생성합니다.
    if (!Torches->GetStaticMesh()) Torches->SetStaticMesh(TorchMesh.LoadSynchronous());
    const FVector Mount = Center + Normal * (EncounterDungeonLayout::CellSize * 0.5f - 15.f) + FVector(0.f, 0.f, 220.f);
    const FQuat Rotation = FRotationMatrix::MakeFromYZ(-Normal, FVector::UpVector).ToQuat();
    if (Torches->GetStaticMesh()) Torches->AddInstance(FTransform(Rotation, Mount));
    const FVector FlamePosition = Mount - Normal * 22.f + FVector(0.f, 0.f, 30.f);
    AddLight(FlamePosition);
    // Approximate a little torch bounce locally because this project does not enable Lumen.
    // Lumen을 사용하지 않는 프로젝트이므로 횃불의 약한 반사광을 통로 내부에 국소적으로 보완합니다.
    UPointLightComponent* Bounce = NewObject<UPointLightComponent>(this);
    Bounce->SetupAttachment(RouteRoot);
    Bounce->SetRelativeLocation(Center + FVector(0.f, 0.f, 180.f));
    Bounce->SetIntensityUnits(ELightUnits::Lumens);
    Bounce->SetUseInverseSquaredFalloff(true);
    Bounce->SetIntensity(350.f);
    Bounce->SetAttenuationRadius(950.f);
    Bounce->SetLightColor(FLinearColor(1.f, 0.66f, 0.38f));
    Bounce->SetLightingChannels(false, true, false);
    Bounce->SetCastShadows(false);
    Bounce->MaxDrawDistance = 1400.f;
    Bounce->MaxDistanceFadeRange = 300.f;
    Bounce->SetVisibility(bPresentationVisible);
    AddInstanceComponent(Bounce);
    Bounce->RegisterComponent();
    Lights.Add(Bounce);
    UNiagaraSystem* System = TorchFlame.LoadSynchronous();
    if (!System) return;
    UNiagaraComponent* Effect = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
    Effect->SetAutoActivate(false);
    Effect->SetupAttachment(RouteRoot);
    Effect->SetRelativeLocation(FlamePosition);
    Effect->SetRelativeScale3D(FVector(0.35f));
    Effect->SetAsset(System);
    Effect->SetCastShadow(false);
    Effect->SetCanEverAffectNavigation(false);
    Effect->SetVisibility(bPresentationVisible);
    AddInstanceComponent(Effect);
    Effect->RegisterComponent();
    // Niagara owns component ticks according to its solo or shared simulation mode.
    // Niagara의 단독 또는 공유 시뮬레이션 방식에 따른 컴포넌트 틱은 엔진이 관리합니다.
    if (bPresentationVisible) Effect->Activate(true);
    TorchEffects.Add(Effect);
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
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars, LeftDoors, RightDoors, DoorSeals, Torches};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->SetVisibility(bVisible);
    for (UPointLightComponent* Light : Lights)
    {
        if (IsValid(Light) && Light->GetOwner() == this) Light->SetVisibility(bVisible);
    }
    for (UNiagaraComponent* Effect : TorchEffects)
    {
        if (!IsValid(Effect) || Effect->GetOwner() != this) continue;
        Effect->SetVisibility(bVisible);
        // Activation manages Niagara ticking; visibility must not override its simulation mode.
        // 활성화가 Niagara 틱을 관리하므로 표시 상태로 시뮬레이션 방식의 틱을 덮어쓰지 않습니다.
        if (bVisible) Effect->Activate(false);
        else Effect->DeactivateImmediate();
    }
    if (IsValid(PresentedStage) && PresentedStage->GetOwner() == this) PresentedStage->SetPresentationVisible(bVisible);
}

void AEncounterDungeonRoute::ClearGeometry()
{
    UInstancedStaticMeshComponent* Shapes[] = {Floor, Ceiling, Mortar, WallBlocks, Pillars, LeftDoors, RightDoors, DoorSeals, Torches};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->ClearInstances();
    DoorCenters.Reset();
    DoorRotations.Reset();
    for (UPointLightComponent* Light : Lights)
    {
        if (IsValid(Light) && Light->GetOwner() == this) Light->DestroyComponent();
    }
    Lights.Reset();
    for (UNiagaraComponent* Effect : TorchEffects)
    {
        if (IsValid(Effect) && Effect->GetOwner() == this) Effect->DestroyComponent();
    }
    TorchEffects.Reset();
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
        Floor->SetMaterial(0, FloorMaterial.LoadSynchronous());
        Ceiling->SetMaterial(0, CeilingMaterial.LoadSynchronous());
        ApplyMaterial(Mortar, FLinearColor(0.055f, 0.05f, 0.04f));
        // Persist instancing support and world-aligned original textures on every dungeon surface.
        // 인스턴싱 지원과 월드 정렬 원본 텍스처를 모든 미로 표면에 유지합니다.
        UMaterialInterface* LoadedWallMaterial = WallMaterial.LoadSynchronous();
        WallBlocks->SetMaterial(0, LoadedWallMaterial);
        Pillars->SetMaterial(0, LoadedWallMaterial);
    }
    if (bGeometryPrepared) return;

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
                    AddTorch(Edge + WallTangent * Offset - Normal * (CellSize * 0.5f), Normal);
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
        AddDoorway(EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, false), Rotation);
        AddDoorway(EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, true), Rotation);
        const FVector LoopDoor = EncounterDungeonLayout::GetLoopDoorCenter(Direction, LayoutVariant);
        AddDoorway(LoopDoor, Rotation);
        AddBlock(DoorSeals, LoopDoor + Rotation.RotateVector(FVector(20.f, 0.f, 150.f)), FVector(4.f, 280.f, 300.f), Rotation);
    }
    const FVector LoopEntrance(-CellSize * 0.5f, 0.f, 0.f);
    AddDoorway(LoopEntrance);
    AddBlock(DoorSeals, LoopEntrance + FVector(20.f, 0.f, 150.f), FVector(4.f, 280.f, 300.f));
    for (int32 Direction = 0; Direction < 3; ++Direction) AddDoorway(EncounterDungeonLayout::GetChoiceDoorCenter(Direction), EncounterDungeonLayout::GetChoiceDoorRotation(Direction));
    bGeometryPrepared = true;
}

AEncounterPrototypeStage* AEncounterDungeonRoute::ConfigureStage(const AEncounterPrototypeStage* Template, int32 Direction, const FRunEncounterOffer& Offer)
{
    if (!IsValid(Template) || Template == PresentedStage || !GetWorld() || GetNetMode() == NM_DedicatedServer || Direction < 0 || Direction > 2) return nullptr;
    PrepareGeometry(Template->PrototypeMaterial);
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.ObjectFlags |= RF_Transient;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEncounterPrototypeStage* Stage = GetWorld()->SpawnActor<AEncounterPrototypeStage>(AEncounterPrototypeStage::StaticClass(), EncounterDungeonLayout::GetStageTransform(Direction, LayoutVariant) * GetActorTransform(), Parameters);
    if (!Stage) return nullptr;
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
    Stage->SetEncounterOffer(Offer);
    Stage->Camera->SetRelativeLocation(FVector(-800.f, 0.f, EncounterDungeonLayout::EyeHeight));
    // Preserve the corridor arrival position while aiming through the central shop UI gap.
    // 통로 도착 위치는 유지하고 상점 UI의 중앙 빈 공간을 향해 시선을 맞춥니다.
    FVector Focus;
    float FocusRadius = 0.f;
    const FVector Aim = Stage->GetPresentationFocus(Focus, FocusRadius) ? Stage->GetActorTransform().InverseTransformPosition(Focus) + FVector(0.f, -40.f, 0.f) : FVector(35.f, -170.f, 145.f);
    Stage->Camera->SetRelativeRotation((Aim - Stage->Camera->GetRelativeLocation()).Rotation());
    Stage->Camera->SetFieldOfView(80.f);
    Stage->Camera->PostProcessSettings = Camera->PostProcessSettings;
    Stage->Camera->PostProcessBlendWeight = Camera->PostProcessBlendWeight;
    Stage->UseDungeonLighting();
    Stage->SetPresentationVisible(bPresentationVisible);
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
    for (int32 Index = 0; Index < DoorCenters.Num(); ++Index) SetDoorOpen(Index, 0.f);
    const TArray<FVector> Path = EncounterDungeonLayout::GetPath(0, LayoutVariant);
    if (Path.IsEmpty()) return;
    Camera->SetRelativeLocation(Path[0]);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    Camera->SetFieldOfView(EncounterDungeonLayout::ChoiceFieldOfView);
}

bool AEncounterDungeonRoute::StartTravel(int32 Direction)
{
    if (bTraveling || GetNetMode() == NM_DedicatedServer || Direction != StageDirection || !IsValid(PresentedStage) || !PresentedStage->Camera) return false;
    TArray<FVector> Path = EncounterDungeonLayout::GetPath(Direction, LayoutVariant);
    if (Path.Num() < 2) return false;
    const FVector Arrival = GetActorTransform().InverseTransformPosition(PresentedStage->Camera->GetComponentLocation());
    if (!Arrival.Equals(Path.Last(), 1.f)) return false;
    const FVector Forward = EncounterDungeonLayout::GetBranchRotation(Direction).GetForwardVector();
    const FVector DoorApproach = EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, false) - Forward * EncounterDungeonLayout::EntranceDoorApproachDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight);
    Path.Insert(DoorApproach, Path.Num() - 1);
    const FVector ChoiceApproach = EncounterDungeonLayout::GetChoiceDoorCenter(Direction) - EncounterDungeonLayout::GetChoiceDoorRotation(Direction).GetForwardVector() * EncounterDungeonLayout::EntranceDoorApproachDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight);
    TArray<FEncounterDungeonTravelSegment> Segments;
    FQuat CurrentRotation = FQuat::Identity;
    float TotalDistance = 0.f;
    int32 TurnCount = 0;
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const FVector Delta = Path[Index] - Path[Index - 1];
        if (Path[Index - 1].ContainsNaN() || Path[Index].ContainsNaN() || !FMath::IsNearlyZero(Delta.Z) || Delta.IsNearlyZero()) return false;
        const FQuat Facing = Delta.Rotation().Quaternion();
        if (!CurrentRotation.Equals(Facing, 0.001f))
        {
            Segments.Add({Path[Index - 1], Path[Index - 1], CurrentRotation, Facing, 0.f});
            ++TurnCount;
        }
        Segments.Add({Path[Index - 1], Path[Index], Facing, Facing, static_cast<float>(Delta.Size())});
        CurrentRotation = Facing;
        if (Path[Index].Equals(ChoiceApproach))
        {
            const FQuat DoorRotation = EncounterDungeonLayout::GetChoiceDoorRotation(Direction);
            if (!CurrentRotation.Equals(DoorRotation, 0.001f))
            {
                Segments.Add({ChoiceApproach, ChoiceApproach, CurrentRotation, DoorRotation, 0.f});
                ++TurnCount;
            }
            Segments.Add({ChoiceApproach, ChoiceApproach, DoorRotation, DoorRotation, EncounterDungeonLayout::DoorOpenDuration, EncounterDungeonLayout::ChoiceEntranceDoorIndex + Direction});
            CurrentRotation = DoorRotation;
        }
        if (Path[Index].Equals(DoorApproach)) Segments.Add({DoorApproach, DoorApproach, Facing, Facing, EncounterDungeonLayout::DoorOpenDuration, Direction * EncounterDungeonLayout::DoorsPerRoom});
        TotalDistance += static_cast<float>(Delta.Size());
    }
    const FQuat FinalRotation = GetActorQuat().Inverse() * PresentedStage->Camera->GetComponentQuat();
    if (!CurrentRotation.Equals(FinalRotation, 0.001f))
    {
        Segments.Add({Path.Last(), Path.Last(), CurrentRotation, FinalRotation, 0.f});
        ++TurnCount;
    }
    const float TurnDuration = TurnCount > 0 ? FMath::Min(EncounterDungeonLayout::TurnDuration, 1.4f / TurnCount) : 0.f;
    const float MoveDuration = EncounterDungeonLayout::ArrivalTravelDuration - TurnCount * TurnDuration;
    for (FEncounterDungeonTravelSegment& Segment : Segments)
    {
        if (Segment.DoorIndex == INDEX_NONE) Segment.Duration = Segment.Start.Equals(Segment.End) ? TurnDuration : FMath::Max(MoveDuration * Segment.Duration / TotalDistance, Segment.Duration / EncounterDungeonLayout::MaxWalkSpeed);
    }
    StopTravel();
    TravelSegments = MoveTemp(Segments);
    Camera->SetRelativeLocation(Path[0]);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    BeginTravel(EncounterDungeonLayout::ChoiceFieldOfView, PresentedStage->Camera->FieldOfView);
    return true;
}

bool AEncounterDungeonRoute::StartCombatTravel(int32 Direction)
{
    if (bTraveling || GetNetMode() == NM_DedicatedServer || Direction < 0 || Direction >= 3 || !bGeometryPrepared) return false;
    TArray<FVector> Path = EncounterDungeonLayout::GetPath(Direction, LayoutVariant);
    if (Path.Num() < 2) return false;
    const FVector Forward = EncounterDungeonLayout::GetBranchRotation(Direction).GetForwardVector();
    const FVector EntranceApproach = EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, false) - Forward * EncounterDungeonLayout::EntranceDoorApproachDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight);
    const FVector RearApproach = EncounterDungeonLayout::GetDoorCenter(Direction, LayoutVariant, true) - Forward * 220.f + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight);
    Path.Insert(EntranceApproach, Path.Num() - 1);
    const FVector ChoiceApproach = EncounterDungeonLayout::GetChoiceDoorCenter(Direction) - EncounterDungeonLayout::GetChoiceDoorRotation(Direction).GetForwardVector() * EncounterDungeonLayout::EntranceDoorApproachDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight);
    Path.Add(RearApproach);
    Path.Add(EncounterDungeonLayout::GetLoopDoorCenter(Direction, LayoutVariant) - Forward * EncounterDungeonLayout::DoorCoverDistance + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight));
    TArray<FEncounterDungeonTravelSegment> Segments;
    FQuat Rotation = FQuat::Identity;
    float Distance = 0.f;
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const FVector Delta = Path[Index] - Path[Index - 1];
        if (Delta.ContainsNaN() || Delta.IsNearlyZero() || !FMath::IsNearlyZero(Delta.Z)) return false;
        const FQuat Facing = Delta.Rotation().Quaternion();
        if (!Rotation.Equals(Facing, 0.001f)) Segments.Add({Path[Index - 1], Path[Index - 1], Rotation, Facing, EncounterDungeonLayout::TurnDuration});
        Segments.Add({Path[Index - 1], Path[Index], Facing, Facing, static_cast<float>(Delta.Size())});
        Rotation = Facing;
        if (Path[Index].Equals(ChoiceApproach))
        {
            const FQuat DoorRotation = EncounterDungeonLayout::GetChoiceDoorRotation(Direction);
            if (!Rotation.Equals(DoorRotation, 0.001f)) Segments.Add({ChoiceApproach, ChoiceApproach, Rotation, DoorRotation, EncounterDungeonLayout::TurnDuration});
            Segments.Add({ChoiceApproach, ChoiceApproach, DoorRotation, DoorRotation, EncounterDungeonLayout::DoorOpenDuration, EncounterDungeonLayout::ChoiceEntranceDoorIndex + Direction});
            Rotation = DoorRotation;
        }
        if (Path[Index].Equals(EntranceApproach)) Segments.Add({Path[Index], Path[Index], Facing, Facing, EncounterDungeonLayout::DoorOpenDuration, Direction * EncounterDungeonLayout::DoorsPerRoom});
        if (Path[Index].Equals(RearApproach)) Segments.Add({Path[Index], Path[Index], Facing, Facing, EncounterDungeonLayout::DoorOpenDuration, Direction * EncounterDungeonLayout::DoorsPerRoom + 1});
        Distance += static_cast<float>(Delta.Size());
    }
    for (FEncounterDungeonTravelSegment& Segment : Segments)
    {
        if (Segment.DoorIndex == INDEX_NONE && !Segment.Start.Equals(Segment.End)) Segment.Duration = FMath::Max(EncounterDungeonLayout::CombatWalkDuration * Segment.Duration / Distance, Segment.Duration / EncounterDungeonLayout::MaxWalkSpeed);
    }
    StopTravel();
    DestroyPresentedStage();
    TravelSegments = MoveTemp(Segments);
    Camera->SetRelativeLocation(Path[0]);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    bCombatEntering = true;
    BeginTravel(EncounterDungeonLayout::ChoiceFieldOfView, 80.f);
    return true;
}

bool AEncounterDungeonRoute::IsTraveling() const
{
    return bTraveling;
}

bool AEncounterDungeonRoute::StartExitTravel()
{
    if (bTraveling || GetNetMode() == NM_DedicatedServer || !IsValid(PresentedStage) || !PresentedStage->Camera || StageDirection == INDEX_NONE) return false;
    const TArray<FVector> Path = EncounterDungeonLayout::GetExitPath(StageDirection, LayoutVariant);
    if (Path.Num() < 2) return false;
    StopTravel();
    Camera->SetWorldLocationAndRotation(PresentedStage->Camera->GetComponentLocation(), PresentedStage->Camera->GetComponentQuat());
    FQuat CurrentRotation = Camera->GetRelativeRotation().Quaternion();
    for (int32 Index = 1; Index < Path.Num(); ++Index)
    {
        const FQuat Facing = (Path[Index] - Path[Index - 1]).Rotation().Quaternion();
        if (!CurrentRotation.Equals(Facing, 0.001f)) TravelSegments.Add({Path[Index - 1], Path[Index - 1], CurrentRotation, Facing, EncounterDungeonLayout::TurnDuration});
        if (Index == 2) TravelSegments.Add({Path[Index - 1], Path[Index - 1], Facing, Facing, EncounterDungeonLayout::DoorOpenDuration, StageDirection * EncounterDungeonLayout::DoorsPerRoom + 1});
        TravelSegments.Add({Path[Index - 1], Path[Index], Facing, Facing, static_cast<float>(FVector::Distance(Path[Index - 1], Path[Index])) / EncounterDungeonLayout::ExitWalkSpeed});
        CurrentRotation = Facing;
    }
    bExiting = true;
    BeginTravel(80.f, 80.f);
    return true;
}

bool AEncounterDungeonRoute::StartLoopEntrance()
{
    if (bTraveling || GetNetMode() == NM_DedicatedServer || !DoorCenters.IsValidIndex(EncounterDungeonLayout::LoopEntranceDoorIndex)) return false;
    StopTravel();
    const FVector Start = DoorCenters[EncounterDungeonLayout::LoopEntranceDoorIndex] + FVector(-EncounterDungeonLayout::DoorCoverDistance, 0.f, EncounterDungeonLayout::EyeHeight);
    const FVector End(0.f, 0.f, EncounterDungeonLayout::EyeHeight);
    Camera->SetRelativeLocation(Start);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    SetDoorOpen(EncounterDungeonLayout::LoopEntranceDoorIndex, 0.f);
    TravelSegments = {{Start, Start, FQuat::Identity, FQuat::Identity, EncounterDungeonLayout::DoorOpenDuration, EncounterDungeonLayout::LoopEntranceDoorIndex}, {Start, End, FQuat::Identity, FQuat::Identity, EncounterDungeonLayout::LoopEntryWalkDuration}};
    bLoopEntering = true;
    BeginTravel(80.f, EncounterDungeonLayout::ChoiceFieldOfView);
    return true;
}

AEncounterPrototypeStage* AEncounterDungeonRoute::GetPresentedStage() const
{
    return IsValid(PresentedStage) ? PresentedStage.Get() : nullptr;
}

void AEncounterDungeonRoute::StopTravel()
{
    bTraveling = false;
    bExiting = false;
    bLoopEntering = false;
    bCombatEntering = false;
    SetActorTickEnabled(false);
    TravelSegments.Reset();
    TravelSegmentIndex = 0;
    SegmentSeconds = 0.f;
    TravelSeconds = 0.f;
    TravelElapsedSeconds = 0.f;
}

void AEncounterDungeonRoute::BeginTravel(float InitialFieldOfView, float FinalFieldOfView)
{
    TravelInitialFieldOfView = InitialFieldOfView;
    TravelFinalFieldOfView = FinalFieldOfView;
    TravelSeconds = 0.f;
    TravelElapsedSeconds = 0.f;
    for (const FEncounterDungeonTravelSegment& Segment : TravelSegments) TravelSeconds += Segment.Duration;
    Camera->SetFieldOfView(InitialFieldOfView);
    bTraveling = true;
    SetActorTickEnabled(true);
}

void AEncounterDungeonRoute::FinishAtStage()
{
    StopTravel();
    if (!IsValid(PresentedStage) || !PresentedStage->Camera) return;
    for (int32 Index = 0; Index < EncounterDungeonLayout::DoorsPerRoom; ++Index) SetDoorOpen(StageDirection * EncounterDungeonLayout::DoorsPerRoom + Index, 0.f);
    Camera->SetWorldLocationAndRotation(PresentedStage->Camera->GetComponentLocation(), PresentedStage->Camera->GetComponentQuat());
    Camera->SetFieldOfView(PresentedStage->Camera->FieldOfView);
}

void AEncounterDungeonRoute::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bTraveling || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
    if (!bLoopEntering && !bCombatEntering && !IsValid(PresentedStage))
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
        TravelElapsedSeconds += Step;
        RemainingSeconds -= Step;
        const float Alpha = FMath::Clamp(SegmentSeconds / Segment.Duration, 0.f, 1.f);
        const float SmoothedAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
        if (Segment.DoorIndex != INDEX_NONE) SetDoorOpen(Segment.DoorIndex, SmoothedAlpha);
        Camera->SetRelativeLocation(FMath::Lerp(Segment.Start, Segment.End, SmoothedAlpha));
        Camera->SetRelativeRotation(FQuat::Slerp(Segment.StartRotation, Segment.EndRotation, SmoothedAlpha));
        const float ViewAlpha = FMath::Clamp(TravelElapsedSeconds / TravelSeconds, 0.f, 1.f);
        Camera->SetFieldOfView(FMath::Lerp(TravelInitialFieldOfView, TravelFinalFieldOfView, ViewAlpha * ViewAlpha * (3.f - 2.f * ViewAlpha)));
        if (SegmentSeconds < Segment.Duration) break;
        ++TravelSegmentIndex;
        SegmentSeconds = 0.f;
        if (RemainingSeconds <= 0.f) break;
    }
    if (!TravelSegments.IsValidIndex(TravelSegmentIndex))
    {
        if (bLoopEntering)
        {
            StopTravel();
            SetDoorOpen(EncounterDungeonLayout::LoopEntranceDoorIndex, 0.f);
        }
        else if (bExiting || bCombatEntering) StopTravel();
        else FinishAtStage();
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
