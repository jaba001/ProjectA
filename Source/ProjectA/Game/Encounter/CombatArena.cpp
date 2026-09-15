#include "Game/Encounter/CombatArena.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Engine/World.h"

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
        }
        Controller->SetViewTargetWithBlend(ViewTarget, 0.f);
    }
}

void ACombatArena::CleanupArena()
{
    if (IsValid(Grid))
    {
        Grid->ClearOccupancy();
        Grid->SetGridActive(false);
    }
}
