#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UnitAIController.generated.h"

struct FPathFollowingResult;

UCLASS()
class PROJECTA_API AUnitAIController : public AAIController
{
    GENERATED_BODY()

public:

    void MoveUnitToLocation(const FVector& TargetLocation, float AcceptanceRadius = 10.f);

protected:

    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
};