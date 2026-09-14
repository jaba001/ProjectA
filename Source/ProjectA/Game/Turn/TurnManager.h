#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/CombatResult.h"
#include "TurnManager.generated.h"

class AUnitBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatResult, ECombatResult);
DECLARE_MULTICAST_DELEGATE(FOnTurnChanged);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FCommitCombatTurnBoundary, int32, int32);

// Compatibility type for existing serialized references; sequential execution has been removed.
// 기존 직렬화 참조용 호환 타입이며 순차 실행은 제거되었습니다.
UCLASS()
class PROJECTA_API UTurnManager : public UObject
{
    GENERATED_BODY()

public:
    FOnCombatResult OnCombatResult;
    FOnTurnChanged OnTurnChanged;
    FCommitCombatTurnBoundary CommitTurnBoundary;
    bool IsCombatActive() const { return false; }
    bool IsAwaitingTurnCheckpoint() const { return false; }
    bool RetryTurnCheckpoint();
    bool RestoreFromBoundary(const TArray<AUnitBase*>& Units, int32 CompletedTurnSerial, int32 NextTurnIndex);
    void SuspendForRecovery();
    ECombatResult GetCombatResult() const { return ECombatResult::None; }
    void EvaluateCombatResult();
    void StopCombat();
    void ResetCombat();
    int32 GetRegisteredUnitCount() const { return 0; }
    void InitializeTurnOrder(const TArray<AUnitBase*>& Units);
    void StartTurn();
    void EndTurn();
    void NextTurn();
    AUnitBase* GetCurrentUnit() const { return nullptr; }
    bool CheckCombatEnd() const { return false; }
    int32 GetCurrentTurnIndex() const { return INDEX_NONE; }
    int32 GetTurnCounter() const { return 0; }
    FString GetCurrentUnitName() const { return TEXT("None"); }
};
