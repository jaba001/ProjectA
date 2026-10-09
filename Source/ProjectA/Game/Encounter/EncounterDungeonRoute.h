#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterDungeonRoute.generated.h"

class AEncounterPrototypeStage;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;

struct FEncounterDungeonTravelSegment
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FQuat StartRotation = FQuat::Identity;
    FQuat EndRotation = FQuat::Identity;
    float Duration = 0.0f;
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

    AEncounterPrototypeStage* ConfigureStage(const AEncounterPrototypeStage* Template, int32 Direction);
    void ResetAtJunction();
    bool StartTravel(int32 Direction);
    bool IsTraveling() const;
    AEncounterPrototypeStage* GetPresentedStage() const;
    void FinishAtStage();

    FSimpleMulticastDelegate OnTravelFinished;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void PrepareGeometry(UMaterialInterface* Material);
    void ApplyMaterial(UInstancedStaticMeshComponent* Component, const FLinearColor& Color);
    void AddBlock(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size);
    void AddLight(const FVector& Position);
    void DestroyPresentedStage();
    void StopTravel();

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
    TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> RouteMaterial;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY(Transient)
    TObjectPtr<AEncounterPrototypeStage> PresentedStage;

    TArray<FEncounterDungeonTravelSegment> TravelSegments;
    int32 TravelSegmentIndex = 0;
    int32 StageDirection = INDEX_NONE;
    float SegmentSeconds = 0.0f;
    bool bGeometryPrepared = false;
    bool bTraveling = false;
};
