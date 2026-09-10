#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Commands/CombatActionTypes.h"
#include "PartyAutoCombatComponent.generated.h"

class ACombatManager;
class ACombatGridTile;
class APlayerUnit;
enum class EUnitActionType : uint8;
enum class EUnitActionResult : uint8;

// Trusted server AI submits the same value commands while the original character owner remains unchanged.
// 신뢰된 서버 AI는 원래 캐릭터 소유자를 유지하면서 같은 값 기반 명령을 제출합니다.
UCLASS()
class PROJECTA_API UPartyAutoCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPartyAutoCombatComponent();
    void InitializeCombat(ACombatManager* InCombatManager);
    void StartTurn();
    void Stop();
    void HandleActionCompleted(EUnitActionType ActionType, EUnitActionResult Result);
    const FCombatActionRequest& GetLastDecision() const { return LastDecision; }
    bool IsWaitingForAction() const { return bWaitingForAction; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FTurnContext
    {
        FGuid CombatId;
        FGuid ControlSessionId;
        int32 TurnSerial = 0;
        uint64 Generation = 0;
    };

    APlayerUnit* GetPlayerUnit() const;
    bool MatchesCurrentContext(const FTurnContext& Context) const;
    void ScheduleDecision(bool bFinishTurn);
    void DecideAction();
    bool ChooseHealingItem(FCombatActionRequest& Request) const;
    bool ChooseSkill(FCombatActionRequest& Request) const;
    bool ChooseMove(FCombatActionRequest& Request) const;
    TArray<ACombatGridTile*> GetSortedCombatTiles() const;
    void SubmitDecision(FCombatActionRequest Request);

    TWeakObjectPtr<ACombatManager> CombatManager;
    FTimerHandle DecisionTimer;
    FTurnContext ActiveTurn;
    FTurnContext PendingActionContext;
    FCombatActionRequest LastDecision;
    uint64 Generation = 0;
    int64 RequestSequence = 0;
    EUnitActionType PendingActionType;
    bool bWaitingForAction = false;
    bool bFinishTurnPending = false;
    bool bEndTurnRequested = false;
};
