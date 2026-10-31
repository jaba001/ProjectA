#include "Combat/Presentation/CombatShoulderCamera.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Unit/UnitBase.h"

float CombatShoulderCameraGeometry::UpdateYaw(float CurrentYaw, float UnitYaw, float DeltaTime, float InterpSpeed, float MaxDegreesPerSecond, bool bReturning)
{
    if (bReturning) return CurrentYaw;
    const float Step = FMath::Clamp(DeltaTime, 0.f, 0.1f);
    const float Alpha = 1.f - FMath::Exp(-FMath::Max(1.f, InterpSpeed) * Step);
    const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, UnitYaw);
    const float MaxStep = FMath::Max(0.f, MaxDegreesPerSecond) * Step;
    return FRotator::NormalizeAxis(CurrentYaw + FMath::Clamp(DeltaYaw * Alpha, -MaxStep, MaxStep));
}

float CombatShoulderCameraGeometry::DecorationHitFraction(const FBox& Bounds, const FVector& Pivot, const FVector& Camera, float Radius)
{
    if (!Bounds.IsValid) return 1.f;
    FVector HitLocation;
    FVector HitNormal;
    float HitTime = 1.f;
    return FMath::LineExtentBoxIntersection(Bounds, Pivot, Camera, FVector(FMath::Max(0.f, Radius)), HitLocation, HitNormal, HitTime) ? FMath::Clamp(HitTime, 0.f, 1.f) : 1.f;
}

ACombatShoulderCamera::ACombatShoulderCamera()
{
    // Camera actors bypass CalcCamera; a plain view target keeps the custom POV on the engine camera path.
    // CameraActor는 CalcCamera를 우회하므로 일반 시점 액터로 엔진의 커스텀 POV 호출 경로를 사용합니다.
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
    bFindCameraComponentWhenViewTarget = false;
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetReplicateMovement(false);
    SetActorEnableCollision(false);
}

void ACombatShoulderCamera::FollowUnit(AUnitBase* InUnit, const FMinimalViewInfo& TacticalPOV)
{
    FollowedUnit = InUnit;
    bHasPose = false;
    bReturning = false;
    ViewTemplate = FMinimalViewInfo();
    ViewTemplate.PostProcessSettings = TacticalPOV.PostProcessSettings;
    ViewTemplate.PostProcessBlendWeight = TacticalPOV.PostProcessBlendWeight;
    ViewTemplate.AspectRatio = TacticalPOV.AspectRatio;
    ViewTemplate.AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
    ViewTemplate.PerspectiveNearClipPlane = 5.f;
    ViewTemplate.bConstrainAspectRatio = false;
    LastView = TacticalPOV;
    CacheDecorationBounds();
    // Initialize the pose before blending so the new view target never starts at its spawn origin.
    // 블렌드 전에 구도를 초기화하여 새 시점이 스폰 원점에서 시작하지 않도록 합니다.
    FMinimalViewInfo InitialView;
    CalcCamera(0.f, InitialView);
}

void ACombatShoulderCamera::CacheDecorationBounds()
{
    DecorationBounds.Reset();
    const AUnitBase* Unit = FollowedUnit.Get();
    if (!Unit || !GetWorld()) return;
    const float FloorZ = Unit->GetActorLocation().Z - Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor->IsHidden() || (!Actor->ActorHasTag(TEXT("ProjectAUnifiedGameplayVisual")) && !IsValid(Cast<ACombatArena>(Actor->GetOwner())))) continue;
        TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
        for (UStaticMeshComponent* Component : Components)
        {
            if (!IsValid(Component) || !Component->GetStaticMesh() || !Component->IsVisible() || Component->bHiddenInGame) continue;
            if (Component->Bounds.GetBox().ComputeSquaredDistanceToPoint(Unit->GetActorLocation()) > FMath::Square(2400.0)) continue;
            const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component);
            const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
            for (int32 Index = 0; Index < Count; ++Index)
            {
                FTransform Transform = Component->GetComponentTransform();
                if (Instances && !Instances->GetInstanceTransform(Index, Transform, true)) continue;
                const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox().TransformBy(Transform);
                if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || Bounds.Max.Z <= FloorZ + 20.f || Bounds.ComputeSquaredDistanceToPoint(Unit->GetActorLocation()) > FMath::Square(2400.0)) continue;
                FDecorationBounds& Entry = DecorationBounds.AddDefaulted_GetRef();
                Entry.Component = Component;
                Entry.Bounds = Bounds;
            }
        }
    }
}

FVector ACombatShoulderCamera::ResolveCameraCollision(const FVector& Pivot, const FVector& DesiredLocation) const
{
    const FVector Segment = DesiredLocation - Pivot;
    const double Length = Segment.Size();
    if (Length <= UE_SMALL_NUMBER) return Pivot;
    float SafeTime = 1.f;
    const float Radius = FMath::Max(5.f, ProbeRadius);

    // Frozen encounter decoration stays nonphysical; instance bounds only shorten this local camera arm.
    // 고정 인카운터 장식의 물리는 유지하며 인스턴스 경계로 로컬 카메라 거리만 줄입니다.
    for (const FDecorationBounds& Entry : DecorationBounds)
    {
        const UStaticMeshComponent* Component = Entry.Component.Get();
        if (!Component || !Component->IsVisible() || Component->bHiddenInGame || !Component->GetOwner() || Component->GetOwner()->IsHidden()) continue;
        SafeTime = FMath::Min(SafeTime, CombatShoulderCameraGeometry::DecorationHitFraction(Entry.Bounds, Pivot, DesiredLocation, Radius));
    }
    if (UWorld* World = GetWorld())
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatShoulderCamera), false, FollowedUnit.Get());
        Params.AddIgnoredActor(this);
        // Unit meshes block the camera channel; a melee run past other units would otherwise snap the arm in for a frame.
        // 유닛 메시는 카메라 채널을 막으므로 근접 이동 중 다른 유닛을 지나가면 카메라 거리가 순간적으로 당겨지는 것을 막습니다.
        for (TActorIterator<AUnitBase> It(World); It; ++It) Params.AddIgnoredActor(*It);
        FHitResult Hit;
        if (World->SweepSingleByChannel(Hit, Pivot, DesiredLocation, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(Radius), Params)) SafeTime = FMath::Min(SafeTime, Hit.Time);
    }
    if (SafeTime < 1.f) SafeTime = FMath::Max(0.f, SafeTime - static_cast<float>(2.0 / Length));
    return Pivot + Segment * SafeTime;
}

void ACombatShoulderCamera::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    const AUnitBase* Unit = FollowedUnit.Get();
    if (!Unit)
    {
        OutResult = LastView;
        return;
    }
    const float Step = FMath::Clamp(DeltaTime, 0.f, 0.1f);
    const float Alpha = 1.f - FMath::Exp(-FMath::Max(1.f, FollowInterpSpeed) * Step);
    const float HalfHeight = Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Pivot = Unit->GetActorLocation() + FVector(0.f, 0.f, HalfHeight * FMath::Clamp(PivotHeightFraction, -1.f, 1.f));
    const FVector Lead = (Unit->GetVelocity() * FMath::Max(0.f, VelocityLeadTime)).GetClampedToMaxSize(35.f);
    if (!bHasPose)
    {
        SmoothedPivot = Pivot;
        SmoothedYaw = Unit->GetActorRotation().Yaw;
    }
    else
    {
        SmoothedPivot = FMath::Lerp(SmoothedPivot, Pivot + Lead, Alpha);
        SmoothedPivot = Pivot + (SmoothedPivot - Pivot).GetClampedToMaxSize(FMath::Max(0.f, MaxPositionLag));
        // Return movement faces the starting tile; keep the action's heading instead of orbiting by 180 degrees.
        // 복귀 이동은 원래 타일을 바라보므로 180도 공전하지 않고 행동 시선 방향을 유지합니다.
        SmoothedYaw = CombatShoulderCameraGeometry::UpdateYaw(SmoothedYaw, Unit->GetActorRotation().Yaw, DeltaTime, FollowInterpSpeed, MaxYawDegreesPerSecond, bReturning);
    }
    const FRotator Heading(0.f, SmoothedYaw, 0.f);
    const FVector Forward = Heading.Vector();
    const FVector Right = FRotationMatrix(Heading).GetUnitAxis(EAxis::Y);
    const FVector LookAt = SmoothedPivot + Forward * FMath::Max(0.f, LookAheadDistance);
    const FVector HorizontalLocation = SmoothedPivot - Forward * FMath::Max(50.f, FollowDistance) + Right * FMath::Max(0.f, ShoulderOffset);
    // Include the look-ahead and shoulder offsets so the unobstructed view uses the requested downward angle.
    // 장애물이 없는 시점이 지정한 내려다보기 각도를 유지하도록 전방 주시 거리와 어깨 오프셋을 함께 반영합니다.
    const double Rise = FVector::Dist2D(HorizontalLocation, LookAt) * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(DownwardViewAngle, 5.f, 75.f)));
    const FVector DesiredLocation = HorizontalLocation + FVector(0.f, 0.f, Rise);
    const FVector SafeLocation = ResolveCameraCollision(Pivot, DesiredLocation);
    const float SafeDistance = static_cast<float>(FVector::Distance(Pivot, SafeLocation));

    // Obstructions retract immediately; recovering arm length eases out and never interpolates through a wall.
    // 장애물이 있으면 즉시 당기고 복구 거리는 벽을 통과하지 않도록 부드럽게 늘립니다.
    CurrentArmDistance = !bHasPose || SafeDistance < CurrentArmDistance ? SafeDistance : FMath::Lerp(CurrentArmDistance, SafeDistance, Alpha);
    const FVector Location = Pivot + (SafeLocation - Pivot).GetSafeNormal() * CurrentArmDistance;
    OutResult = ViewTemplate;
    OutResult.Location = Location;
    OutResult.Rotation = (LookAt - Location).Rotation();
    OutResult.FOV = FMath::Clamp(ShoulderFOV, 45.f, 100.f);
    OutResult.DesiredFOV = OutResult.FOV;
    SetActorLocationAndRotation(OutResult.Location, OutResult.Rotation);
    LastView = OutResult;
    bHasPose = true;
}
