#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/CombatResult.h"
#include "TurnManager.generated.h"

class AUnitBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatResult, ECombatResult);
DECLARE_MULTICAST_DELEGATE(FOnTurnChanged);

// Server-side object that manages combat turn order.
// 전투 턴 순서를 관리하는 서버 측 오브젝트입니다.
UCLASS()
class PROJECTA_API UTurnManager : public UObject
{
    GENERATED_BODY()

public:
    FOnCombatResult OnCombatResult;
    FOnTurnChanged OnTurnChanged;

    bool IsCombatActive() const { return bCombatActive; }
    ECombatResult GetCombatResult() const { return CombatResult; }
    void EvaluateCombatResult();
    void StopCombat();
    void ResetCombat();
    int32 GetRegisteredUnitCount() const { return TurnOrder.Num(); }

    // Initialize turn order.
    // 턴 순서를 초기화합니다.
    void InitializeTurnOrder(const TArray<AUnitBase*>& Units);

    // Start the current turn.
    // 현재 턴을 시작합니다.
    void StartTurn();

    // End the current turn and proceed to the next turn.
    // 현재 턴을 종료하고 다음 턴으로 진행합니다.
    void EndTurn();

    // Find and switch to the next unit.
    // 다음 유닛을 찾아 턴을 전환합니다.
    void NextTurn();

    // Get the current turn unit.
    // 현재 턴 유닛을 반환합니다.
    AUnitBase* GetCurrentUnit() const;

    // Check combat end condition by remaining teams.
    // 남은 유닛의 팀 기준으로 전투 종료 조건을 확인합니다.
    bool CheckCombatEnd() const;

    // Get current turn index used by CombatManager replication.
    // CombatManager 복제에 사용되는 현재 턴 인덱스를 반환합니다.
    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }

    // Get current turn counter used by CombatManager replication.
    // CombatManager 복제에 사용되는 현재 턴 카운터를 반환합니다.
    int32 GetTurnCounter() const { return TurnCounter; }

    // Get current turn unit name used for HUD updates.
    // HUD 갱신에 사용할 현재 턴 유닛 이름을 반환합니다.
    FString GetCurrentUnitName() const;

private:
    bool bCombatActive = false;
    ECombatResult CombatResult = ECombatResult::None;

    // Server-only turn order array.
    // 서버에서만 사용하는 턴 순서 배열입니다.
    UPROPERTY()
    TArray<AUnitBase*> TurnOrder;

    // Internal turn index.
    // 외부에서 직접 수정하지 않는 내부 턴 인덱스입니다.
    int32 CurrentTurnIndex = 0;

    // Turn progression counter.
    // 턴 진행 횟수 카운터입니다.
    int32 TurnCounter = 0;

};
