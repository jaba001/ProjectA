#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Combat/Commands/CombatActionTypes.h"
#include "Game/Run/RunIdentityTypes.h"
#include "PartyPlayerController.generated.h"

class AUnitBase;
class ACombatManager;
class ACombatGridTile;
class UUserWidget;
class USkillDefinitionDataAsset;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatActionResponse, const FCombatActionResponse&);

// Tile input mode selected by the player controller.
// 플레이어 컨트롤러에서 선택한 타일 입력 모드입니다.
UENUM(BlueprintType)
enum class ETileInputMode : uint8
{
    None,
    Skill,
    Move,
    Item
};

// Player controller that bridges combat UI input and combat actions.
// 전투 UI 입력과 전투 행동을 연결하는 플레이어 컨트롤러입니다.
UCLASS()
class PROJECTA_API APartyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    // Sets controller defaults for combat input.
    // 전투 입력을 위한 컨트롤러 기본값을 설정합니다.
    APartyPlayerController();

protected:
    // Finds required combat actors and initializes HUD.
    // 필요한 전투 액터를 찾고 HUD를 초기화합니다.
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual bool ShouldCreateCombatHUD() const { return true; }

private:
    // Finds and caches the combat manager.
    // 전투 매니저를 찾아 캐시합니다.
    void InitializeCombatManager();

    // Creates and stores the combat HUD widget.
    // 전투 HUD 위젯을 생성하고 보관합니다.
    void InitializeHUD();

public:
    void SetCombatContext(ACombatManager* InManager, bool bEnableInput);
    // Returns the currently active combat unit.
    // 현재 활성화된 전투 유닛을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    AUnitBase* GetActiveUnit() const;

    // Requests the active unit's turn end.
    // 활성 유닛의 턴 종료를 요청합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RequestEndTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RequestHealingItem();

    // The owning connection sends intent only; server bindings determine the requesting participant.
    // 소유 연결은 행동 의도만 전송하며 요청 참가자는 서버의 바인딩으로 결정합니다.
    UFUNCTION(Server, Reliable)
    void ServerRequestCombatAction(const FCombatActionRequest& Request);

    UFUNCTION(Client, Reliable)
    void ClientReceiveCombatActionResponse(const FCombatActionResponse& Response);

    bool BuildCombatActionRequest(ECombatActionKind Kind, USkillDefinitionDataAsset* Skill, ACombatGridTile* TargetTile, FCombatActionRequest& OutRequest);
    FCombatActionResponse SubmitCombatActionRequest(const FCombatActionRequest& Request);
    const FCombatActionResponse& GetLastCombatActionResponse() const { return LastCombatActionResponse; }
    bool IsCombatInputEnabled() const { return bCombatInputEnabled; }
    void SetCombatParticipantBinding(const FRunAccountId& AccountId, FGuid BindingId);
    const FRunAccountId& GetBoundParticipantAccount() const { return BoundParticipantAccount; }
    FGuid GetParticipantBindingId() const { return ParticipantBindingId; }
    FOnCombatActionResponse OnCombatActionResponse;

    // Own all player tile commands; tile actors only forward input.
    // 플레이어 타일 명령을 전담하며 타일 액터는 입력만 전달합니다.
    void HandleTileClicked(ACombatGridTile* Tile);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatManager* GetCombatManager() const { return CombatManager; }

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanUseActiveUnitAction() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanUseActiveUnitActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanUseActiveUnitSubActionPoint(int32 Cost) const;

public:
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetSelectedTile(ACombatGridTile* InTile);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ACombatGridTile* GetSelectedTile() const;

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void ClearSelectedTile();

public:
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetTileInputMode(ETileInputMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void EnterMoveMode();

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void EnterSkillMode(USkillDefinitionDataAsset* SkillData);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void CancelTileInputMode();

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ETileInputMode GetTileInputMode() const { return CurrentTileInputMode; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsMoveInputMode() const { return CurrentTileInputMode == ETileInputMode::Move; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsSkillInputMode() const { return CurrentTileInputMode == ETileInputMode::Skill; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    USkillDefinitionDataAsset* GetPendingSkillData() const { return PendingSkillData; }

    bool IsValidTileForPendingSkill(ACombatGridTile* Tile) const;

private:
    UFUNCTION()
    void OnRep_CombatContext();

    void HandleCombatViewChanged();
    TWeakObjectPtr<ACombatManager> ObservedCombatManager;
    FGuid ObservedCombatId;
    int32 ObservedTurnSerial = 0;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    FRunAccountId BoundParticipantAccount;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    FGuid ParticipantBindingId;

    void HandleCombatActionResponse(const FCombatActionResponse& Response);
    bool IsEquippedInputSkill(USkillDefinitionDataAsset* Skill) const;
    FGuid RequestCombatInstanceId;
    int64 NextRequestSequence = 0;
    FGuid LatestSubmittedCombatId;
    int64 LatestSubmittedSequence = 0;
    int64 LastHandledResponseSequence = 0;
    uint64 SelectionRevision = 0;
    uint64 SubmittedSelectionRevision = 0;

    UPROPERTY(Transient)
    FCombatActionResponse LastCombatActionResponse;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    bool bCombatInputEnabled = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (AllowPrivateAccess = "true"))
    ACombatGridTile* SelectedTile = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (AllowPrivateAccess = "true"))
    ETileInputMode CurrentTileInputMode = ETileInputMode::None;

    // Skill definition data currently pending in skill input mode
    UPROPERTY()
    TObjectPtr<USkillDefinitionDataAsset> PendingSkillData = nullptr;

private:
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    UUserWidget* HUDWidget = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_CombatContext)
    ACombatManager* CombatManager = nullptr;
};
