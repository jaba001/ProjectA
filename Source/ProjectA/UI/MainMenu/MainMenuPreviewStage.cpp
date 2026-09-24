#include "UI/MainMenu/MainMenuPreviewStage.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Unit/CharacterAppearanceComponent.h"

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
    RefreshPreviewFocus();
    UE_LOG(LogTemp, Log, TEXT("[MainMenuPreviewStage] Preview actor updated. SlotIndex: %d, ClassId: %s"), SlotIndex, *ClassId.ToString());
}

void AMainMenuPreviewStage::ClearPreviewActorForSlot(int32 SlotIndex)
{
    if (!SpawnedPreviewActors.IsValidIndex(SlotIndex))
    {
        return;
    }

    AActor* PreviewActor = SpawnedPreviewActors[SlotIndex];
    if (IsValid(PreviewActor))
    {
        PreviewActor->Destroy();
    }
    SpawnedPreviewActors[SlotIndex] = nullptr;
}

void AMainMenuPreviewStage::ClearAllPreviewActors()
{
    ClearPreviewFocus();
    for (int32 Index = 0; Index < SpawnedPreviewActors.Num(); ++Index)
    {
        ClearPreviewActorForSlot(Index);
    }
}

bool AMainMenuPreviewStage::SetPreviewAppearance(int32 SlotIndex, UCharacterAppearanceCatalog* Catalog, const FCharacterAppearanceSelection& Selection)
{
    AActor* PreviewActor = GetPreviewActorForSlot(SlotIndex);
    if (!PreviewActor) return false;
    UCharacterAppearanceComponent* Appearance = PreviewActor->FindComponentByClass<UCharacterAppearanceComponent>();
    if (!Appearance && Catalog)
    {
        Appearance = NewObject<UCharacterAppearanceComponent>(PreviewActor);
        PreviewActor->AddInstanceComponent(Appearance);
        Appearance->RegisterComponent();
    }
    return Appearance ? Appearance->SetAppearance(Catalog, Selection) : !Catalog && Selection.ItemIds.IsEmpty();
}

void AMainMenuPreviewStage::SetFocusedPreviewSlot(int32 SlotIndex)
{
    if (!GetPreviewActorForSlot(SlotIndex)) return;
    if (FocusedSlot == INDEX_NONE)
    {
        UnfocusedCameraTransform = PreviewCamera->GetComponentTransform();
        UnfocusedFieldOfView = PreviewCamera->FieldOfView;
    }
    FocusedSlot = SlotIndex;
    FocusedYawOffset = 0.0f;
    RefreshPreviewFocus();
}

void AMainMenuPreviewStage::RefreshPreviewFocus()
{
    AActor* FocusedActor = GetPreviewActorForSlot(FocusedSlot);
    if (!FocusedActor) return;
    for (int32 Index = 0; Index < SpawnedPreviewActors.Num(); ++Index)
    {
        if (AActor* Actor = GetPreviewActorForSlot(Index)) Actor->SetActorHiddenInGame(Index != FocusedSlot);
    }
    USceneComponent* Anchor = GetSlotAnchor(FocusedSlot);
    FocusedActor->SetActorRotation(Anchor->GetComponentRotation() + FRotator(0.0f, FocusedYawOffset, 0.0f));
    const USkeletalMeshComponent* Mesh = FocusedActor->FindComponentByClass<USkeletalMeshComponent>();
    const FVector Center = Mesh ? Mesh->Bounds.Origin : FocusedActor->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
    const float HalfHeight = Mesh ? FMath::Max(90.0f, Mesh->Bounds.BoxExtent.Z) : 100.0f;
    int32 Width = 1920;
    int32 Height = 1080;
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController()) Controller->GetViewportSize(Width, Height);
    const float Aspect = Height > 0 && Width > 0 ? static_cast<float>(Width) / Height : 16.0f / 9.0f;
    const FRotator Rotation(0.0f, UnfocusedCameraTransform.Rotator().Yaw, 0.0f);
    const FVector Forward = Rotation.Vector();
    const FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
    const float HorizontalTangent = FMath::Tan(FMath::DegreesToRadians(22.5f));
    const float Distance = HalfHeight * 1.35f * Aspect / HorizontalTangent;
    // Leave the right side of the view available for the clothing panel.
    // 화면 오른쪽은 의상 편집 패널을 위해 비워 둡니다.
    PreviewCamera->SetWorldLocationAndRotation(Center - Forward * Distance + Right * Distance * HorizontalTangent * 0.38f, Rotation);
    PreviewCamera->SetFieldOfView(45.0f);
}

void AMainMenuPreviewStage::RotateFocusedPreview(float DeltaYaw)
{
    if (FocusedSlot == INDEX_NONE) return;
    FocusedYawOffset = FRotator::NormalizeAxis(FocusedYawOffset + DeltaYaw);
    RefreshPreviewFocus();
}

void AMainMenuPreviewStage::ClearPreviewFocus()
{
    if (FocusedSlot == INDEX_NONE) return;
    for (int32 Index = 0; Index < SpawnedPreviewActors.Num(); ++Index)
    {
        if (AActor* Actor = GetPreviewActorForSlot(Index))
        {
            Actor->SetActorHiddenInGame(false);
            Actor->SetActorRotation(GetSlotAnchor(Index)->GetComponentRotation());
        }
    }
    PreviewCamera->SetWorldTransform(UnfocusedCameraTransform);
    PreviewCamera->SetFieldOfView(UnfocusedFieldOfView);
    FocusedSlot = INDEX_NONE;
    FocusedYawOffset = 0.0f;
}

AActor* AMainMenuPreviewStage::GetPreviewActorForSlot(int32 SlotIndex) const
{
    return SpawnedPreviewActors.IsValidIndex(SlotIndex) && IsValid(SpawnedPreviewActors[SlotIndex]) ? SpawnedPreviewActors[SlotIndex].Get() : nullptr;
}

void AMainMenuPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearAllPreviewActors();
    Super::EndPlay(EndPlayReason);
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
