#include "Game/Encounter/CombatArena.h"
#include "Camera/CameraActor.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Engine/World.h"

ACombatArena::ACombatArena()
{
    PrimaryActorTick.bCanEverTick = false;
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
