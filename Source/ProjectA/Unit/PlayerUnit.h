#pragma once

#include "CoreMinimal.h"
#include "Combat/AI/PartyControlTypes.h"
#include "Unit/UnitBase.h"
#include "PlayerUnit.generated.h"

class ACombatManager;
class UCombatActionAuthority;
class UPartyAutoCombatComponent;

// Player-controlled combat unit implementation.
// 플레이어가 조작하는 전투 유닛 구현 클래스입니다.
UCLASS()
class PROJECTA_API APlayerUnit : public AUnitBase
{
	GENERATED_BODY()

public:
	// Sets player unit defaults.
	// 플레이어 유닛의 기본값을 설정합니다.
	APlayerUnit();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Unit|Control")
    EPartyControlMode GetPartyControlMode() const { return PartyControlMode; }

    UFUNCTION(BlueprintPure, Category = "Unit|Control")
    bool IsServerAIControlled() const { return PartyControlMode == EPartyControlMode::ServerAI; }

    FGuid GetAIControlSessionId() const { return AIControlSessionId; }
    void InitializeAutoCombat(ACombatManager* CombatManager);

public:
	// Handles player unit turn start behavior.
	// 플레이어 유닛의 턴 시작 동작을 처리합니다.
	virtual void OnTurnStart() override;

	// Handles player unit turn end behavior.
	// 플레이어 유닛의 턴 종료 동작을 처리합니다.
    virtual void OnTurnEnd() override;

protected:
    // End exhausted turns only after action cleanup and completion notifications.
    // 행동 정리와 완료 통지 후에만 자원이 소진된 턴을 종료합니다.
    virtual void OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    FTimerHandle ExhaustedTurnTimer;

private:
    friend class UCombatActionAuthority;
    friend class AUnitBase;

    // Only validated authority transitions and fresh checkpoint restoration can apply a mode.
    // 검증된 권위 전환과 새 체크포인트 복원만 조작 모드를 적용할 수 있습니다.
    bool ApplyPartyControlMode(EPartyControlMode Mode);

    UFUNCTION()
    void OnRep_PartyControlMode();

    UPROPERTY(ReplicatedUsing = OnRep_PartyControlMode)
    EPartyControlMode PartyControlMode = EPartyControlMode::Human;

    UPROPERTY(VisibleAnywhere, Category = "Unit|Control")
    TObjectPtr<UPartyAutoCombatComponent> AutoCombat;

    // Reject callbacks from an earlier AI assignment; this nonce is neither saved nor replicated.
    // 이전 AI 배정의 콜백을 거절하며 이 식별자는 저장하거나 복제하지 않습니다.
    FGuid AIControlSessionId;
};
