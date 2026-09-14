#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UnitAIController.generated.h"

// Serialized class compatibility; actual movement belongs to the round coordinator.
// 직렬화된 클래스 호환을 유지하며 실제 이동은 라운드 조정자가 담당합니다.
UCLASS()
class PROJECTA_API AUnitAIController : public AAIController
{
    GENERATED_BODY()

public:

    // Rejects legacy path requests.
    // 기존 경로 이동 요청을 거절합니다.
    void MoveUnitToLocation(const FVector& TargetLocation, float AcceptanceRadius = 10.f);

};
