#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "CombatRoundPlayerController.generated.h"

class ACombatRoundCoordinator;
class ACombatGridTile;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundWorldUnitClicked, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundWorldTileClicked, FIntPoint);

// Owns round planning requests while Gameplay retains Run progression and screen lifetime.
// Gameplay가 Run 진행과 화면 수명을 유지하는 동안 라운드 계획 요청을 소유합니다.
UCLASS()
class PROJECTA_API ACombatRoundPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACombatRoundPlayerController();
    virtual void PlayerTick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void SetRoundSession(ACombatRoundCoordinator* InCoordinator, int32 InSlot);
    ACombatRoundCoordinator* GetRoundCoordinator() const { return Coordinator; }
    int32 GetRoundParticipantSlot() const { return ParticipantSlot; }
    const FText& GetRoundRequestStatus() const { return RequestStatus; }
    bool IsRoundRequestPending() const;
    virtual bool IsRoundInputEnabled() const { return true; }
    void SubmitRoundPlan(const FCombatRoundCommand& Command);
    void SubmitRoundMove(int32 UnitId, FIntPoint Destination);
    void SetRoundReady(bool bReady);
    FOnRoundWorldUnitClicked OnRoundWorldUnitClicked;
    FOnRoundWorldTileClicked OnRoundWorldTileClicked;
    bool HandleRoundWorldTileClicked(ACombatGridTile* Tile);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRoundResponse(bool bAccepted, const FText& Message);

protected:
    virtual void SetupInputComponent() override;

private:
    bool CanSelectRoundWorldTarget() const;
    bool IsCursorOverGameViewport() const;
    void HandleRoundWorldClick();

    UFUNCTION(Server, Reliable)
    void ServerSubmitRoundPlan(FGuid CombatId, int32 RoundNumber, int32 Revision, FCombatRoundCommand Command);

    UFUNCTION(Server, Reliable)
    void ServerSubmitRoundMove(FGuid CombatId, int32 RoundNumber, int32 Revision, int32 UnitId, FIntPoint Destination);

    UFUNCTION(Server, Reliable)
    void ServerSetRoundReady(FGuid CombatId, int32 RoundNumber, int32 Revision, bool bReady);

    UFUNCTION()
    void OnRep_RoundSession();

    UPROPERTY(ReplicatedUsing = OnRep_RoundSession)
    TObjectPtr<ACombatRoundCoordinator> Coordinator;

    UPROPERTY(Replicated)
    int32 ParticipantSlot = 0;

    FText RequestStatus;
    FGuid RequestedCombatId;
    int32 RequestedRound = 0;
    int32 RequestedRevision = 0;
    bool bRequestPending = false;
    bool bAwaitingReplicatedResult = false;
    bool bCameraInitialized = false;
};
