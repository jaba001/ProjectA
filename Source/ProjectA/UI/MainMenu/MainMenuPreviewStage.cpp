#include "UI/MainMenu/MainMenuPreviewStage.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Unit/CharacterAppearanceComponent.h"

AMainMenuPreviewStage::AMainMenuPreviewStage()
{
    PrimaryActorTick.bCanEverTick = true;
    SpawnedPreviewActors.SetNum(4);
    PreviewBaseRotations.Init(FQuat::Identity, 4);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
    PreviewCamera->SetupAttachment(SceneRoot);
    PreviewCamera->SetRelativeLocation(FVector(-500.0f, 0.0f, 140.0f));
    PreviewCamera->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));

    Slot0Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot0Anchor"));
    Slot0Anchor->SetupAttachment(SceneRoot);
    Slot0Anchor->SetRelativeLocation(FVector(0.0f, -450.0f, 0.0f));
    Slot0Anchor->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

    Slot1Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot1Anchor"));
    Slot1Anchor->SetupAttachment(SceneRoot);
    Slot1Anchor->SetRelativeLocation(FVector(0.0f, -150.0f, 0.0f));
    Slot1Anchor->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

    Slot2Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot2Anchor"));
    Slot2Anchor->SetupAttachment(SceneRoot);
    Slot2Anchor->SetRelativeLocation(FVector(0.0f, 150.0f, 0.0f));
    Slot2Anchor->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

    Slot3Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Slot3Anchor"));
    Slot3Anchor->SetupAttachment(SceneRoot);
    Slot3Anchor->SetRelativeLocation(FVector(0.0f, 450.0f, 0.0f));
    Slot3Anchor->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
}

void AMainMenuPreviewStage::BeginPlay()
{
    Super::BeginPlay();
    OverviewCameraTransform = PreviewCamera->GetComponentTransform();
    OverviewFieldOfView = PreviewCamera->FieldOfView;
    UnfocusedCameraTransform = OverviewCameraTransform;
    UnfocusedFieldOfView = OverviewFieldOfView;
    InvalidateOverviewCameraFit();
}

void AMainMenuPreviewStage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Controller || Controller->GetViewTarget() != this || !Controller->PlayerCameraManager) return;
    int32 Width = 0;
    int32 Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return;
    const FIntPoint ViewportSize(Width, Height);
    const bool bViewportChanged = PreviewViewportSize != ViewportSize;
    if (bViewportChanged)
    {
        PreviewViewportSize = ViewportSize;
        InvalidateOverviewCameraFit();
    }
    if (FocusedSlot != INDEX_NONE)
    {
        if (bViewportChanged) RefreshPreviewFocus();
        return;
    }
    if (bOverviewCameraFitInvalidated)
    {
        // Start a refit from the authored camera and wait for its actual cached view before projecting bounds.
        // 작성된 카메라에서 재조정을 시작하고 실제 캐시 시점이 반영된 뒤 몸체 경계를 투영합니다.
        PreviewCamera->SetWorldTransform(OverviewCameraTransform);
        PreviewCamera->SetFieldOfView(OverviewFieldOfView);
        OverviewCameraDistanceScale = 1.0;
        bOverviewCameraFitInvalidated = false;
        return;
    }
    if (!Controller->PlayerCameraManager->GetCameraLocation().Equals(PreviewCamera->GetComponentLocation(), 0.1f) || !Controller->PlayerCameraManager->GetCameraRotation().Equals(PreviewCamera->GetComponentRotation(), 0.1f) || !FMath::IsNearlyEqual(Controller->PlayerCameraManager->GetFOVAngle(), PreviewCamera->FieldOfView, 0.1f)) return;
    double RequiredScale = 1.0;
    for (AActor* Actor : SpawnedPreviewActors)
    {
        const USkeletalMeshComponent* Mesh = IsValid(Actor) ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
        if (!Mesh || Mesh->Bounds.BoxExtent.IsNearlyZero()) continue;
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector Point = Mesh->Bounds.Origin + Mesh->Bounds.BoxExtent * FVector((Corner & 1) ? 1.0 : -1.0, (Corner & 2) ? 1.0 : -1.0, (Corner & 4) ? 1.0 : -1.0);
            FVector2D Screen;
            if (!Controller->ProjectWorldLocationToScreen(Point, Screen, true)) continue;
            RequiredScale = FMath::Max(RequiredScale, FMath::Abs(Screen.X - Width * 0.5) / (Width * 0.4));
        }
    }
    if (RequiredScale <= 1.005) return;
    // Fit all four bodies inside the party cards' horizontal margins using the actual viewport projection.
    // 실제 뷰포트 투영을 사용하여 네 몸체가 파티 카드의 수평 여백 안에 들어오도록 맞춥니다.
    const FVector Center = (Slot0Anchor->GetComponentLocation() + Slot3Anchor->GetComponentLocation()) * 0.5 + GetActorUpVector() * 90.0;
    OverviewCameraDistanceScale *= RequiredScale;
    PreviewCamera->SetWorldLocation(Center + (OverviewCameraTransform.GetLocation() - Center) * OverviewCameraDistanceScale);
}

void AMainMenuPreviewStage::InvalidateOverviewCameraFit()
{
    bOverviewCameraFitInvalidated = true;
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
    if (const USceneComponent* PreviewRoot = PreviewActor->GetRootComponent()) PreviewBaseRotations[SlotIndex] = PreviewRoot->GetRelativeTransform().GetRotation();
    InvalidateOverviewCameraFit();
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
    PreviewBaseRotations[SlotIndex] = FQuat::Identity;
    InvalidateOverviewCameraFit();
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
    // Restore the body orientation before applying appearance so repeated refreshes do not accumulate drag rotation.
    // 외형 적용 전에 몸체 기본 방향을 복원하여 반복 갱신에서 드래그 회전이 누적되지 않게 합니다.
    PreviewActor->SetActorRelativeRotation(PreviewBaseRotations[SlotIndex]);
    UCharacterAppearanceComponent* Appearance = PreviewActor->FindComponentByClass<UCharacterAppearanceComponent>();
    if (!Appearance && Catalog)
    {
        Appearance = NewObject<UCharacterAppearanceComponent>(PreviewActor);
        PreviewActor->AddInstanceComponent(Appearance);
        Appearance->RegisterComponent();
    }
    const bool bApplied = Appearance ? Appearance->SetAppearance(Catalog, Selection) : !Catalog && Selection.IsEmpty();
    if (bApplied && PreviewActor->GetRootComponent()) PreviewBaseRotations[SlotIndex] = PreviewActor->GetRootComponent()->GetRelativeTransform().GetRotation();
    if (bApplied) InvalidateOverviewCameraFit();
    if (FocusedSlot == SlotIndex) RefreshPreviewFocus();
    return bApplied;
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
    // Rotate around the slot anchor while retaining the selected body's local orientation.
    // 선택한 몸체의 로컬 방향을 유지한 채 슬롯 앵커를 기준으로 회전합니다.
    const FQuat RotationOffset(FVector::UpVector, FMath::DegreesToRadians(FocusedYawOffset));
    FocusedActor->SetActorRelativeRotation(RotationOffset * PreviewBaseRotations[FocusedSlot]);
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
    const float Distance = HalfHeight * FMath::Max(0.1f, FocusedCameraDistanceScale) * Aspect / HorizontalTangent;
    // Leave the right side of the view available for the character editor panel.
    // 화면 오른쪽은 캐릭터 편집 패널을 위해 비워 둡니다.
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
            Actor->SetActorRelativeRotation(PreviewBaseRotations[Index]);
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
