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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

    // The authority freezes the selected environment; clients reconstruct only its local decoration.
    // 권위 측이 선택 환경을 고정하고 클라이언트는 로컬 장식만 재구성합니다.
    bool ApplyEnvironment(FName ArenaId, FText& OutError);
    void ResetEnvironment();

    UPROPERTY(ReplicatedUsing = OnRep_EnvironmentId, BlueprintReadOnly, Category = "Arena|Presentation")
    FName EnvironmentId;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<AActor> RuntimeCamera;

    UPROPERTY(Transient)
    TObjectPtr<AActor> EnvironmentVisual;

    // Keep original visibility so retries, switching and teardown restore only our own changes.
    // 재시도·교체·종료 시 이 기능의 변경만 복원하도록 원래 표시 상태를 보관합니다.
    TMap<TWeakObjectPtr<AActor>, bool> OriginalEnvironmentVisibility;
    FName AppliedEnvironmentId;

    UFUNCTION()
    void OnRep_EnvironmentId();
    bool ApplyEnvironmentVisuals(FName ArenaId, FText& OutError);
    void ResetEnvironmentVisuals();
};
