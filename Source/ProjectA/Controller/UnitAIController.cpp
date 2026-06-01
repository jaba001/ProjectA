#include "Controller/UnitAIController.h"
#include "Unit/UnitBase.h"
#include "Navigation/PathFollowingComponent.h"

void AUnitAIController::MoveUnitToLocation(const FVector& TargetLocation, float AcceptanceRadius)
{
    MoveToLocation(TargetLocation, AcceptanceRadius);
}

void AUnitAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    AUnitBase* Unit = Cast<AUnitBase>(GetPawn());

    if (!Unit)
    {
        return;
    }

    if (!Result.IsSuccess())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitAIController] Move failed | Unit=%s | Result=%d"), *GetNameSafe(Unit), static_cast<int32>(Result.Code));
        Unit->HandleMoveFailed();
        return;
    }

    Unit->HandleMoveCompleted();
}
