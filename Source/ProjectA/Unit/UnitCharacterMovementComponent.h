#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnitCharacterMovementComponent.generated.h"

// Keep locomotion animation inputs available while the round coordinator owns movement.
// 라운드 조정자가 이동을 담당하는 동안에도 보행 애니메이션 입력을 유지합니다.
UCLASS()
class PROJECTA_API UUnitCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    void SetRoundMovementVelocity(const FVector& InVelocity);
    virtual void SimulateMovement(float DeltaSeconds) override;
};
