#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"
#include "CombatShoulderCamera.generated.h"

class AUnitBase;
class UStaticMeshComponent;

namespace CombatShoulderCameraGeometry
{
    PROJECTA_API float UpdateYaw(float CurrentYaw, float UnitYaw, float DeltaTime, float InterpSpeed, float MaxDegreesPerSecond, bool bReturning);
    PROJECTA_API float DecorationHitFraction(const FBox& Bounds, const FVector& Pivot, const FVector& Camera, float Radius);
}

// A local view target follows one action without possessing or rotating its unit.
// 유닛을 빙의하거나 회전시키지 않고 하나의 행동을 따라가는 로컬 시점입니다.
UCLASS(NotPlaceable, Transient)
class PROJECTA_API ACombatShoulderCamera : public ACameraActor
{
    GENERATED_BODY()

public:
    ACombatShoulderCamera();
    void FollowUnit(AUnitBase* InUnit, const FMinimalViewInfo& TacticalPOV);
    void SetReturning(bool bInReturning) { bReturning = bInReturning; }
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "50", ClampMax = "600", Units = "cm"))
    float FollowDistance = 250.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "0", ClampMax = "150", Units = "cm"))
    float ShoulderOffset = 65.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "0", ClampMax = "150", Units = "cm"))
    float CameraHeight = 25.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "0", ClampMax = "1"))
    float PivotHeightFraction = 0.55f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "100", ClampMax = "1000", Units = "cm"))
    float LookAheadDistance = 380.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "45", ClampMax = "100", Units = "deg"))
    float ShoulderFOV = 65.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "1", ClampMax = "30"))
    float FollowInterpSpeed = 12.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "30", ClampMax = "360"))
    float MaxYawDegreesPerSecond = 150.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "0", ClampMax = "150", Units = "cm"))
    float MaxPositionLag = 65.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "0", ClampMax = "0.2", Units = "s"))
    float VelocityLeadTime = 0.06f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|ShoulderCamera", meta = (ClampMin = "5", ClampMax = "40", Units = "cm"))
    float ProbeRadius = 14.f;

private:
    struct FDecorationBounds
    {
        TWeakObjectPtr<UStaticMeshComponent> Component;
        FBox Bounds = FBox(ForceInit);
    };

    void CacheDecorationBounds();
    FVector ResolveCameraCollision(const FVector& Pivot, const FVector& DesiredLocation) const;
    TWeakObjectPtr<AUnitBase> FollowedUnit;
    TArray<FDecorationBounds> DecorationBounds;
    UPROPERTY(Transient)
    FMinimalViewInfo ViewTemplate;
    UPROPERTY(Transient)
    FMinimalViewInfo LastView;
    FVector SmoothedPivot = FVector::ZeroVector;
    float SmoothedYaw = 0.f;
    float CurrentArmDistance = 0.f;
    bool bHasPose = false;
    bool bReturning = false;
};
