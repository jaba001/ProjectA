#include "Controller/PartyPlayerController.h"

#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Net/UnrealNetwork.h"

APartyPlayerController::APartyPlayerController()
{
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
    bShowMouseCursor = true;
}

void APartyPlayerController::SetCombatContext(ACombatManager* InManager, bool bEnableInput)
{
    if (!HasAuthority()) return;
    CombatManager = InManager;
    bCombatInputEnabled = bEnableInput;
    OnRep_CombatContext();
    ForceNetUpdate();
}

void APartyPlayerController::SetCombatParticipantBinding(const FRunAccountId& AccountId, FGuid BindingId)
{
    if (!HasAuthority()) return;
    BoundParticipantAccount = AccountId;
    ParticipantBindingId = BindingId;
    OnRep_CombatContext();
    ForceNetUpdate();
}

void APartyPlayerController::OnRep_CombatContext()
{
    if (ObservedCombatManager.Get() != CombatManager)
    {
        if (ObservedCombatManager.IsValid()) ObservedCombatManager->OnCombatViewChanged.RemoveAll(this);
        ObservedCombatManager = CombatManager;
        if (CombatManager) CombatManager->OnCombatViewChanged.AddUObject(this, &APartyPlayerController::HandleCombatViewChanged);
    }
    HandleCombatViewChanged();
}

void APartyPlayerController::HandleCombatViewChanged()
{
    CancelTileInputMode();
    if (!HasAuthority()) return;
    ACombatRoundCoordinator* Round = CombatManager ? CombatManager->GetRoundCoordinator() : nullptr;
    SetRoundSession(Round, Round ? Round->GetParticipantSlot(this) : 0);
}

void APartyPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(APartyPlayerController, CombatManager, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(APartyPlayerController, bCombatInputEnabled, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(APartyPlayerController, BoundParticipantAccount, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(APartyPlayerController, ParticipantBindingId, COND_OwnerOnly);
}

void APartyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ObservedCombatManager.IsValid()) ObservedCombatManager->OnCombatViewChanged.RemoveAll(this);
    Super::EndPlay(EndPlayReason);
}

void APartyPlayerController::HandleTileClicked(ACombatGridTile* Tile)
{
    if (HandleRoundWorldTileClicked(Tile)) SetSelectedTile(Tile);
}

void APartyPlayerController::SetSelectedTile(ACombatGridTile* InTile)
{
    SelectedTile = IsValid(InTile) && InTile->GetWorld() == GetWorld() ? InTile : nullptr;
}

ACombatGridTile* APartyPlayerController::GetSelectedTile() const
{
    return SelectedTile;
}

void APartyPlayerController::ClearSelectedTile()
{
    SelectedTile = nullptr;
}

void APartyPlayerController::CancelTileInputMode()
{
    ClearSelectedTile();
}
