#pragma once

#include "CoreMinimal.h"
#include "Controller/PartyPlayerController.h"
#include "GameplayPlayerController.generated.h"

class AEncounterManager;
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
    void RefreshRunFlowPermissions();
    virtual bool IsRoundInputEnabled() const override;
    FGuid GetInventoryCharacterId(const FGameplayViewState& View) const;

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void RequestStartNode(FName NodeId);

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void RequestContinueRun();
    void RequestSelectRunEncounter(FName EncounterId);
    void RequestLeaveRunEncounter();
    void RequestPurchaseShopOffer(FGuid CharacterId, FName OfferId);
    FGuid GetShopBuyerCharacterId(const FGameplayViewState& View) const;
    const FText& GetShopPurchaseMessage() const { return ShopPurchaseMessage; }
    bool IsShopPurchasePending() const { return bShopPurchasePending; }

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

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
    TSubclassOf<UGameplayRootWidget> GameplayRootWidgetClass;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FEncounterPreparationRetryTest;
#endif

    void RefreshGameplayFlow();
    void ExecuteShopPurchase(FGuid CharacterId, FName OfferId);
    void ExecuteGoldRewardSelection(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex);

    UFUNCTION(Server, Reliable)
    void ServerSelectGoldReward(FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex);

    UFUNCTION(Client, Reliable)
    void ClientReceiveGoldRewardResult(FGuid CharacterId, FName ExpectedNodeId, bool bSucceeded, const FText& Message);

    UFUNCTION(Server, Reliable)
    void ServerPurchaseShopOffer(FGuid CharacterId, FName OfferId);

    UFUNCTION(Client, Reliable)
    void ClientReceiveShopPurchaseResult(bool bSucceeded, const FText& Message);

    UFUNCTION()
    void OnRep_RunParticipantAccount();

    // Presentation binding survives combat cleanup; purchase authority is resolved again on the server.
    // 표시용 연결은 전투 정리 이후에도 유지하며 구매 권한은 서버에서 다시 조회합니다.
    UPROPERTY(ReplicatedUsing = OnRep_RunParticipantAccount)
    FRunAccountId RunParticipantAccount;

    FText ShopPurchaseMessage;
    bool bShopPurchasePending = false;
    FText RewardSelectionMessage;
    FGuid PendingRewardCharacterId;
    FName PendingRewardNodeId;
    bool bRewardSelectionPending = false;
    bool bAwaitingRewardReplication = false;
    bool CanRetryGameplayRecovery() const;
    void TryBindGameplayState();
    FTimerHandle BindStateTimer;

    UPROPERTY(Transient)
    TObjectPtr<AGameplayGameState> GameplayState;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayRootWidget> GameplayRootWidget;

    UPROPERTY(Transient)
    TObjectPtr<URunStateSubsystem> RunState;

    UPROPERTY(Transient)
    TObjectPtr<AEncounterManager> EncounterManager;
};
