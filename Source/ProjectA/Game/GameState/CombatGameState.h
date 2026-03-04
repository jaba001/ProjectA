#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CombatGameState.generated.h"

class UTurnManager;

UCLASS()
class PROJECTA_API ACombatGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ACombatGameState();

protected:
    virtual void BeginPlay() override;

public:

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn")
    UTurnManager* TurnManager;

    // 필요시 TurnManager 재생성 가능
    UFUNCTION(BlueprintCallable, Category = "Turn")
    void CreateTurnManager();
};
