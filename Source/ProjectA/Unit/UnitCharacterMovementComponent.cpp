#include "UnitCharacterMovementComponent.h"

#include "Unit/UnitBase.h"

void UUnitCharacterMovementComponent::SetRoundMovementVelocity(const FVector& InVelocity)
{
    Velocity = InVelocity.ContainsNaN() ? FVector::ZeroVector : FVector(InVelocity.X, InVelocity.Y, 0.0);
    // The existing AnimBP requires both speed and nonzero movement intent to leave idle.
    // 기존 AnimBP는 속도와 0이 아닌 이동 의도가 모두 있어야 대기 자세를 벗어납니다.
    Acceleration = Velocity.GetSafeNormal() * GetMaxAcceleration();
    UpdateComponentVelocity();
}

void UUnitCharacterMovementComponent::SimulateMovement(float DeltaSeconds)
{
    Super::SimulateMovement(DeltaSeconds);
    if (!HasValidData() || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy || MovementMode != MOVE_None || UpdatedComponent->IsSimulatingPhysics()) return;
    // MOVE_None skips the engine's proxy acceleration update; reuse the authoritative replicated velocity.
    // MOVE_None은 엔진의 프록시 가속도 갱신을 생략하므로 서버에서 복제한 속도를 재사용합니다.
    const AUnitBase* Unit = Cast<AUnitBase>(CharacterOwner);
    SetRoundMovementVelocity(Unit && Unit->IsUnitAlive() ? CharacterOwner->GetReplicatedMovement().LinearVelocity : FVector::ZeroVector);
}
