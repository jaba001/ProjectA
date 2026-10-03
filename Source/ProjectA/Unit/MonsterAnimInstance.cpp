#include "Unit/MonsterAnimInstance.h"
#include "GameFramework/Pawn.h"

UMonsterAnimInstance::UMonsterAnimInstance()
{
    // Extract authored root motion for the pose while the round remains the sole actor movement authority.
    // 작성된 루트 모션을 포즈에서 추출하되 액터 이동 권한은 라운드에만 유지합니다.
    RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void UMonsterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const APawn* Pawn = TryGetPawnOwner();
    const float Speed = IsValid(Pawn) ? static_cast<float>(Pawn->GetVelocity().Size2D()) : 0.f;
    GroundSpeed = FMath::IsFinite(Speed) ? FMath::Max(0.f, Speed) : 0.f;
}
