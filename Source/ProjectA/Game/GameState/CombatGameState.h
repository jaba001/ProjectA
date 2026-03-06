#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CombatGameState.generated.h"

UCLASS()
class PROJECTA_API ACombatGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ACombatGameState();

protected:
    virtual void BeginPlay() override;


};
