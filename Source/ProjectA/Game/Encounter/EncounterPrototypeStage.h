#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Run/RunEncounterTypes.h"
#include "EncounterPrototypeStage.generated.h"

class UCameraComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace EncounterPresentation
{
    PROJECTA_API bool MatchesOffer(const FRunEncounterOffer& Offer, const FGameplayTagContainer& RequiredTags, const FGameplayTagContainer& ExcludedTags);
}

// Keep reusable scenery and local greeting animation separate from authoritative Run transactions.
// 재사용 무대와 로컬 인사 연출을 권위 있는 Run 거래 처리와 분리합니다.
UCLASS(Blueprintable)
class PROJECTA_API AEncounterPrototypeStage : public AActor
{
    GENERATED_BODY()

public:
    AEncounterPrototypeStage();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostRegisterAllComponents() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage")
    FName StageId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage")
    FText StageTitle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage", meta = (Categories = "Encounter"))
    FGameplayTagContainer RequiredTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage", meta = (Categories = "Encounter"))
    FGameplayTagContainer ExcludedTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage")
    int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage")
    FLinearColor Tint = FLinearColor(0.12f, 0.32f, 0.42f, 1.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage")
    TObjectPtr<UMaterialInterface> PrototypeMaterial;

    // Style changes geometry only; gameplay classification always uses the authored tags.
    // 스타일은 도형 표현만 바꾸며 게임플레이 분류는 항상 작성된 태그를 사용합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Stage", meta = (ClampMin = "0", ClampMax = "4"))
    int32 VisualStyle = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Stage")
    TObjectPtr<UCameraComponent> Camera;

    UFUNCTION(BlueprintPure, Category = "Encounter Stage")
    bool MatchesOffer(const FRunEncounterOffer& Offer) const;
    UFUNCTION(BlueprintCallable, Category = "Encounter Stage")
    void RefreshPrototype();
    UFUNCTION(BlueprintCallable, Category = "Encounter Stage")
    void StartPresentation();
    UFUNCTION(BlueprintCallable, Category = "Encounter Stage")
    void StopPresentation();

protected:
    virtual void BeginPlay() override;

private:
    void ConfigureShape(UStaticMeshComponent* Component, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color);
    void ResetPose();

    UPROPERTY()
    TObjectPtr<USceneComponent> StageRoot;
    UPROPERTY()
    TObjectPtr<USceneComponent> NPCBody;
    UPROPERTY()
    TObjectPtr<USceneComponent> RightArmPivot;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Platform;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Counter;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Canopy;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftPost;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightPost;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SignBoard;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Torso;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftArm;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightArm;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LeftEye;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RightEye;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Hat;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> DisplayBase;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> DisplayAccent;
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> SignText;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> ConeMesh;

    UPROPERTY(Transient)
    float PresentationSeconds = 0.0f;
    UPROPERTY(Transient)
    bool bPresenting = false;
};
