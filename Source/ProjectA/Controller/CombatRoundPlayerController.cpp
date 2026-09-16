#include "Controller/CombatRoundPlayerController.h"

#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/Encounter/CombatArena.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Layout/WidgetPath.h"
#include "Net/UnrealNetwork.h"
#include "Unit/UnitBase.h"
#include "Widgets/SViewport.h"

ACombatRoundPlayerController::ACombatRoundPlayerController()
{
    bShowMouseCursor = true;
    bAutoManageActiveCameraTarget = false;
}

void ACombatRoundPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (InputComponent) InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ACombatRoundPlayerController::HandleRoundWorldClick);
}

bool ACombatRoundPlayerController::CanSelectRoundWorldTarget() const
{
    if (!IsLocalController() || !IsRoundInputEnabled() || !IsValid(Coordinator) || ParticipantSlot <= 0 || IsRoundRequestPending()) return false;
    const FCombatRoundView& View = Coordinator->GetView();
    if (View.Phase != ECombatRoundPhase::Planning || Coordinator->IsPlanningMoveInProgress()) return false;
    return View.Units.ContainsByPredicate([this](const FCombatRoundUnitView& Unit)
    {
        return !Unit.bEnemy && Unit.OwnerSlot == ParticipantSlot && Unit.HP > 0.f && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive();
    });
}

bool ACombatRoundPlayerController::IsCursorOverGameViewport() const
{
    UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!ViewportClient || !FSlateApplication::IsInitialized()) return false;
    const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
    if (!ViewportWidget.IsValid()) return false;
    FSlateApplication& Slate = FSlateApplication::Get();
    const FWidgetPath CursorPath = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows());
    // Unhandled panel clicks must not select the world hidden behind the UI.
    // 처리되지 않은 패널 클릭이 UI 뒤의 월드를 선택하지 않도록 합니다.
    return CursorPath.IsValid() && CursorPath.GetLastWidget() == ViewportWidget.ToSharedRef();
}

void ACombatRoundPlayerController::HandleRoundWorldClick()
{
    if (!CanSelectRoundWorldTarget() || !IsCursorOverGameViewport()) return;
    if (!OnRoundWorldUnitClicked.IsBound() && !OnRoundWorldTileClicked.IsBound()) return;
    FVector RayStart;
    FVector RayDirection;
    if (!DeprojectMousePositionToWorld(RayStart, RayDirection)) return;
    const FVector RayEnd = RayStart + RayDirection * HitResultTraceDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RoundWorldClick), false);
    FHitResult WorldHit;
    GetWorld()->LineTraceSingleByChannel(WorldHit, RayStart, RayEnd, ECC_Visibility, Params);
    float ClosestDistance = WorldHit.bBlockingHit ? WorldHit.Distance : HitResultTraceDistance;
    int32 ClickedUnitId = INDEX_NONE;
    // Combat capsules ignore Visibility, so query only the registered units' actual capsules.
    // 전투 캡슐은 Visibility를 무시하므로 등록된 유닛의 실제 캡슐만 직접 검사합니다.
    for (const FCombatRoundUnitView& Unit : Coordinator->GetView().Units)
    {
        if (Unit.HP <= 0.f || !IsValid(Unit.Unit) || !Unit.Unit->IsUnitAlive() || !Unit.Unit->GetActorEnableCollision()) continue;
        UCapsuleComponent* Capsule = Unit.Unit->GetCapsuleComponent();
        if (!Capsule || !Capsule->IsQueryCollisionEnabled()) continue;
        FHitResult UnitHit;
        if (!Capsule->LineTraceComponent(UnitHit, RayStart, RayEnd, Params) || UnitHit.Distance > ClosestDistance) continue;
        ClosestDistance = UnitHit.Distance;
        ClickedUnitId = Unit.UnitId;
    }
    if (ClickedUnitId != INDEX_NONE)
    {
        OnRoundWorldUnitClicked.Broadcast(ClickedUnitId);
        return;
    }
    HandleRoundWorldTileClicked(Cast<ACombatGridTile>(WorldHit.GetActor()));
}

bool ACombatRoundPlayerController::HandleRoundWorldTileClicked(ACombatGridTile* Tile)
{
    if (!CanSelectRoundWorldTarget() || !IsCursorOverGameViewport() || !IsValid(Tile) || Tile->GetWorld() != GetWorld()) return false;
    const ACombatArena* Arena = Coordinator->GetArena();
    if (!IsValid(Arena) || !IsValid(Arena->Grid) || Tile->GetGridManager() != Arena->Grid || Arena->Grid->GetTileAtCoord(Tile->GridCoord) != Tile) return false;
    OnRoundWorldTileClicked.Broadcast(Tile->GridCoord);
    return true;
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

void ACombatRoundPlayerController::SubmitRoundMove(int32 UnitId, FIntPoint Destination)
{
    if (!CanSelectRoundWorldTarget()) return;
    const FCombatRoundView& View = Coordinator->GetView();
    const FCombatRoundUnitView* Unit = View.Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
    if (!Unit || Unit->bEnemy || Unit->OwnerSlot != ParticipantSlot || Unit->HP <= 0.f) return;
    RequestedCombatId = View.CombatId;
    RequestedRound = View.RoundNumber;
    RequestedRevision = View.PlanRevision;
    bRequestPending = true;
    bAwaitingReplicatedResult = false;
    RequestStatus = NSLOCTEXT("CombatRound", "MovePending", "이동을 서버에서 확인하고 있습니다.");
    ServerSubmitRoundMove(View.CombatId, View.RoundNumber, View.PlanRevision, UnitId, Destination);
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

void ACombatRoundPlayerController::ServerSubmitRoundMove_Implementation(FGuid CombatId, int32 RoundNumber, int32 Revision, int32 UnitId, FIntPoint Destination)
{
    FText Error = NSLOCTEXT("CombatRound", "MoveNoSession", "라운드 세션에 연결되지 않았습니다.");
    const bool bAccepted = IsRoundInputEnabled() && IsValid(Coordinator) && Coordinator->SubmitMove(this, CombatId, RoundNumber, Revision, UnitId, Destination, Error);
    ClientReceiveRoundResponse(bAccepted, bAccepted ? NSLOCTEXT("CombatRound", "MoveAccepted", "이동을 반영했습니다. 행동 계획을 확인해 주세요.") : Error);
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
