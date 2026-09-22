#pragma once

#include "CoreMinimal.h"
#include "Combat/AI/PartyControlTypes.h"
#include "Unit/UnitBase.h"
#include "PlayerUnit.generated.h"

class ACombatManager;
class UCombatActionAuthority;

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
    // Authored weapon skills identify the geometry whose visibility follows the saved loadout.
    // 작성된 무기 스킬로 저장 장착에 따라 표시할 무기 지오메트리를 지정합니다.
    UPROPERTY(EditDefaultsOnly, Category = "UnitBase|Skill")
    TArray<TSoftObjectPtr<USkillDefinitionDataAsset>> WeaponPresentationSkills;

    virtual void RefreshSkillPresentation() override;

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

    // Reject callbacks from an earlier AI assignment; this nonce is neither saved nor replicated.
    // 이전 AI 배정의 콜백을 거절하며 이 식별자는 저장하거나 복제하지 않습니다.
    FGuid AIControlSessionId;
};
