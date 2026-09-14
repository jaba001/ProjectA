#include "Controller/PartyPlayerController.h"

#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Net/UnrealNetwork.h"

namespace
{
    FCombatActionResponse RejectLegacyAction(const FCombatActionRequest& Request)
    {
        FCombatActionResponse Response;
        Response.CombatInstanceId = Request.CombatInstanceId;
        Response.RequestSequence = Request.RequestSequence;
        Response.Result = ECombatRequestResult::InvalidContext;
        Response.Message = FText::FromString(TEXT("개별 턴 즉시 실행은 종료되었습니다. 라운드 계획을 적용한 뒤 준비 완료를 사용하세요."));
        return Response;
    }
}

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

AUnitBase* APartyPlayerController::GetActiveUnit() const
{
    return nullptr;
}

void APartyPlayerController::RequestEndTurn()
{
    SubmitCombatActionRequest(FCombatActionRequest());
}

void APartyPlayerController::RequestHealingItem()
{
    SubmitCombatActionRequest(FCombatActionRequest());
}

bool APartyPlayerController::BuildCombatActionRequest(ECombatActionKind Kind, USkillDefinitionDataAsset* Skill, ACombatGridTile* TargetTile, FCombatActionRequest& OutRequest)
{
    OutRequest = FCombatActionRequest();
    OutRequest.Kind = Kind;
    return false;
}

FCombatActionResponse APartyPlayerController::SubmitCombatActionRequest(const FCombatActionRequest& Request)
{
    LastCombatActionResponse = RejectLegacyAction(Request);
    OnCombatActionResponse.Broadcast(LastCombatActionResponse);
    return LastCombatActionResponse;
}

void APartyPlayerController::ServerRequestCombatAction_Implementation(const FCombatActionRequest& Request)
{
    ClientReceiveCombatActionResponse(RejectLegacyAction(Request));
}

void APartyPlayerController::ClientReceiveCombatActionResponse_Implementation(const FCombatActionResponse& Response)
{
    LastCombatActionResponse = Response;
    OnCombatActionResponse.Broadcast(Response);
}

void APartyPlayerController::HandleTileClicked(ACombatGridTile* Tile)
{
    SetSelectedTile(Tile);
}

bool APartyPlayerController::CanUseActiveUnitAction() const
{
    return false;
}

bool APartyPlayerController::CanUseActiveUnitActionPoint(int32 Cost) const
{
    return false;
}

bool APartyPlayerController::CanUseActiveUnitSubActionPoint(int32 Cost) const
{
    return false;
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

void APartyPlayerController::SetTileInputMode(ETileInputMode NewMode)
{
    CancelTileInputMode();
}

void APartyPlayerController::EnterMoveMode()
{
    CancelTileInputMode();
}

void APartyPlayerController::EnterSkillMode(USkillDefinitionDataAsset* SkillData)
{
    CancelTileInputMode();
}

void APartyPlayerController::CancelTileInputMode()
{
    ClearSelectedTile();
}

bool APartyPlayerController::IsValidTileForPendingSkill(ACombatGridTile* Tile) const
{
    return false;
}
