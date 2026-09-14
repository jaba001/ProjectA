#include "Controller/UnitAIController.h"

void AUnitAIController::MoveUnitToLocation(const FVector& TargetLocation, float AcceptanceRadius)
{
    // Legacy path requests cannot move units outside the round coordinator.
    // 기존 경로 요청은 라운드 조정자 외부에서 유닛을 이동시키지 않습니다.
    UE_LOG(LogTemp, Warning, TEXT("[UnitAIController] Sequential path requests are retired."));
}
