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

    // 현재 턴 시작
    void StartTurn();

    // 현재 턴 종료 후 다음 턴 진행
    void EndTurn();

    // 다음 유닛 탐색
    void NextTurn();

    // 현재 턴 유닛 반환
    AUnitBase* GetCurrentUnit() const;

	// 전투 종료 조건 체크 (모든 유닛이 같은 팀에 속해있는지)
    bool CheckCombatEnd() const;

    // 현재 턴 인덱스 반환 (CombatManager가 복제용으로 사용)
    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }

	// 현재 턴 카운터 반환 (CombatManager가 복제용으로 사용)
    int32 GetTurnCounter() const { return TurnCounter; }

	// 현재 턴 유닛 이름 반환 (HUD 업데이트용)
    FString GetCurrentUnitName() const;

private:

    // 서버 전용 턴 순서 배열 ,서버 전용임을 명시
    UPROPERTY()
    TArray<AUnitBase*> TurnOrder;

    // 서버 내부 턴 인덱스 ,외부 직접 수정 금지
    int32 CurrentTurnIndex = 0;

    int32 TurnCounter = 0;

};
