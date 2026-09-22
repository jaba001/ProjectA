#pragma once

#include "CoreMinimal.h"
#include "Controller/CombatRoundPlayerController.h"
#include "Combat/Commands/CombatActionTypes.h"
#include "Game/Run/RunIdentityTypes.h"
#include "PartyPlayerController.generated.h"

class AUnitBase;
class ACombatManager;
class ACombatGridTile;
class UUserWidget;
class USkillDefinitionDataAsset;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatActionResponse, const FCombatActionResponse&);

// Retained for serialized Blueprint references; tile clicks no longer execute actions.
// 직렬화된 Blueprint 참조를 위해 보존하며 타일 클릭은 더 이상 행동을 실행하지 않습니다.
UENUM(BlueprintType)
enum class ETileInputMode : uint8
{
    None,
    Skill,
    Move,
    Item
};

// Preserves party identity and Run authorization while inheriting round planning requests.
// 라운드 계획 요청을 상속하면서 파티 식별자와 Run 권한을 유지합니다.
UCLASS()
class PROJECTA_API APartyPlayerController : public ACombatRoundPlayerController
{
    GENERATED_BODY()

public:
    APartyPlayerController();
    void SetCombatContext(ACombatManager* InManager, bool bEnableInput);
    void SetCombatParticipantBinding(const FRunAccountId& AccountId, FGuid BindingId);
    const FRunAccountId& GetBoundParticipantAccount() const { return BoundParticipantAccount; }
    FGuid GetParticipantBindingId() const { return ParticipantBindingId; }
    bool IsCombatInputEnabled() const { return bCombatInputEnabled; }
    virtual bool IsRoundInputEnabled() const override { return bCombatInputEnabled; }
    FOnCombatActionResponse OnCombatActionResponse;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatManager* GetCombatManager() const { return CombatManager; }

    // Legacy serialized calls reject execution instead of bypassing the round lock.
    // 기존 직렬화 호출은 라운드 잠금을 우회하지 않고 실행을 거부합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Select a unit in round planning."))
    AUnitBase* GetActiveUnit() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round readiness."))
    void RequestEndTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use a supported round skill."))
    void RequestHealingItem();

    UFUNCTION(Server, Reliable)
    void ServerRequestCombatAction(const FCombatActionRequest& Request);

    UFUNCTION(Client, Reliable)
    void ClientReceiveCombatActionResponse(const FCombatActionResponse& Response);

    bool BuildCombatActionRequest(ECombatActionKind Kind, USkillDefinitionDataAsset* Skill, ACombatGridTile* TargetTile, FCombatActionRequest& OutRequest);
    FCombatActionResponse SubmitCombatActionRequest(const FCombatActionRequest& Request);
    const FCombatActionResponse& GetLastCombatActionResponse() const { return LastCombatActionResponse; }
    void HandleTileClicked(ACombatGridTile* Tile);

    UFUNCTION(BlueprintCallable, Category = "Combat|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round planning; this entry point only preserves legacy asset compatibility."))
    bool CanUseActiveUnitAction() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round planning; this entry point only preserves legacy asset compatibility."))
    bool CanUseActiveUnitActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round planning; this entry point only preserves legacy asset compatibility."))
    bool CanUseActiveUnitSubActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetSelectedTile(ACombatGridTile* InTile);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ACombatGridTile* GetSelectedTile() const;

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void ClearSelectedTile();

    UFUNCTION(BlueprintCallable, Category = "Tile|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round planning; this entry point only preserves legacy asset compatibility."))
    void SetTileInputMode(ETileInputMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Tile|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round planning; this entry point only preserves legacy asset compatibility."))
    void EnterMoveMode();

    UFUNCTION(BlueprintCallable, Category = "Tile|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use round planning; this entry point only preserves legacy asset compatibility."))
    void EnterSkillMode(USkillDefinitionDataAsset* SkillData);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void CancelTileInputMode();

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ETileInputMode GetTileInputMode() const { return ETileInputMode::None; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsMoveInputMode() const { return false; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsSkillInputMode() const { return false; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    USkillDefinitionDataAsset* GetPendingSkillData() const { return nullptr; }

    bool IsValidTileForPendingSkill(ACombatGridTile* Tile) const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UFUNCTION()
    void OnRep_CombatContext();

    void HandleCombatViewChanged();
    TWeakObjectPtr<ACombatManager> ObservedCombatManager;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    FRunAccountId BoundParticipantAccount;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    FGuid ParticipantBindingId;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    bool bCombatInputEnabled = false;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    TObjectPtr<ACombatManager> CombatManager;

    UPROPERTY(Transient)
    TObjectPtr<ACombatGridTile> SelectedTile;

    UPROPERTY(Transient)
    FCombatActionResponse LastCombatActionResponse;

    // Preserve the authored property without instantiating the old immediate-action HUD.
    // 기존 즉시 행동 HUD를 생성하지 않고 작성된 속성만 보존합니다.
    UPROPERTY(meta = (DeprecatedProperty))
    TSubclassOf<UUserWidget> HUDWidgetClass;
};
