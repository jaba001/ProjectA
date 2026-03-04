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
    // 턴 순서 초기화
    void InitializeTurnOrder(const TArray<AUnitBase*>& Units);

    // 현재 턴 시작/종료
    void StartTurn();
    void EndTurn();
    void NextTurn();

    // 현재 턴 유닛 가져오기
    AUnitBase* GetCurrentUnit() const;

private:
    UPROPERTY()
    TArray<AUnitBase*> TurnOrder;

    int32 CurrentTurnIndex = 0;
};
