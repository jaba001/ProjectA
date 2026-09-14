#include "Controller/CombatRoundPlayerController.h"

#include "Combat/Round/CombatRoundCoordinator.h"
#include "Game/Encounter/CombatArena.h"
#include "Net/UnrealNetwork.h"

ACombatRoundPlayerController::ACombatRoundPlayerController()
{
    bShowMouseCursor = true;
    bAutoManageActiveCameraTarget = false;
}

void ACombatRoundPlayerController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (IsLocalController() && !bCameraInitialized && IsValid(Coordinator) && IsValid(Coordinator->GetArena()))
    {
        Coordinator->GetArena()->ActivateArena(this);
        bCameraInitialized = true;
    }
}

void ACombatRoundPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ACombatRoundPlayerController, Coordinator, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ACombatRoundPlayerController, ParticipantSlot, COND_OwnerOnly);
}

void ACombatRoundPlayerController::SetRoundSession(ACombatRoundCoordinator* InCoordinator, int32 InSlot)
{
    if (!HasAuthority() || (Coordinator == InCoordinator && ParticipantSlot == InSlot)) return;
    Coordinator = InCoordinator;
    ParticipantSlot = InSlot;
    OnRep_RoundSession();
    ForceNetUpdate();
}

void ACombatRoundPlayerController::OnRep_RoundSession()
{
    bCameraInitialized = false;
    bRequestPending = false;
    bAwaitingReplicatedResult = false;
    RequestStatus = FText::GetEmpty();
}

bool ACombatRoundPlayerController::IsRoundRequestPending() const
{
    if (!bRequestPending) return false;
    if (!bAwaitingReplicatedResult) return true;
    if (!IsValid(Coordinator)) return false;
    const FCombatRoundView& View = Coordinator->GetView();
    return View.CombatId == RequestedCombatId && View.RoundNumber == RequestedRound && View.PlanRevision <= RequestedRevision;
}

void ACombatRoundPlayerController::SubmitRoundPlan(const FCombatRoundCommand& Command)
{
    if (!IsLocalController() || !IsRoundInputEnabled() || !IsValid(Coordinator) || IsRoundRequestPending()) return;
    const FCombatRoundView& View = Coordinator->GetView();
    RequestedCombatId = View.CombatId;
    RequestedRound = View.RoundNumber;
    RequestedRevision = View.PlanRevision;
    bRequestPending = true;
    bAwaitingReplicatedResult = false;
    RequestStatus = FText::FromString(TEXT("계획을 서버에서 확인하고 있습니다."));
    ServerSubmitRoundPlan(View.CombatId, View.RoundNumber, View.PlanRevision, Command);
}

void ACombatRoundPlayerController::SetRoundReady(bool bReady)
{
    if (!IsLocalController() || !IsRoundInputEnabled() || !IsValid(Coordinator) || IsRoundRequestPending()) return;
    const FCombatRoundView& View = Coordinator->GetView();
    RequestedCombatId = View.CombatId;
    RequestedRound = View.RoundNumber;
    RequestedRevision = View.PlanRevision;
    bRequestPending = true;
    bAwaitingReplicatedResult = false;
    RequestStatus = FText::FromString(TEXT("준비 상태를 서버에서 확인하고 있습니다."));
    ServerSetRoundReady(View.CombatId, View.RoundNumber, View.PlanRevision, bReady);
}

void ACombatRoundPlayerController::ServerSubmitRoundPlan_Implementation(FGuid CombatId, int32 RoundNumber, int32 Revision, FCombatRoundCommand Command)
{
    FText Error = FText::FromString(TEXT("라운드 세션에 연결되지 않았습니다."));
    const bool bAccepted = IsRoundInputEnabled() && IsValid(Coordinator) && Coordinator->SubmitPlan(this, CombatId, RoundNumber, Revision, Command, Error);
    ClientReceiveRoundResponse(bAccepted, bAccepted ? FText::FromString(TEXT("계획 적용 완료. 모든 아군 계획을 확인한 뒤 준비 완료를 누르세요.")) : Error);
}

void ACombatRoundPlayerController::ServerSetRoundReady_Implementation(FGuid CombatId, int32 RoundNumber, int32 Revision, bool bReady)
{
    FText Error = FText::FromString(TEXT("라운드 세션에 연결되지 않았습니다."));
    const bool bAccepted = IsRoundInputEnabled() && IsValid(Coordinator) && Coordinator->SetParticipantReady(this, CombatId, RoundNumber, Revision, bReady, Error);
    ClientReceiveRoundResponse(bAccepted, bAccepted ? FText::FromString(bReady ? TEXT("준비 완료를 반영했습니다.") : TEXT("준비 완료를 취소했습니다.")) : Error);
}

void ACombatRoundPlayerController::ClientReceiveRoundResponse_Implementation(bool bAccepted, const FText& Message)
{
    // Reliable replies and replicated actor state can arrive independently; wait for the accepted revision.
    // Reliable 응답과 액터 상태 복제는 독립적으로 도착할 수 있으므로 승인된 수정 상태를 기다립니다.
    bRequestPending = bAccepted;
    bAwaitingReplicatedResult = bAccepted;
    RequestStatus = bAccepted ? Message : FText::Format(NSLOCTEXT("CombatRound", "Rejected", "요청 불가: {0}"), Message);
}
