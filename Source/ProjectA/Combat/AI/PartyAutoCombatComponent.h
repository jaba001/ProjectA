#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Commands/CombatActionTypes.h"
#include "PartyAutoCombatComponent.generated.h"

class ACombatManager;
enum class EUnitActionType : uint8;
enum class EUnitActionResult : uint8;

// Retains serialized component references without the removed sequential AI loop.
// 제거된 순차 AI 루프 없이 직렬화된 컴포넌트 참조만 유지합니다.
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
    bool IsWaitingForAction() const { return false; }

private:
    FCombatActionRequest LastDecision;
};
