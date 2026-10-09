#pragma once

#include "CoreMinimal.h"
#include "Controller/PartyPlayerController.h"
#include "Game/Run/RunEquipmentTypes.h"
#include "Game/Run/RunTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayPlayerController.generated.h"

class AEncounterManager;
class AEncounterPrototypeStage;
class AEncounterDungeonRoute;
class UGameplayRootWidget;
class URunStateSubsystem;
class AGameplayGameState;
struct FGameplayViewState;

// Reuses grid input while owning only gameplay UI and input routing.
// 그리드 입력을 재사용하고 Gameplay UI와 입력 전환만 소유합니다.
UCLASS()
class PROJECTA_API AGameplayPlayerController : public APartyPlayerController
{
    GENERATED_BODY()

public:
    AGameplayPlayerController();
    void InitializeGameplay(AEncounterManager* InEncounterManager);
    bool CanIssueRunCommands() const;
    bool IsEncounterPresentationTransitioning() const { return bEncounterPresentationTransition; }
    int32 GetResidentDungeonRouteCount() const { return (DungeonRoute.IsValid() ? 1 : 0) + (PreparedDungeonRoute.IsValid() ? 1 : 0); }
    int32 GetPreparedDungeonVisitIndex() const { return PreparedDungeonVisitIndex; }
    void RefreshRunFlowPermissions();
    virtual bool IsRoundInputEnabled() const override;
    FGuid GetInventoryCharacterId(const FGameplayViewState& View) const;

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void RequestStartNode(FName NodeId, FGameplayTag DifficultyTag = FGameplayTag());

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void RequestContinueRun();
    void RequestSelectRunEncounter(FName EncounterId);
    void RequestLeaveRunEncounter();
    void RequestToggleInventory();
    void RequestPurchaseShopOffer(FGuid CharacterId, FName OfferId, int32 ExpectedShopRevision = INDEX_NONE);
    FGuid GetShopBuyerCharacterId(const FGameplayViewState& View) const;
    const FText& GetShopPurchaseMessage() const { return ShopPurchaseMessage; }
    bool IsShopPurchasePending() const { return bShopPurchasePending; }
    void RequestChangeEquipment(const FRunEquipmentCommand& Command);
    bool CanChangeEquipment(const FGameplayViewState& View, FGuid CharacterId) const;
    bool IsEquipmentChangePending() const { return bEquipmentChangePending; }
    const FText& GetEquipmentMessage() const { return EquipmentMessage; }

    void RequestSelectGoldReward(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex);
    FGuid GetRewardCharacterId(const FGameplayViewState& View) const;
    const FText& GetRewardSelectionMessage() const { return RewardSelectionMessage; }
    bool IsRewardSelectionPending() const { return bRewardSelectionPending; }

    void RequestRetryCombatCheckpoint();
    UFUNCTION(Server, Reliable)
    void ServerSetDevelopmentReady(bool bReady);
    void RequestStartDevelopmentCoop();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool CanActivateRoundCamera() const override;

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
    TSubclassOf<UGameplayRootWidget> GameplayRootWidgetClass;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FEncounterPreparationRetryTest;
#endif

    void RefreshGameplayFlow();
    void RefreshEncounterPresentation(const FGameplayViewState& View);
    void ResetEncounterPresentation();
    void FinishEncounterPresentation(uint32 Generation);
    bool PrepareDungeonRoutes(const FGameplayViewState& View);
    void ExecuteShopPurchase(FGuid CharacterId, FName OfferId, int32 ExpectedShopRevision);
    void ExecuteEquipmentChange(const FRunEquipmentCommand& Command);
    void ExecuteGoldRewardSelection(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex);

    UFUNCTION(Server, Reliable)
    void ServerSelectGoldReward(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex);

    UFUNCTION(Client, Reliable)
    void ClientReceiveGoldRewardResult(FGuid CharacterId, FName ExpectedNodeId, bool bSucceeded, const FText& Message);

    UFUNCTION(Server, Reliable)
    void ServerPurchaseShopOffer(FGuid CharacterId, FName OfferId, int32 ExpectedShopRevision);

    UFUNCTION(Client, Reliable)
    void ClientReceiveShopPurchaseResult(bool bSucceeded, const FText& Message, int32 ConfirmedShopRevision);

    UFUNCTION(Server, Reliable)
    void ServerChangeEquipment(const FRunEquipmentCommand& Command);

    UFUNCTION(Client, Reliable)
    void ClientReceiveEquipmentResult(FGuid CharacterId, bool bSucceeded, const FText& Message, int32 ConfirmedRevision);

    UFUNCTION()
    void OnRep_RunParticipantAccount();

    // Presentation binding survives combat cleanup; purchase authority is resolved again on the server.
    // 표시용 연결은 전투 정리 이후에도 유지하며 구매 권한은 서버에서 다시 조회합니다.
    UPROPERTY(ReplicatedUsing = OnRep_RunParticipantAccount)
    FRunAccountId RunParticipantAccount;

    FText ShopPurchaseMessage;
    bool bShopPurchasePending = false;
    bool bPendingItemShop = false;
    bool bPendingService = false;
    int32 PendingShopRevision = INDEX_NONE;
    FText EquipmentMessage;
    bool bEquipmentChangePending = false;
    FGuid PendingEquipmentCharacterId;
    int32 PendingEquipmentRevision = INDEX_NONE;
    FText RewardSelectionMessage;
    FGuid PendingRewardCharacterId;
    FName PendingRewardNodeId;
    bool bRewardSelectionPending = false;
    bool bAwaitingRewardReplication = false;
    bool CanRetryGameplayRecovery() const;
    void TryBindGameplayState();
    FTimerHandle BindStateTimer;
    FTimerHandle EncounterPresentationTimer;
    TWeakObjectPtr<AEncounterPrototypeStage> PresentedStage;
    TWeakObjectPtr<AEncounterDungeonRoute> DungeonRoute;
    TWeakObjectPtr<AEncounterDungeonRoute> PreparedDungeonRoute;
    int32 PreparedDungeonVisitIndex = INDEX_NONE;
    int32 CachedDungeonSeed = 0;
    int32 CachedDungeonVersion = 0;
    int32 PresentedDungeonSeed = 0;
    int32 PresentedDungeonVersion = 0;
    TWeakObjectPtr<AActor> PresentationViewTarget;
    TArray<FName> PresentedOfferIds;
    ERunPhase PresentationPhase = ERunPhase::None;
    FName PresentedEncounterId;
    int32 PresentedCompletedCount = INDEX_NONE;
    int32 PresentedVisitIndex = INDEX_NONE;
    uint32 PresentationGeneration = 0;
    bool bWorldEncounterPresentation = false;
    bool bEncounterPresentationTransition = false;
    bool bPresentationEnding = false;

    UPROPERTY(Transient)
    TObjectPtr<AGameplayGameState> GameplayState;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayRootWidget> GameplayRootWidget;

    UPROPERTY(Transient)
    TObjectPtr<URunStateSubsystem> RunState;

    UPROPERTY(Transient)
    TObjectPtr<AEncounterManager> EncounterManager;
};
