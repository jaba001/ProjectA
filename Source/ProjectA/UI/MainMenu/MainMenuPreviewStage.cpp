#include "UI/MainMenu/MainMenuPreviewStage.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AMainMenuPreviewStage::AMainMenuPreviewStage()
{
    PrimaryActorTick.bCanEverTick = false;
    SpawnedPreviewActors.SetNum(4);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
    PreviewCamera->SetupAttachment(SceneRoot);
    PreviewCamera->SetRelativeLocation(FVector(-700.0f, 0.0f, 140.0f));
    PreviewCamera->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));

    Slot0Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot0Anchor"));
    Slot0Anchor->SetupAttachment(SceneRoot);
    Slot0Anchor->SetRelativeLocation(FVector(0.0f, -225.0f, 0.0f));

    Slot1Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot1Anchor"));
    Slot1Anchor->SetupAttachment(SceneRoot);
    Slot1Anchor->SetRelativeLocation(FVector(0.0f, -75.0f, 0.0f));

    Slot2Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot2Anchor"));
    Slot2Anchor->SetupAttachment(SceneRoot);
    Slot2Anchor->SetRelativeLocation(FVector(0.0f, 75.0f, 0.0f));

    Slot3Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot3Anchor"));
    Slot3Anchor->SetupAttachment(SceneRoot);
    Slot3Anchor->SetRelativeLocation(FVector(0.0f, 225.0f, 0.0f));
}

void AMainMenuPreviewStage::SetPreviewActorForSlot(int32 SlotIndex, FName ClassId)
{
    USceneComponent* SlotAnchor = GetSlotAnchor(SlotIndex);
    if (!SlotAnchor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPreviewStage] Invalid preview slot index: %d"), SlotIndex);
        return;
    }

    ClearPreviewActorForSlot(SlotIndex);

    const TSubclassOf<AActor>* PreviewActorClass = PreviewActorClasses.Find(ClassId);
    if (!PreviewActorClass || !PreviewActorClass->Get())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPreviewStage] Preview actor class is not configured for ClassId: %s"), *ClassId.ToString());
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPreviewStage] World is not available."));
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* PreviewActor = World->SpawnActor<AActor>(PreviewActorClass->Get(), SlotAnchor->GetComponentTransform(), SpawnParameters);
    if (!PreviewActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPreviewStage] Failed to spawn preview actor for slot %d."), SlotIndex);
        return;
    }

    PreviewActor->AttachToComponent(SlotAnchor, FAttachmentTransformRules::KeepWorldTransform);
    SpawnedPreviewActors[SlotIndex] = PreviewActor;
    UE_LOG(LogTemp, Log, TEXT("[MainMenuPreviewStage] Preview actor updated. SlotIndex: %d, ClassId: %s"), SlotIndex, *ClassId.ToString());
}

void AMainMenuPreviewStage::ClearPreviewActorForSlot(int32 SlotIndex)
{
    if (!SpawnedPreviewActors.IsValidIndex(SlotIndex))
    {
        return;
    }

    AActor* PreviewActor = SpawnedPreviewActors[SlotIndex];
    if (PreviewActor)
    {
        PreviewActor->Destroy();
        SpawnedPreviewActors[SlotIndex] = nullptr;
    }
}

USceneComponent* AMainMenuPreviewStage::GetSlotAnchor(int32 SlotIndex) const
{
    switch (SlotIndex)
    {
    case 0:
        return Slot0Anchor;
    case 1:
        return Slot1Anchor;
    case 2:
        return Slot2Anchor;
    case 3:
        return Slot3Anchor;
    default:
        return nullptr;
    }
}
