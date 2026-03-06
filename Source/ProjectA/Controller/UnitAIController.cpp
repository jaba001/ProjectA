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

    Unit->HandleMoveCompleted();
}