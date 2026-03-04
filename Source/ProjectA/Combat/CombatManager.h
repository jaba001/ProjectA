#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatManager.generated.h"

class UTurnManager;
class AUnitBase;

UCLASS()
class PROJECTA_API ACombatManager : public AActor
{
    GENERATED_BODY()

public:
    ACombatManager();

protected:
    virtual void BeginPlay() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

    // 서버 전용 턴 매니저
    UPROPERTY()
    UTurnManager* TurnManager;

    // 현재 턴 인덱스 (클라 동기화용)
    UPROPERTY(Replicated)
    int32 CurrentTurnIndex;

public:

    // 전투 시작 요청 (클라 → 서버)
    UFUNCTION(Server, Reliable)
    void Server_StartCombat();

    // 전투 시작 실제 실행 (서버 전용)
    void StartCombat_Internal();

    // 현재 턴 인덱스 Getter
    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }
};