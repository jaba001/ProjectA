#include "Game/Encounter/EncounterDungeonRoute.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Encounter/EncounterMazeLayout.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProjectA.h"
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
    LeftDoors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftDoors"));
    RightDoors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightDoors"));
    DoorSeals = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorSeals"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Cube.Object;
    TorchFlame = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Fantastic_Dungeon_Pack/effects/PS_FX_fire_dungeon_01_Niagara.PS_FX_fire_dungeon_01_Niagara")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> LeftDoor(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/modular/gateway/MOD_Gateway_Door_01_large_door_L.MOD_Gateway_Door_01_large_door_L"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RightDoor(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/modular/gateway/MOD_Gateway_Door_01_large_door_R.MOD_Gateway_Door_01_large_door_R"));
    DoorSeals->SetupAttachment(RouteRoot);
    DoorSeals->SetStaticMesh(CubeMesh);
    DoorSeals->SetMobility(EComponentMobility::Movable);
    DoorSeals->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DoorSeals->SetGenerateOverlapEvents(false);
    DoorSeals->SetCanEverAffectNavigation(false);
    DoorSeals->SetCastShadow(true);
    DoorSeals->SetLightingChannels(false, true, false);
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
    // The baked maze owns the stone frame; the route keeps only the animated leaves in the same order.
    // 구운 미로가 석재 문틀을 소유하며 통로 액터는 같은 순서의 움직이는 문짝만 유지합니다.
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

void AEncounterDungeonRoute::AddTorchEffects(const FEncounterMazeTorchMount& Mount)
{
    // Spawn only the local light and flame for a sconce authored in the baked maze.
    // 구운 미로에 배치된 벽 횃불의 로컬 조명과 불꽃만 생성합니다.
    const FVector Center = Mount.Center;
    const FVector FlamePosition = AEncounterMazeLayout::GetFlamePosition(Mount);
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
    // Saved IDs share the same geometry; reset doors and travel without rebuilding authored scenery.
    // 저장 번호는 같은 지형을 공유하므로 작성 장식을 재생성하지 않고 문과 이동만 초기화합니다.
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
    UInstancedStaticMeshComponent* Shapes[] = {LeftDoors, RightDoors, DoorSeals};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->SetVisibility(bVisible);
    if (IsValid(MazeLayout)) MazeLayout->SetPresentationVisible(bVisible);
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
    UInstancedStaticMeshComponent* Shapes[] = {LeftDoors, RightDoors, DoorSeals};
    for (UInstancedStaticMeshComponent* Shape : Shapes) Shape->ClearInstances();
    if (IsValid(MazeLayout) && MazeLayout->GetOwner() == this) MazeLayout->Destroy();
    MazeLayout = nullptr;
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

bool AEncounterDungeonRoute::SpawnMazeLayout()
{
    // Every saved variant uses the same authored scenery and matching route coordinates.
    // 모든 저장 변형은 동일한 작성 장식과 일치하는 이동 좌표를 사용합니다.
    const FString Name = EncounterDungeonLayout::BakedMazeName;
    const FSoftClassPath BakedPath(FString::Printf(TEXT("%s/%s.%s_C"), EncounterDungeonLayout::BakedMazeFolder, *Name, *Name));
    UClass* LayoutClass = BakedPath.TryLoadClass<AEncounterMazeLayout>();
    const AEncounterMazeLayout* Defaults = LayoutClass ? Cast<AEncounterMazeLayout>(LayoutClass->GetDefaultObject()) : nullptr;
    if (!Defaults || Defaults->LayoutVariant != EncounterDungeonLayout::SharedLayoutVariant)
    {
        UE_LOG(LogProjectA, Error, TEXT("[EncounterMaze] Shared baked layout %s is missing or mismatched; prepare the V7 Blueprint in the editor."), *Name);
        return false;
    }
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.ObjectFlags |= RF_Transient;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    MazeLayout = GetWorld()->SpawnActor<AEncounterMazeLayout>(LayoutClass, GetActorTransform(), Parameters);
    if (!MazeLayout) return false;
    MazeLayout->AttachToComponent(RouteRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    MazeLayout->SetPresentationVisible(bPresentationVisible);
    return true;
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
    if (bGeometryPrepared && Material && RouteMaterial != Material)
    {
        RouteMaterial = Material;
        if (IsValid(MazeLayout)) ApplyMaterial(MazeLayout->Mortar, FLinearColor(0.055f, 0.05f, 0.04f));
    }
    if (bGeometryPrepared) return;
    if (Material) RouteMaterial = Material;
    if (!SpawnMazeLayout()) return;
    ApplyMaterial(MazeLayout->Mortar, FLinearColor(0.055f, 0.05f, 0.04f));
    for (const FEncounterMazeTorchMount& Mount : MazeLayout->TorchMounts) AddTorchEffects(Mount);
    const float CellSize = EncounterDungeonLayout::CellSize;
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
