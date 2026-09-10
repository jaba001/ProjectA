#include "PartyPlayerController.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Blueprint/UserWidget.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Abilities/GameplayAbility.h"

APartyPlayerController::APartyPlayerController()
{
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bShowMouseCursor = true;     
}

void APartyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController() && ShouldCreateCombatHUD())
    {
        InitializeCombatManager();
        InitializeHUD();
    }
}

void APartyPlayerController::SetCombatContext(ACombatManager* InManager, bool bEnableInput)
{
    CancelTileInputMode();
    CombatManager = InManager;
    bCombatInputEnabled = bEnableInput;
}

void APartyPlayerController::InitializeCombatManager()
{
    CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] CombatManager not found"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] CombatManager initialized"));
}

void APartyPlayerController::InitializeHUD()
{
    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] HUDWidgetClass is null"));
        return;
    }

    HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);

    if (!HUDWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] HUDWidget creation failed"));
        return;
    }

    HUDWidget->AddToViewport();
    //UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] HUDWidget initialized"));
}

AUnitBase* APartyPlayerController::GetActiveUnit() const
{
    if (!CombatManager)
        return nullptr;

    return CombatManager->GetCurrentUnit();
}

void APartyPlayerController::RequestEndTurn()
{
    if (CanUseActiveUnitAction())
    {
        FCombatActionRequest Request;
        if (BuildCombatActionRequest(ECombatActionKind::EndTurn, nullptr, nullptr, Request))
        {
            SubmitCombatActionRequest(Request);
        }
    }
}

void APartyPlayerController::RequestHealingItem()
{
    AUnitBase* Unit = GetActiveUnit();
    if (!CanUseActiveUnitAction() || !Unit->CanUseHealingItem(Unit))
    {
        return;
    }
    FCombatActionRequest Request;
    if (BuildCombatActionRequest(ECombatActionKind::HealingItem, nullptr, Unit->CurrentTile, Request))
    {
        SubmitCombatActionRequest(Request);
    }
}

bool APartyPlayerController::BuildCombatActionRequest(ECombatActionKind Kind, USkillDefinitionDataAsset* Skill, ACombatGridTile* TargetTile, FCombatActionRequest& OutRequest)
{
    if (!CombatManager || !CombatManager->IsCombatActive() || !CombatManager->GetTurnManager())
    {
        return false;
    }
    UCombatActionAuthority* Authority = CombatManager->GetActionAuthority();
    if (!Authority || !Authority->GetCombatInstanceId().IsValid() || !Authority->GetUnitId(GetActiveUnit()).IsValid())
    {
        return false;
    }
    if (RequestCombatInstanceId != Authority->GetCombatInstanceId())
    {
        RequestCombatInstanceId = Authority->GetCombatInstanceId();
        NextRequestSequence = 0;
    }
    if (NextRequestSequence == MAX_int64 || (TargetTile && (TargetTile->GetWorld() != GetWorld() || CombatManager->GetTileByCoord(TargetTile->GridCoord) != TargetTile)))
    {
        return false;
    }
    OutRequest = FCombatActionRequest();
    OutRequest.RunId = Authority->GetRunIdentity().RunId;
    OutRequest.HostEpoch = Authority->GetRunIdentity().HostEpoch;
    OutRequest.CombatInstanceId = RequestCombatInstanceId;
    OutRequest.ParticipantBindingId = Authority->GetParticipantBindingId(this);
    OutRequest.TurnSerial = CombatManager->GetTurnManager()->GetTurnCounter();
    OutRequest.RequestSequence = ++NextRequestSequence;
    OutRequest.UnitId = Authority->GetUnitId(GetActiveUnit());
    OutRequest.Kind = Kind;
    if (TargetTile)
    {
        OutRequest.TargetCoord = TargetTile->GridCoord;
    }
    if (Kind == ECombatActionKind::Skill && IsValid(Skill))
    {
        OutRequest.SkillId = Skill->GetPrimaryAssetId();
        const bool bUnitTarget = Skill->bMoveToTarget || Skill->TargetRule == ESkillTargetRule::EnemyUnit || Skill->TargetRule == ESkillTargetRule::AllyUnit || Skill->TargetRule == ESkillTargetRule::AnyUnit;
        if (bUnitTarget && TargetTile)
        {
            OutRequest.TargetUnitId = Authority->GetUnitId(TargetTile->GetOccupyingUnit());
        }
    }
    else if (Kind == ECombatActionKind::HealingItem && TargetTile)
    {
        OutRequest.TargetUnitId = Authority->GetUnitId(TargetTile->GetOccupyingUnit());
    }
    return true;
}

FCombatActionResponse APartyPlayerController::SubmitCombatActionRequest(const FCombatActionRequest& Request)
{
    if (LatestSubmittedCombatId != Request.CombatInstanceId)
    {
        LatestSubmittedCombatId = Request.CombatInstanceId;
        LatestSubmittedSequence = 0;
        LastHandledResponseSequence = 0;
    }
    if (Request.RequestSequence > LatestSubmittedSequence)
    {
        LatestSubmittedSequence = Request.RequestSequence;
        SubmittedSelectionRevision = SelectionRevision;
    }
    FCombatActionResponse Response;
    Response.CombatInstanceId = Request.CombatInstanceId;
    Response.RequestSequence = Request.RequestSequence;
    if (!HasAuthority())
    {
        Response.Result = ECombatRequestResult::Pending;
        LastCombatActionResponse = Response;
        ServerRequestCombatAction(Request);
        return Response;
    }
    if (IsValid(CombatManager) && CombatManager->GetActionAuthority())
    {
        Response = CombatManager->GetActionAuthority()->Execute(this, Request);
    }
    else
    {
        Response.Result = ECombatRequestResult::InvalidContext;
        Response.Message = FText::FromString(TEXT("요청을 처리할 전투가 없습니다."));
    }
    if (IsLocalController())
    {
        HandleCombatActionResponse(Response);
    }
    else
    {
        LastCombatActionResponse = Response;
        ClientReceiveCombatActionResponse(Response);
    }
    return Response;
}

void APartyPlayerController::ServerRequestCombatAction_Implementation(const FCombatActionRequest& Request)
{
    SubmitCombatActionRequest(Request);
}

void APartyPlayerController::ClientReceiveCombatActionResponse_Implementation(const FCombatActionResponse& Response)
{
    HandleCombatActionResponse(Response);
}

void APartyPlayerController::HandleCombatActionResponse(const FCombatActionResponse& Response)
{
    // Only the latest request can update feedback, and a new selection survives a delayed acknowledgement.
    // 최신 요청만 응답 상태를 갱신하며 응답이 늦어져도 새로 선택한 입력은 유지합니다.
    if (!CombatManager || !CombatManager->GetActionAuthority() || Response.CombatInstanceId != CombatManager->GetActionAuthority()->GetCombatInstanceId() || Response.CombatInstanceId != LatestSubmittedCombatId || Response.RequestSequence != LatestSubmittedSequence || Response.RequestSequence <= LastHandledResponseSequence)
    {
        return;
    }
    LastHandledResponseSequence = Response.RequestSequence;
    LastCombatActionResponse = Response;
    if (Response.Result == ECombatRequestResult::Accepted && SelectionRevision == SubmittedSelectionRevision)
    {
        CancelTileInputMode();
    }
    OnCombatActionResponse.Broadcast(Response);
}

bool APartyPlayerController::IsEquippedInputSkill(USkillDefinitionDataAsset* Skill) const
{
    AUnitBase* Unit = GetActiveUnit();
    if (!IsValid(Skill) || !Unit || !Unit->GetEquippedSkillDataAssets().Contains(Skill) || !Skill->GetPrimaryAssetId().IsValid() || !Unit->GetAbilitySystemComponent() || !Skill->AbilityClass)
    {
        return false;
    }
    int32 Matches = 0;
    for (const USkillDefinitionDataAsset* Equipped : Unit->GetEquippedSkillDataAssets())
    {
        Matches += IsValid(Equipped) && Equipped->GetPrimaryAssetId() == Skill->GetPrimaryAssetId() ? 1 : 0;
    }
    const FGameplayAbilitySpec* Spec = Unit->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Skill->AbilityClass);
    return Matches == 1 && Spec && !Spec->IsActive();
}

void APartyPlayerController::HandleTileClicked(ACombatGridTile* Tile)
{
    if (!IsValid(Tile) || Tile->GetWorld() != GetWorld() || !CanUseActiveUnitAction())
    {
        return;
    }

    if (IsSkillInputMode())
    {
        USkillDefinitionDataAsset* SkillData = PendingSkillData;
        if (!IsValid(SkillData) || !SkillData->AbilityClass || !CanUseActiveUnitActionPoint(SkillData->ActionPointCost) || !IsValidTileForPendingSkill(Tile))
        {
            return;
        }
        FCombatActionRequest Request;
        if (BuildCombatActionRequest(ECombatActionKind::Skill, SkillData, Tile, Request))
        {
            SubmitCombatActionRequest(Request);
        }
        return;
    }

    if (IsMoveInputMode())
    {
        if (!CanUseActiveUnitSubActionPoint(1) || !CombatManager->IsReachableMoveTile(Tile))
        {
            return;
        }
        FCombatActionRequest Request;
        if (BuildCombatActionRequest(ECombatActionKind::Move, nullptr, Tile, Request) && SubmitCombatActionRequest(Request).Result == ECombatRequestResult::Accepted)
        {
            SetSelectedTile(Tile);
        }
        return;
    }

    SetSelectedTile(Tile);
}

bool APartyPlayerController::CanUseActiveUnitAction() const
{
    if (!HasAuthority() || !bCombatInputEnabled || !CombatManager || !CombatManager->IsCombatActive())
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    if (!ActiveUnit->IsUnitAlive() || ActiveUnit->GetTeam() != ETeam::Player)
    {
        return false;
    }

    if (!ActiveUnit->IsActiveTurn())
    {
        return false;
    }

    if (ActiveUnit->IsBusy())
    {
        return false;
    }

    return CombatManager->GetActionAuthority() && CombatManager->GetActionAuthority()->CanControllerControl(this, ActiveUnit);
}

bool APartyPlayerController::CanUseActiveUnitActionPoint(int32 Cost) const
{
    if (!CanUseActiveUnitAction())
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    return ActiveUnit->HasEnoughActionPoint(Cost);
}

bool APartyPlayerController::CanUseActiveUnitSubActionPoint(int32 Cost) const
{
    if (!CanUseActiveUnitAction())
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    return ActiveUnit->HasEnoughSubActionPoint(Cost);
}

void APartyPlayerController::SetSelectedTile(ACombatGridTile* InTile)
{
    SelectedTile = InTile;

    if (!InTile)
    {
        return;
    }

    AUnitBase* OccupyingUnit = InTile->GetOccupyingUnit();

    if (OccupyingUnit)
    {
        //UE_LOG(LogTemp, Log, TEXT("[PC] Selected Tile (%d,%d) | Unit=%s"), InTile->GridCoord.X, InTile->GridCoord.Y, *OccupyingUnit->GetName());
    }
    else
    {
        //UE_LOG(LogTemp, Log, TEXT("[PC] Selected Tile (%d,%d) | Unit=None"), InTile->GridCoord.X, InTile->GridCoord.Y);
    }
}

ACombatGridTile* APartyPlayerController::GetSelectedTile() const
{
    return SelectedTile;
}

void APartyPlayerController::ClearSelectedTile()
{
    SelectedTile = nullptr;
    //UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] SelectedTile cleared by ClearSelectedTile"));
}

void APartyPlayerController::SetTileInputMode(ETileInputMode NewMode)
{
    ++SelectionRevision;
    CurrentTileInputMode = NewMode;
    //UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] TileInputMode Changed | Mode=%d"), static_cast<uint8>(CurrentTileInputMode));
}

void APartyPlayerController::EnterMoveMode()
{
    CancelTileInputMode();

    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterMoveMode failed | CombatManager is null"));
        return;
    }

    if (!CanUseActiveUnitSubActionPoint(1))
    {
        return;
    }

    CombatManager->ClearMovableTilesHighlight();
    ClearSelectedTile();

    SetTileInputMode(ETileInputMode::Move);
    CombatManager->RefreshReachableMoveTiles();
    CombatManager->HighlightMovableTiles();
}

void APartyPlayerController::EnterSkillMode(USkillDefinitionDataAsset* SkillData)
{
    CancelTileInputMode();

    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | CombatManager is null"));
        return;
    }

    if (!UCombatTargetingLibrary::IsSupportedSkillArea(SkillData))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | Missing skill or unsupported area"));
        return;
    }

    if (!SkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | AbilityClass is null"));
        return;
    }

    if (SkillData->ActionPointCost <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | Invalid ActionPointCost=%d"), SkillData->ActionPointCost);
        return;
    }

    if (!CanUseActiveUnitActionPoint(SkillData->ActionPointCost))
    {
        return;
    }

    if (!IsEquippedInputSkill(SkillData))
    {
        return;
    }

    CombatManager->ClearMovableTilesHighlight();
    CombatManager->ClearSkillTargetTilesHighlight();
    ClearSelectedTile();

    PendingSkillData = SkillData;

    SetTileInputMode(ETileInputMode::Skill);
    CombatManager->RefreshSkillTargetTiles();
    CombatManager->HighlightSkillTargetTiles();
}

void APartyPlayerController::CancelTileInputMode()
{
    if (CombatManager)
    {
        CombatManager->ClearMovableTilesHighlight();
        CombatManager->ClearSkillTargetTilesHighlight();
    }

    PendingSkillData = nullptr;
    SetTileInputMode(ETileInputMode::None);
    ClearSelectedTile();
}

bool APartyPlayerController::IsValidTileForPendingSkill(ACombatGridTile* Tile) const
{
    return UCombatTargetingLibrary::IsValidSkillTarget(GetActiveUnit(), PendingSkillData, Tile);
}
