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
    // 플레이어 유닛 클래스들
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // 적 유닛 클래스들
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

private:
    // 서버 전용 턴 매니저
    UPROPERTY()
    UTurnManager* TurnManager;

    // 전투 유닛 목록 (서버 소유)
    UPROPERTY()
    TArray<AUnitBase*> CombatUnits;

    // 현재 턴 인덱스 (클라 동기화용)
    UPROPERTY(Replicated)
    int32 CurrentTurnIndex;

public:
	// 현재 턴 유닛
    UFUNCTION(BlueprintCallable)
    UTurnManager* GetTurnManager() const { return TurnManager; }

    UFUNCTION(Server, Reliable)
    void Server_StartCombat();

    void StartCombat_Internal();

    // 턴 진행 요청 (서버 전용)
    void AdvanceTurn();

    // 유닛 등록 (서버 전용)
    void RegisterUnits(const TArray<AUnitBase*>& Units);

    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }

    UFUNCTION(BlueprintCallable)
    AUnitBase* GetCurrentUnit() const;

    UFUNCTION(BlueprintCallable)
    void RequestEndTurn();
};