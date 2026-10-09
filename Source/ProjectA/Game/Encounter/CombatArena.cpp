#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace CombatArenaEnvironmentInternal
{
    const FName VisualOwnerTag(TEXT("ProjectAUnifiedGameplayVisual"));
    const FName CentralRegionTag(TEXT("PineRidge"));

    struct FRenderBatch
    {
        UStaticMeshComponent* Source = nullptr;
        TArray<FTransform> Transforms;
        TArray<int32> SourceIndices;
    };
}

ACombatArena::ACombatArena()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ArenaOrigin")));
    PlayerCoords = {FIntPoint(0, 1), FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(3, 1)};
    EnemyCoords = {FIntPoint(0, 2), FIntPoint(1, 2), FIntPoint(2, 2), FIntPoint(3, 2)};
    CameraTransform = FTransform(FRotator(-65.f, 0.f, 0.f), FVector(-1200.f, 400.f, 1800.f));
}

void ACombatArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatArena, EnvironmentId);
}

void ACombatArena::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) OnRep_EnvironmentId();
}

void ACombatArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetEnvironmentVisuals();
    Super::EndPlay(EndPlayReason);
}

void ACombatArena::OnRep_EnvironmentId()
{
    // Replication can precede BeginPlay; the latter retries after the placed scene is initialized.
    // 복제가 BeginPlay보다 빠르면 배치된 장면 초기화 후 BeginPlay에서 다시 적용합니다.
    if (!HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer) return;
    FText Error;
    if (!ApplyEnvironmentVisuals(EnvironmentId, Error)) UE_LOG(LogTemp, Warning, TEXT("[CombatArena] Environment %s could not be presented: %s"), *EnvironmentId.ToString(), *Error.ToString());
}

bool ACombatArena::ApplyEnvironment(FName ArenaId, FText& OutError)
{
    OutError = NSLOCTEXT("CombatArenaEnvironment", "InvalidEnvironment", "선택한 전투 환경을 적용할 수 없습니다. 기존 전장은 유지됩니다.");
    if (!HasAuthority() || (!ArenaId.IsNone() && !CombatArenaEnvironment::Find(ArenaId))) return false;
    if (GetNetMode() != NM_DedicatedServer && !ApplyEnvironmentVisuals(ArenaId, OutError)) return false;
    EnvironmentId = ArenaId;
    ForceNetUpdate();
    OutError = FText::GetEmpty();
    return true;
}

void ACombatArena::ResetEnvironment()
{
    ResetEnvironmentVisuals();
    if (HasAuthority())
    {
        EnvironmentId = NAME_None;
        ForceNetUpdate();
    }
}

void ACombatArena::ResetEnvironmentVisuals()
{
    if (IsValid(EnvironmentVisual)) EnvironmentVisual->Destroy();
    EnvironmentVisual = nullptr;
    AppliedEnvironmentId = NAME_None;
    for (const TPair<TWeakObjectPtr<AActor>, bool>& Entry : OriginalEnvironmentVisibility)
    {
        if (AActor* Actor = Entry.Key.Get()) Actor->SetActorHiddenInGame(Entry.Value);
    }
    OriginalEnvironmentVisibility.Reset();
}

bool ACombatArena::ApplyEnvironmentVisuals(FName ArenaId, FText& OutError)
{
    if (ArenaId.IsNone())
    {
        ResetEnvironmentVisuals();
        OutError = FText::GetEmpty();
        return true;
    }
    if (AppliedEnvironmentId == ArenaId && IsValid(EnvironmentVisual))
    {
        OutError = FText::GetEmpty();
        return true;
    }
    const FCombatArenaEnvironmentProfile* Profile = CombatArenaEnvironment::Find(ArenaId);
    UWorld* World = GetWorld();
    OutError = NSLOCTEXT("CombatArenaEnvironment", "MissingRegion", "통합 Gameplay에서 선택한 지역 장식과 중앙 전장을 찾을 수 없습니다.");
    if (!Profile || !World) return false;
    FVector CameraPosition = IsValid(CameraAnchor) ? CameraAnchor->GetActorLocation() : (CameraTransform * GetActorTransform()).GetLocation();
    if (const ACameraActor* CameraActor = Cast<ACameraActor>(CameraAnchor)) CameraPosition = CameraActor->GetCameraComponent()->GetComponentLocation();

    // The native region templates share this unchanged 4x4 combat core and physical floor.
    // 기본 지역 템플릿은 변경하지 않는 동일한 4x4 전투 코어와 물리 바닥을 공유합니다.
    const FBox CombatClearance(FVector(-790.f, -190.f, 3.f), FVector(190.f, 990.f, 600.f));
    TArray<FVector> CombatSamples;
    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Column = 0; Column < 4; ++Column)
        {
            for (float Height : {5.f, 150.f, 300.f}) CombatSamples.Add(FVector(-Row * 200.f, Column * 200.f + (Column >= 2 ? 200.f : 0.f), Height));
        }
    }
    TArray<AActor*> CentralActors;
    TArray<CombatArenaEnvironmentInternal::FRenderBatch> Batches;
    int32 GroundCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor->ActorHasTag(CombatArenaEnvironmentInternal::VisualOwnerTag)) continue;
        if (Actor->ActorHasTag(CombatArenaEnvironmentInternal::CentralRegionTag)) CentralActors.Add(Actor);
        if (!Actor->ActorHasTag(Profile->SourceRegionTag)) continue;
        TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
        for (UStaticMeshComponent* Source : Components)
        {
            if (!IsValid(Source) || !Source->GetStaticMesh() || !Source->IsVisible()) continue;
            CombatArenaEnvironmentInternal::FRenderBatch Batch;
            Batch.Source = Source;
            const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Source);
            const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
            if (Instances && (Instances->NumCustomDataFloats < 0 || static_cast<int64>(Instances->NumCustomDataFloats) * Count != Instances->PerInstanceSMCustomData.Num())) return false;
            for (int32 Index = 0; Index < Count; ++Index)
            {
                FTransform Transform = Source->GetComponentTransform();
                if (Instances && !Instances->GetInstanceTransform(Index, Transform, true)) return false;
                Transform = CombatArenaEnvironment::RebaseTransform(Transform, *Profile);
                if (Transform.ContainsNaN()) return false;
                const FBox Bounds = Source->GetStaticMesh()->GetBoundingBox().TransformBy(Transform);
                if (!CombatArenaEnvironment::ShouldKeepBounds(Bounds, CombatClearance, CameraPosition, CombatSamples)) continue;
                GroundCount += CombatArenaEnvironment::IsGroundBounds(Bounds) ? 1 : 0;
                Batch.Transforms.Add(Transform);
                Batch.SourceIndices.Add(Index);
            }
            if (!Batch.Transforms.IsEmpty()) Batches.Add(MoveTemp(Batch));
        }
    }
    if (CentralActors.IsEmpty() || Batches.IsEmpty() || GroundCount == 0) return false;

    // Prepare an invisible local candidate, then publish only after every component succeeds.
    // 모든 컴포넌트 작성이 성공한 뒤에만 공개하도록 보이지 않는 로컬 후보를 준비합니다.
    OutError = NSLOCTEXT("CombatArenaEnvironment", "CreateVisual", "선택한 전투 환경의 표시를 준비하지 못했습니다. 기존 전장은 유지됩니다.");
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Candidate = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
    if (!Candidate) return false;
    Candidate->SetReplicates(false);
    Candidate->SetActorEnableCollision(false);
    Candidate->SetActorHiddenInGame(true);
    USceneComponent* Root = NewObject<USceneComponent>(Candidate, TEXT("EnvironmentRoot"), RF_Transient);
    Candidate->SetRootComponent(Root);
    Candidate->AddInstanceComponent(Root);
    Root->SetMobility(EComponentMobility::Movable);
    Root->RegisterComponent();
    const auto FailCandidate = [Candidate]()
    {
        Candidate->Destroy();
        return false;
    };
    if (!Root->IsRegistered()) return FailCandidate();
    for (const CombatArenaEnvironmentInternal::FRenderBatch& Batch : Batches)
    {
        UStaticMeshComponent* Source = Batch.Source;
        UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Candidate, Source->GetClass(), NAME_None, RF_Transient);
        if (!Component) return FailCandidate();
        Candidate->AddInstanceComponent(Component);
        Component->SetupAttachment(Root);
        Component->SetMobility(EComponentMobility::Movable);
        if (!Component->SetStaticMesh(Source->GetStaticMesh()) && Component->GetStaticMesh() != Source->GetStaticMesh()) return FailCandidate();
        for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index) Component->SetMaterial(Index, Source->GetMaterial(Index));
        Component->SetCastShadow(Source->CastShadow);
        Component->SetReceivesDecals(Source->bReceivesDecals);
        Component->SetAffectDistanceFieldLighting(Source->bAffectDistanceFieldLighting);
        Component->SetVisibleInRayTracing(Source->bVisibleInRayTracing);
        Component->SetRenderInMainPass(Source->bRenderInMainPass);
        Component->SetRenderInDepthPass(Source->bRenderInDepthPass);
        Component->SetRenderCustomDepth(Source->bRenderCustomDepth);
        Component->SetCustomDepthStencilValue(Source->CustomDepthStencilValue);
        Component->SetCustomDepthStencilWriteMask(Source->CustomDepthStencilWriteMask);
        Component->SetForcedLodModel(Source->GetForcedLodModel());
        Component->SetHiddenInGame(Source->bHiddenInGame);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetComponentTickEnabled(false);
        Component->SetCullDistance(18000.f);
        Component->RegisterComponent();
        if (!Component->IsRegistered()) return FailCandidate();
        if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
        {
            // Keep ISM and ordinary component types intact; unsupported source materials never gain ISM usage.
            // ISM과 일반 컴포넌트 종류를 유지하며 미지원 원본 재질에 ISM 사용을 강제하지 않습니다.
            const UInstancedStaticMeshComponent* Original = CastChecked<UInstancedStaticMeshComponent>(Source);
            Instances->SetNumCustomDataFloats(Original->NumCustomDataFloats);
            Instances->SetCullDistances(15000, 18000);
            for (int32 Index = 0; Index < Batch.Transforms.Num(); ++Index)
            {
                if (Instances->AddInstance(Batch.Transforms[Index], true) != Index) return FailCandidate();
                if (Original->NumCustomDataFloats > 0)
                {
                    const int32 Offset = Batch.SourceIndices[Index] * Original->NumCustomDataFloats;
                    if (!Instances->SetCustomData(Index, TArrayView<const float>(Original->PerInstanceSMCustomData.GetData() + Offset, Original->NumCustomDataFloats), true)) return FailCandidate();
                }
            }
        }
        else
        {
            Component->SetWorldTransform(Batch.Transforms[0]);
        }
    }
    if (IsValid(EnvironmentVisual)) EnvironmentVisual->Destroy();
    for (AActor* Actor : CentralActors)
    {
        const TWeakObjectPtr<AActor> Key(Actor);
        if (!OriginalEnvironmentVisibility.Contains(Key)) OriginalEnvironmentVisibility.Add(Key, Actor->IsHidden());
        Actor->SetActorHiddenInGame(true);
    }
    EnvironmentVisual = Candidate;
    AppliedEnvironmentId = ArenaId;
    Candidate->SetActorHiddenInGame(false);
    OutError = FText::GetEmpty();
    return true;
}

bool ACombatArena::PrepareArena(FText& OutError)
{
    if (!IsValid(Grid))
    {
        OutError = FText::FromString(TEXT("Arena Grid is not configured. / 아레나 Grid를 지정하세요."));
        return false;
    }
    if (Grid->TileMap.IsEmpty())
    {
        Grid->GenerateGrid();
    }
    if (Grid->TileMap.IsEmpty())
    {
        OutError = FText::FromString(TEXT("Grid generation failed. Check TileClass. / TileClass를 확인하세요."));
        return false;
    }
    Grid->ClearOccupancy();
    return true;
}

void ACombatArena::ActivateArena(APlayerController* Controller)
{
    if (Grid)
    {
        Grid->SetGridActive(true);
    }
    if (!Controller)
    {
        return;
    }
    AActor* ViewTarget = CameraAnchor;
    if (!IsValid(ViewTarget))
    {
        if (!IsValid(RuntimeCamera))
        {
            RuntimeCamera = GetWorld()->SpawnActor<ACameraActor>();
        }
        ViewTarget = RuntimeCamera;
        if (ViewTarget)
        {
            ViewTarget->SetActorTransform(CameraTransform * GetActorTransform());
        }
    }
    if (ViewTarget)
    {
        UCameraComponent* Camera = nullptr;
        if (ACameraActor* CameraActor = Cast<ACameraActor>(ViewTarget))
        {
            Camera = CameraActor->GetCameraComponent();
        }
        else
        {
            TInlineComponentArray<UCameraComponent*> Cameras(ViewTarget);
            for (UCameraComponent* Candidate : Cameras)
            {
                if (!Candidate->IsActive()) continue;
                Camera = Candidate;
                break;
            }
        }
        if (Camera)
        {
            // Fill the viewport while preserving vertical framing and expanding the view on wider screens.
            // 세로 구도를 유지하고 넓은 화면에서는 좌우 시야를 확장해 뷰포트를 채웁니다.
            Camera->SetConstraintAspectRatio(false);
            Camera->bOverrideAspectRatioAxisConstraint = true;
            Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);
            if (!Camera->PostProcessSettings.bOverride_ColorSaturation)
            {
                // Apply a stable default without compounding it on later visits or changing exposure.
                // 반복 방문 때 누적하거나 노출을 바꾸지 않고 일정한 기본값을 적용합니다.
                Camera->PostProcessSettings.bOverride_ColorSaturation = true;
                Camera->PostProcessSettings.ColorSaturation = FVector4(1.f, 1.f, 1.f, FMath::Clamp(DefaultSceneSaturation, 0.f, 1.f));
            }
        }
        Controller->SetViewTargetWithBlend(ViewTarget, 0.f);
    }
}

void ACombatArena::CleanupArena()
{
    ResetEnvironment();
    if (IsValid(Grid))
    {
        Grid->ClearOccupancy();
        Grid->SetGridActive(false);
    }
}
