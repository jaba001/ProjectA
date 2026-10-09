#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatArena.generated.h"

class ACombatGridManager;

UCLASS(Blueprintable)
class PROJECTA_API ACombatArena : public AActor
{
    GENERATED_BODY()

public:
    ACombatArena();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena")
    TObjectPtr<ACombatGridManager> Grid;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Arena")
    TObjectPtr<AActor> CameraAnchor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
    TArray<FIntPoint> PlayerCoords;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
    TArray<FIntPoint> EnemyCoords;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
    FTransform CameraTransform;

    // Reduce scene saturation only when the placed camera has no authored saturation override.
    // 배치된 카메라에 채도 재정의가 없을 때만 장면의 채도를 완화합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Presentation", meta = (ClampMin = "0", ClampMax = "1"))
    float DefaultSceneSaturation = 0.88f;

    // Arena owns placement and visibility; encounters own units and results.
    // 아레나는 배치와 표시를 담당하고 인카운터는 유닛과 결과를 소유합니다.
    virtual bool PrepareArena(FText& OutError);
    virtual void ActivateArena(APlayerController* Controller);
    virtual void CleanupArena();

private:
    UPROPERTY(Transient)
    TObjectPtr<AActor> RuntimeCamera;
};
