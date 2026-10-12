#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterDungeonRoute.generated.h"

class AEncounterPrototypeStage;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
struct FRunEncounterOffer;

struct FEncounterDungeonTravelSegment
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FQuat StartRotation = FQuat::Identity;
    FQuat EndRotation = FQuat::Identity;
    float Duration = 0.0f;
    int32 DoorIndex = INDEX_NONE;
};

// Own only local dungeon scenery, camera travel and a copied presentation stage.
// 로컬 던전 장식·카메라 이동·복사한 표시 무대만 소유합니다.
UCLASS(NotBlueprintable, Transient)
class PROJECTA_API AEncounterDungeonRoute : public AActor
{
    GENERATED_BODY()

public:
    AEncounterDungeonRoute();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Dungeon")
    TObjectPtr<UCameraComponent> Camera;

    // Reject invalid layouts without mutation and rebuild only when the cached variant changes.
    // 잘못된 배치는 상태 변경 없이 거절하며 캐시 변형이 바뀔 때만 재구성합니다.
    bool ConfigureLayout(int32 InLayoutVariant);
    int32 GetLayoutVariant() const { return LayoutVariant; }
    void SetPresentationVisible(bool bVisible);
    AEncounterPrototypeStage* ConfigureStage(const AEncounterPrototypeStage* Template, int32 Direction, const FRunEncounterOffer& Offer);
    void ResetAtJunction();
    bool StartTravel(int32 Direction);
    bool StartExitTravel();
    bool StartLoopEntrance();
    bool StartCombatTravel(int32 Direction);
    bool IsTraveling() const;
    AEncounterPrototypeStage* GetPresentedStage() const;
    void FinishAtStage();

    FSimpleMulticastDelegate OnTravelFinished;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FEncounterDungeonDoorLifecycleTest;
#endif

    void PrepareGeometry(UMaterialInterface* Material);
    void ClearGeometry();
    void ApplyMaterial(UInstancedStaticMeshComponent* Component, const FLinearColor& Color);
    void AddBlock(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, const FQuat& Rotation = FQuat::Identity);
    void AddMasonryPanel(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Horizontal, const FVector& Vertical, const FVector& Normal, float Width, float Height, float Thickness);
    void AddLight(const FVector& Position);
    void AddTorch(const FVector& Center, const FVector& Normal);
    void DestroyPresentedStage();
    void StopTravel();
    void AddDoorway(const FVector& Center, const FQuat& Rotation = FQuat::Identity);
    void SetDoorOpen(int32 Index, float Alpha);

    UPROPERTY()
    TObjectPtr<USceneComponent> RouteRoot;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Floor;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Ceiling;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Mortar;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> WallBlocks;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Pillars;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> LeftDoors;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> RightDoors;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> DoorSeals;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Torches;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY()
    TSoftObjectPtr<UMaterialInterface> WallMaterial;
    UPROPERTY()
    TSoftObjectPtr<UMaterialInterface> FloorMaterial;
    UPROPERTY()
    TSoftObjectPtr<UMaterialInterface> CeilingMaterial;
    UPROPERTY()
    TSoftObjectPtr<UStaticMesh> TorchMesh;
    UPROPERTY()
    TSoftObjectPtr<UNiagaraSystem> TorchFlame;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraComponent>> TorchEffects;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> RouteMaterial;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY(Transient)
    TObjectPtr<AEncounterPrototypeStage> PresentedStage;

    TArray<FEncounterDungeonTravelSegment> TravelSegments;
    TArray<FVector> DoorCenters;
    TArray<FQuat> DoorRotations;
    int32 LayoutVariant = 0;
    int32 TravelSegmentIndex = 0;
    int32 StageDirection = INDEX_NONE;
    float SegmentSeconds = 0.0f;
    bool bGeometryPrepared = false;
    bool bTraveling = false;
    bool bExiting = false;
    bool bLoopEntering = false;
    bool bCombatEntering = false;
    bool bPresentationVisible = true;
};
