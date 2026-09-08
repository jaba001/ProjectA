#include "Controller/UnitAIController.h"
#include "Unit/UnitBase.h"
#include "Navigation/PathFollowingComponent.h"

void AUnitAIController::MoveUnitToLocation(const FVector& TargetLocation, float AcceptanceRadius)
{
    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(TargetLocation);
    MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetProjectGoalLocation(true);
    MoveRequest.SetAllowPartialPath(false);
    MoveRequest.SetNavigationFilter(DefaultNavigationFilterClass);
    MoveRequest.SetReachTestIncludesAgentRadius(true);
    MoveRequest.SetCanStrafe(false);

    // Immediate rejection and arrival may callback inside MoveTo itself.
    // 즉시 거절과 도착은 MoveTo 내부에서 콜백을 호출할 수 있습니다.
    ActiveUnitMoveRequest = FAIRequestID::InvalidRequest;
    bIssuingUnitMove = true;
    StopMovement();
    const FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
    bIssuingUnitMove = false;

    AUnitBase* Unit = Cast<AUnitBase>(GetPawn());
    if (!Unit)
    {
        return;
    }

    if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        ActiveUnitMoveRequest = RequestResult.MoveId;
    }
    else if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        Unit->HandleMoveCompleted();
    }
    else
    {
        Unit->HandleMoveFailed(EUnitActionResult::Failed);
    }
}

void AUnitAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    if (bIssuingUnitMove || !ActiveUnitMoveRequest.IsValid() || RequestID != ActiveUnitMoveRequest)
    {
        return;
    }

    ActiveUnitMoveRequest = FAIRequestID::InvalidRequest;

    AUnitBase* Unit = Cast<AUnitBase>(GetPawn());

    if (!Unit)
    {
        return;
    }

    if (!Result.IsSuccess())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UnitAIController] Move failed | Unit=%s | Result=%d"), *GetNameSafe(Unit), static_cast<int32>(Result.Code));
        EUnitActionResult ActionResult = EUnitActionResult::Failed;
        if (Result.Code == EPathFollowingResult::Aborted)
        {
            ActionResult = EUnitActionResult::Cancelled;
        }

        Unit->HandleMoveFailed(ActionResult);
        return;
    }

    Unit->HandleMoveCompleted();
}
