#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UnitAIController.generated.h"

struct FPathFollowingResult;

// AI controller that reports movement completion back to units.
// 이동 완료 결과를 유닛으로 전달하는 AI 컨트롤러입니다.
UCLASS()
class PROJECTA_API AUnitAIController : public AAIController
{
    GENERATED_BODY()

public:

    // Moves the controlled unit to a world-space location.
    // 조종 중인 유닛을 월드 위치로 이동시킵니다.
    void MoveUnitToLocation(const FVector& TargetLocation, float AcceptanceRadius = 10.f);

protected:

    // Handles path following completion and failure callbacks.
    // 경로 이동 완료와 실패 콜백을 처리합니다.
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
    FAIRequestID ActiveUnitMoveRequest = FAIRequestID::InvalidRequest;
    bool bIssuingUnitMove = false;
};
