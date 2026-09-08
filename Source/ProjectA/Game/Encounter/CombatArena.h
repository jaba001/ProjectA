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

    // Arena owns placement and visibility; encounters own units and results.
    // 아레나는 배치와 표시를 담당하고 인카운터는 유닛과 결과를 소유합니다.
    virtual bool PrepareArena(FText& OutError);
    virtual void ActivateArena(APlayerController* Controller);
    virtual void CleanupArena();

private:
    UPROPERTY(Transient)
    TObjectPtr<AActor> RuntimeCamera;
};
