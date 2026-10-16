#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterMazeLayout.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

USTRUCT(BlueprintType)
struct PROJECTA_API FEncounterMazeTorchMount
{
    GENERATED_BODY()

    // Corridor point in front of the wall and the wall normal facing away from the corridor.
    // 벽 앞 통로 지점과 통로 바깥쪽을 향하는 벽 법선입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    FVector Center = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    FVector Normal = FVector::ForwardVector;
};

// Static stone scenery shared by every saved maze variant, baked into one editable Blueprint.
// The route actor spawns it and keeps doors, torch lights and camera travel in code.
// 모든 저장 미로 변형이 공유하는 정적 석재 장식이며 편집 가능한 Blueprint 하나로 저장합니다.
// 통로 액터가 이를 생성하고 문·횃불 조명·카메라 이동은 코드에서 유지합니다.
UCLASS(Blueprintable)
class PROJECTA_API AEncounterMazeLayout : public AActor
{
    GENERATED_BODY()

public:
    AEncounterMazeLayout();

    // Regenerate instances from the shared layout rules for explicit editor baking only.
    // 명시적인 에디터 굽기에서만 공통 배치 규칙으로 인스턴스를 다시 만듭니다.
    UFUNCTION(BlueprintCallable, Category = "Encounter Maze")
    bool BuildFromLayout(int32 InLayoutVariant);

    void SetPresentationVisible(bool bVisible);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<USceneComponent> MazeRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<UInstancedStaticMeshComponent> Floor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<UInstancedStaticMeshComponent> Ceiling;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<UInstancedStaticMeshComponent> Mortar;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<UInstancedStaticMeshComponent> WallBlocks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<UInstancedStaticMeshComponent> Pillars;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TObjectPtr<UInstancedStaticMeshComponent> Torches;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    int32 LayoutVariant = INDEX_NONE;
    // Torch sconces whose lights and flames the route spawns locally.
    // 통로 액터가 조명과 불꽃을 로컬로 생성할 벽 횃불 위치입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter Maze")
    TArray<FEncounterMazeTorchMount> TorchMounts;

    static FTransform MakeTorchTransform(const FEncounterMazeTorchMount& Mount);
    static FVector GetFlamePosition(const FEncounterMazeTorchMount& Mount);

private:
    void ClearInstances();
    void AddBlock(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, const FQuat& Rotation = FQuat::Identity);
    void AddMasonryPanel(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Horizontal, const FVector& Vertical, const FVector& Normal, float Width, float Height, float Thickness);
    void AddDoorFrame(const FVector& Center, const FQuat& Rotation = FQuat::Identity);
};
