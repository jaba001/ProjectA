#include "Game/Encounter/CombatArena.h"
#include "ProjectA.h"
#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace CombatArenaEnvironmentInternal
{
    const FName VisualOwnerTag(TEXT("ProjectAUnifiedGameplayVisual"));
    const FName CentralRegionTag(TEXT("PineRidge"));
}

ACombatArena::ACombatArena()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ArenaOrigin")));
    for (const FCombatArenaEnvironmentProfile& Profile : CombatArenaEnvironment::GetProfiles())
    {
        const FString Path = FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/Environment/BP_CombatArena_%s.BP_CombatArena_%s_C"), *Profile.ArenaId.ToString(), *Profile.ArenaId.ToString());
        EnvironmentTemplates.Add(Profile.ArenaId, TSoftClassPtr<AActor>(FSoftObjectPath(Path)));
    }
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
    if (!ApplyEnvironmentVisuals(EnvironmentId, Error)) UE_LOG(LogProjectA, Warning, TEXT("[CombatArena] Environment %s could not be presented: %s"), *EnvironmentId.ToString(), *Error.ToString());
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
    const TSoftClassPtr<AActor>* Template = EnvironmentTemplates.Find(ArenaId);
    UClass* VisualClass = Template ? Template->LoadSynchronous() : nullptr;
    if (!VisualClass)
    {
        OutError = NSLOCTEXT("CombatArenaEnvironment", "MissingBakedEnvironment", "구운 전투장 Blueprint가 없습니다. Arena의 EnvironmentTemplates와 에디터 굽기 결과를 확인하세요.");
        UE_LOG(LogProjectA, Warning, TEXT("[CombatArena] Missing baked environment %s / 구운 전투장 누락"), *ArenaId.ToString());
        return false;
    }
    TArray<AActor*> CentralActors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(CombatArenaEnvironmentInternal::VisualOwnerTag) && It->ActorHasTag(CombatArenaEnvironmentInternal::CentralRegionTag)) CentralActors.Add(*It);
    }
    if (CentralActors.IsEmpty()) return false;
    for (AActor* Actor : EnvironmentOccluders)
    {
        if (IsValid(Actor)) CentralActors.AddUnique(Actor);
    }
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Candidate = World->SpawnActor<AActor>(VisualClass, FTransform::Identity, Params);
    if (!Candidate) return false;
    Candidate->SetReplicates(false);
    Candidate->SetActorEnableCollision(false);
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
        OutError = NSLOCTEXT("CombatArenaText", "M_8D9368AD", "Arena Grid is not configured. / 아레나 Grid를 지정하세요.");
        return false;
    }
    if (Grid->TileMap.IsEmpty())
    {
        Grid->GenerateGrid();
    }
    if (Grid->TileMap.IsEmpty())
    {
        OutError = NSLOCTEXT("CombatArenaText", "M_E2B12843", "Grid generation failed. Check TileClass. / TileClass를 확인하세요.");
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
