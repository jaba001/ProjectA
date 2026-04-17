#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TurnManager.generated.h"

class AUnitBase;

UCLASS()
class PROJECTA_API UTurnManager : public UObject
{
    GENERATED_BODY()

public:
    // Initialize turn order
    void InitializeTurnOrder(const TArray<AUnitBase*>& Units);

    // Start the current turn
    void StartTurn();

    // End the current turn and proceed to the next turn
    void EndTurn();

    // Find and switch to the next unit
    void NextTurn();

    // Get the current turn unit
    AUnitBase* GetCurrentUnit() const;

    // Check combat end condition (whether all remaining units belong to the same team)
    bool CheckCombatEnd() const;

    // Get current turn index (used by CombatManager for replication)
    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }

    // Get current turn counter (used by CombatManager for replication)
    int32 GetTurnCounter() const { return TurnCounter; }

    // Get current turn unit name (used for HUD update)
    FString GetCurrentUnitName() const;

private:

    // Server-only turn order array (explicitly server-only)
    UPROPERTY()
    TArray<AUnitBase*> TurnOrder;

    // Internal turn index (do not modify externally)
    int32 CurrentTurnIndex = 0;

    // Turn progression counter
    int32 TurnCounter = 0;

};