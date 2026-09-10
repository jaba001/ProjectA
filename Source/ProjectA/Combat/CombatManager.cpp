#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Net/UnrealNetwork.h"
#include "Game/Turn/TurnManager.h"
#include "Controller/PartyPlayerController.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Queue.h"
#include "TimerManager.h"

ACombatManager::ACombatManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    CombatGridManager = nullptr;
    ActionAuthority = CreateDefaultSubobject<UCombatActionAuthority>(TEXT("ActionAuthority"));
}

void ACombatManager::BeginPlay()
{
    Super::BeginPlay();

    if (!CombatGridManager)
    {
        CombatGridManager = Cast<ACombatGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatGridManager::StaticClass()));
    }

    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CombatGridManager not found"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatManager] CombatGridManager initialized"));
}

void ACombatManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACombatManager, CombatView);
    DOREPLIFETIME(ACombatManager, CombatGridManager);
}

void ACombatManager::SetCombatGrid(ACombatGridManager* Grid)
{
    if (!HasAuthority() || (Grid && Grid->GetWorld() != GetWorld()))
    {
        return;
    }
    CombatGridManager = Grid;
    ForceNetUpdate();
    OnRep_CombatGrid();
}

void ACombatManager::OnRep_CombatGrid()
{
    OnCombatViewChanged.Broadcast();
}

void ACombatManager::OnRep_CombatView()
{
    if (!HasAuthority())
    {
        CombatUnits.Reset();
        for (const FCombatUnitView& Entry : CombatView.Units)
        {
            if (IsValid(Entry.Unit))
            {
                CombatUnits.AddUnique(Entry.Unit);
            }
        }
        CurrentTurnIndex = CombatView.CurrentTurnIndex;
    }
    OnCombatViewChanged.Broadcast();
}

void ACombatManager::PublishCombatView()
{
    if (!HasAuthority())
    {
        return;
    }
    FCombatViewState NewView;
    if (!TurnManager && CombatUnits.IsEmpty() && CombatView.CombatResult != ECombatResult::None)
    {
        NewView = CombatView;
    }
    NewView.ViewRevision = CombatView.ViewRevision + 1;
    NewView.bSuspendedForRecovery = bSuspendedForRecovery;
    if (ActionAuthority && (TurnManager || !CombatUnits.IsEmpty() || CombatView.CombatResult == ECombatResult::None))
    {
        NewView.CombatInstanceId = ActionAuthority->GetCombatInstanceId();
        NewView.RunId = ActionAuthority->GetRunIdentity().RunId;
        NewView.HostEpoch = ActionAuthority->GetRunIdentity().HostEpoch;
    }
    if (TurnManager)
    {
        NewView.TurnSerial = TurnManager->GetTurnCounter();
        NewView.CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();
        NewView.bCombatActive = TurnManager->IsCombatActive();
        NewView.bAwaitingTurnCheckpoint = TurnManager->IsAwaitingTurnCheckpoint();
        NewView.CombatResult = TurnManager->GetCombatResult();
        NewView.CurrentUnit = TurnManager->GetCurrentUnit();
        NewView.CurrentUnitName = TurnManager->GetCurrentUnitName();
    }
    for (AUnitBase* Unit : CombatUnits)
    {
        if (IsValid(Unit))
        {
            FCombatUnitView& Entry = NewView.Units.AddDefaulted_GetRef();
            Entry.Unit = Unit;
            if (ActionAuthority)
            {
                Entry.RuntimeUnitId = ActionAuthority->GetUnitId(Unit);
                Entry.CharacterId = ActionAuthority->GetCharacterId(Unit);
                Entry.OwnerAccountId = ActionAuthority->GetOwnerAccountId(Unit);
            }
        }
    }
    CombatView = MoveTemp(NewView);
    CurrentTurnIndex = CombatView.CurrentTurnIndex;
    ForceNetUpdate();
    OnRep_CombatView();
}

void ACombatManager::HandleTurnChanged()
{
    PublishCombatView();
}

bool ACombatManager::HandleCommitTurnBoundary(int32 CompletedTurnSerial, int32 NextTurnIndex)
{
    return HasAuthority() && !bSuspendedForRecovery && CommitTurnBoundary.IsBound() && CommitTurnBoundary.Execute(CompletedTurnSerial, NextTurnIndex);
}

bool ACombatManager::IsAwaitingTurnCheckpoint() const
{
    return HasAuthority() ? TurnManager && TurnManager->IsAwaitingTurnCheckpoint() && !bSuspendedForRecovery : CombatView.bAwaitingTurnCheckpoint;
}

bool ACombatManager::RetryTurnCheckpoint()
{
    if (!HasAuthority() || bSuspendedForRecovery || !TurnManager)
    {
        return false;
    }
    const bool bResumed = TurnManager->RetryTurnCheckpoint();
    PublishCombatView();
    return bResumed;
}

bool ACombatManager::RestoreCombatFromBoundary(int32 CompletedTurnSerial, int32 NextTurnIndex)
{
    if (!HasAuthority() || IsCombatActive() || IsAwaitingTurnCheckpoint() || CombatUnits.IsEmpty() || !ActionAuthority)
    {
        return false;
    }
    if (TurnManager)
    {
        TurnManager->OnTurnChanged.RemoveAll(this);
        TurnManager->ResetCombat();
    }
    TurnManager = NewObject<UTurnManager>(this);
    TurnManager->OnCombatResult.AddUObject(this, &ACombatManager::HandleCombatResult);
    TurnManager->OnTurnChanged.AddUObject(this, &ACombatManager::HandleTurnChanged);
    if (CommitTurnBoundary.IsBound())
    {
        TurnManager->CommitTurnBoundary.BindUObject(this, &ACombatManager::HandleCommitTurnBoundary);
    }
    bSuspendedForRecovery = false;
    ActionAuthority->BeginCombat();
    if (!TurnManager->RestoreFromBoundary(CombatUnits, CompletedTurnSerial, NextTurnIndex))
    {
        SuspendCombatForRecovery();
        return false;
    }
    RefreshTileProtectedByFront();
    PublishCombatView();
    return true;
}

void ACombatManager::SuspendCombatForRecovery()
{
    if (!HasAuthority())
    {
        return;
    }
    // Suspension never creates a result or a new checkpoint from a partially completed action.
    // 정지는 부분 완료 행동으로 결과나 새 체크포인트를 만들지 않습니다.
    bSuspendedForRecovery = true;
    ClearPlayerSelection();
    GetWorldTimerManager().ClearTimer(DeadTurnTimer);
    if (TurnManager)
    {
        TurnManager->SuspendForRecovery();
    }
    for (AUnitBase* Unit : CombatUnits)
    {
        if (IsValid(Unit))
        {
            Unit->OnTurnEnd();
            Unit->CancelCurrentAction();
        }
    }
    ClearMovableTilesHighlight();
    ClearSkillTargetTilesHighlight();
    PublishCombatView();
}

int32 ACombatManager::GetTurnSerial() const
{
    return HasAuthority() && TurnManager ? TurnManager->GetTurnCounter() : CombatView.TurnSerial;
}

ECombatResult ACombatManager::GetCombatResult() const
{
    return HasAuthority() && TurnManager ? TurnManager->GetCombatResult() : CombatView.CombatResult;
}

FGuid ACombatManager::GetRuntimeUnitId(const AUnitBase* Unit) const
{
    if (!IsValid(Unit))
    {
        return FGuid();
    }
    if (HasAuthority())
    {
        return ActionAuthority ? ActionAuthority->GetUnitId(Unit) : FGuid();
    }
    for (const FCombatUnitView& Entry : CombatView.Units)
    {
        if (Entry.Unit == Unit)
        {
            return Entry.RuntimeUnitId;
        }
    }
    return FGuid();
}

AUnitBase* ACombatManager::ResolveRuntimeUnit(FGuid UnitId) const
{
    if (!UnitId.IsValid())
    {
        return nullptr;
    }
    if (HasAuthority())
    {
        return ActionAuthority ? ActionAuthority->ResolveUnit(UnitId) : nullptr;
    }
    for (const FCombatUnitView& Entry : CombatView.Units)
    {
        if (Entry.RuntimeUnitId == UnitId && IsValid(Entry.Unit))
        {
            return Entry.Unit;
        }
    }
    return nullptr;
}

FGuid ACombatManager::GetCharacterId(const AUnitBase* Unit) const
{
    if (!IsValid(Unit))
    {
        return FGuid();
    }
    if (HasAuthority())
    {
        return ActionAuthority ? ActionAuthority->GetCharacterId(Unit) : FGuid();
    }
    for (const FCombatUnitView& Entry : CombatView.Units)
    {
        if (Entry.Unit == Unit)
        {
            return Entry.CharacterId;
        }
    }
    return FGuid();
}

FRunAccountId ACombatManager::GetOwnerAccountId(const AUnitBase* Unit) const
{
    if (!IsValid(Unit))
    {
        return FRunAccountId();
    }
    if (HasAuthority())
    {
        return ActionAuthority ? ActionAuthority->GetOwnerAccountId(Unit) : FRunAccountId();
    }
    for (const FCombatUnitView& Entry : CombatView.Units)
    {
        if (Entry.Unit == Unit)
        {
            return Entry.OwnerAccountId;
        }
    }
    return FRunAccountId();
}


bool ACombatManager::CanUnitEnterTile(AUnitBase* Unit, ACombatGridTile* Tile) const
{
    if (!Unit || !Tile)
    {
        return false;
    }

    if (Tile == Unit->CurrentTile)
    {
        return true;
    }

    if (Tile->GetOccupyingUnit())
    {
        return false;
    }

    const ETileTerritory TileTerritory = Tile->GetTerritory();

    if (Unit->GetTeam() == ETeam::Player && TileTerritory != ETileTerritory::Player)
    {
        return false;
    }

    if (Unit->GetTeam() == ETeam::Enemy && TileTerritory != ETileTerritory::Enemy)
    {
        return false;
    }

    return true;
}

void ACombatManager::Server_StartCombat_Implementation()
{
    if (!HasAuthority()) return;

    StartCombat_Internal();
}

void ACombatManager::StartCombat_Internal()
{
    if (!HasAuthority() || IsCombatActive() || IsAwaitingTurnCheckpoint() || bSuspendedForRecovery || CombatUnits.IsEmpty())
    {
        return;
    }

    if (TurnManager)
    {
        TurnManager->OnTurnChanged.RemoveAll(this);
        TurnManager->ResetCombat();
    }
    TurnManager = NewObject<UTurnManager>(this);

    if (!TurnManager)
    {
        return;
    }

    ActionAuthority->BeginCombat();
    TurnManager->OnCombatResult.AddUObject(this, &ACombatManager::HandleCombatResult);
    TurnManager->OnTurnChanged.AddUObject(this, &ACombatManager::HandleTurnChanged);
    if (CommitTurnBoundary.IsBound())
    {
        TurnManager->CommitTurnBoundary.BindUObject(this, &ACombatManager::HandleCommitTurnBoundary);
    }
    TurnManager->InitializeTurnOrder(CombatUnits);
    CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();
    RefreshReachableMoveTiles();
    RefreshTileProtectedByFront();
    PublishCombatView();
}

void ACombatManager::RegisterUnits(const TArray<AUnitBase*>& Units)
{
    if (!HasAuthority()) return;  

    ResetCombat();
    for (AUnitBase* Unit : Units)
    {
        if (IsValid(Unit) && !CombatUnits.Contains(Unit))
        {
            CombatUnits.AddUnique(Unit);
            Unit->OnUnitDied.AddUObject(this, &ACombatManager::HandleUnitDied);
            Unit->OnActionCompleted.AddWeakLambda(this, [this](AUnitBase*, EUnitActionType, EUnitActionResult)
            {
                PublishCombatView();
            });
        }
    }
    ActionAuthority->RegisterUnits(CombatUnits);
    CombatView.CombatResult = ECombatResult::None;
    PublishCombatView();
}

void ACombatManager::AdvanceTurn()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!IsCombatActive())
    {
        return;
    }

    ClearPlayerSelection();
    TurnManager->EndTurn();
    CurrentTurnIndex = TurnManager->GetCurrentTurnIndex();
    RefreshReachableMoveTiles();
    RefreshTileProtectedByFront();
}

void ACombatManager::RequestEndTurn()
{
    APartyPlayerController* Controller = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (Controller && Controller->GetCombatManager() == this)
    {
        Controller->RequestEndTurn();
    }
}

bool ACombatManager::RequestEndTurnForUnit(AUnitBase* RequestingUnit)
{
    if (!HasAuthority() || !IsCombatActive() || !IsValid(RequestingUnit) || RequestingUnit != GetCurrentUnit() || !RequestingUnit->IsActiveTurn() || !RequestingUnit->IsUnitAlive() || RequestingUnit->IsBusy())
    {
        return false;
    }
    AdvanceTurn();
    return true;
}

void ACombatManager::ClearPlayerSelection()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APartyPlayerController* Controller = Cast<APartyPlayerController>(It->Get());
        if (Controller && Controller->GetCombatManager() == this)
        {
            Controller->CancelTileInputMode();
        }
    }
}

AUnitBase* ACombatManager::GetCurrentUnit() const
{
    if (IsCombatActive())
    {
        return HasAuthority() ? TurnManager->GetCurrentUnit() : CombatView.CurrentUnit.Get();
    }
    return nullptr;
}

bool ACombatManager::IsCombatActive() const
{
    return HasAuthority() ? !bSuspendedForRecovery && TurnManager && TurnManager->IsCombatActive() : CombatView.bCombatActive;
}

void ACombatManager::HandleUnitDied(AUnitBase* Unit)
{
    if (IsCombatActive())
    {
        TurnManager->EvaluateCombatResult();
        RefreshTileProtectedByFront();
        if (IsCombatActive() && GetCurrentUnit() == Unit)
        {
            // A dead active unit cannot receive input; resume after its death callback returns.
            // 사망한 현재 유닛은 입력을 받을 수 없으므로 사망 콜백 반환 후 다음 턴으로 진행합니다.
            DeadTurnTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakUnit = TWeakObjectPtr<AUnitBase>(Unit)]()
            {
                if (IsCombatActive() && GetCurrentUnit() == WeakUnit.Get())
                {
                    AdvanceTurn();
                }
            }));
        }
    }
    PublishCombatView();
}

void ACombatManager::HandleCombatResult(ECombatResult Result)
{
    EndCombat();
    OnCombatResult.Broadcast(Result);
}

void ACombatManager::EndCombat()
{
    if (!HasAuthority())
    {
        return;
    }
    ClearPlayerSelection();
    GetWorldTimerManager().ClearTimer(DeadTurnTimer);
    if (TurnManager)
    {
        TurnManager->StopCombat();
    }
    ClearMovableTilesHighlight();
    ClearSkillTargetTilesHighlight();
    ReachableMoveTiles.Reset();
    SkillTargetTiles.Reset();
    for (AUnitBase* Unit : CombatUnits)
    {
        if (IsValid(Unit))
        {
            Unit->OnTurnEnd();
            Unit->CancelCurrentAction();
        }
    }
    if (TurnManager)
    {
        PublishCombatView();
    }
}

void ACombatManager::ResetCombat()
{
    if (!HasAuthority())
    {
        CombatUnits.Reset();
        return;
    }
    EndCombat();
    bSuspendedForRecovery = false;
    for (AUnitBase* Unit : CombatUnits)
    {
        if (IsValid(Unit))
        {
            Unit->OnUnitDied.RemoveAll(this);
            Unit->OnActionCompleted.RemoveAll(this);
        }
    }
    CombatUnits.Reset();
    if (ActionAuthority)
    {
        ActionAuthority->Reset();
    }
    if (TurnManager)
    {
        TurnManager->OnTurnChanged.RemoveAll(this);
        TurnManager->ResetCombat();
        TurnManager = nullptr;
    }
    CurrentTurnIndex = INDEX_NONE;
    // Cleanup removes actor references while retaining the last result until a new encounter is registered.
    // 정리는 액터 참조를 제거하되 다음 인카운터가 등록될 때까지 마지막 결과를 유지합니다.
    FCombatViewState ClearedView;
    if (CombatView.CombatResult != ECombatResult::None)
    {
        ClearedView.CombatInstanceId = CombatView.CombatInstanceId;
        ClearedView.RunId = CombatView.RunId;
        ClearedView.HostEpoch = CombatView.HostEpoch;
        ClearedView.TurnSerial = CombatView.TurnSerial;
        ClearedView.CombatResult = CombatView.CombatResult;
    }
    ClearedView.ViewRevision = CombatView.ViewRevision + 1;
    CombatView = MoveTemp(ClearedView);
    ForceNetUpdate();
    OnRep_CombatView();
}

void ACombatManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    OnCombatViewChanged.Clear();
    ResetCombat();
    OnCombatResult.Clear();
    Super::EndPlay(EndPlayReason);
}

void ACombatManager::RefreshReachableMoveTiles()
{
    ReachableMoveTiles.Empty();

    AUnitBase* CurrentUnit = GetCurrentUnit();

    if (!CurrentUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshReachableMoveTiles failed | CurrentUnit is null"));
        return;
    }

    //UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshReachableMoveTiles | CurrentUnit=%s | MoveRange=%d"), *GetNameSafe(CurrentUnit), CurrentUnit->GetMoveRange());

    ReachableMoveTiles = CalculateReachableMoveTiles(CurrentUnit);

    //UE_LOG(LogTemp, Log, TEXT("[CombatManager] ReachableMoveTiles refreshed | Unit=%s | Count=%d"), *GetNameSafe(CurrentUnit), ReachableMoveTiles.Num());
}

bool ACombatManager::IsReachableMoveTile(ACombatGridTile* Tile) const
{
    if (!Tile)
    {
        return false;
    }

    return ReachableMoveTiles.Contains(Tile);
}

void ACombatManager::HighlightMovableTiles()
{
    for (ACombatGridTile* Tile : ReachableMoveTiles)
    {
        if (!Tile)
        {
            continue;
        }

        //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Highlight Tile | Coord=(%d,%d)"), Tile->GridCoord.X, Tile->GridCoord.Y);

        Tile->ApplyMovableTileVisual();
    }
}

void ACombatManager::ClearMovableTilesHighlight()
{
    for (ACombatGridTile* Tile : ReachableMoveTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ClearHighlightVisual();
    }
}

void ACombatManager::RefreshSkillTargetTiles()
{
    SkillTargetTiles.Empty();

    AUnitBase* CurrentUnit = GetCurrentUnit();

    if (!CurrentUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshSkillTargetTiles failed | CurrentUnit is null"));
        return;
    }

    SkillTargetTiles = CalculateSkillTargetTiles(CurrentUnit);

    //UE_LOG(LogTemp, Log, TEXT("[CombatManager] SkillTargetTiles refreshed | Unit=%s | Count=%d"), *GetNameSafe(CurrentUnit), SkillTargetTiles.Num());
}

bool ACombatManager::IsSkillTargetTile(ACombatGridTile* Tile) const
{
    if (!Tile)
    {
        return false;
    }

    return SkillTargetTiles.Contains(Tile);
}

void ACombatManager::HighlightSkillTargetTiles()
{
    for (ACombatGridTile* Tile : SkillTargetTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ApplySkillTargetTileVisual();
    }
}

void ACombatManager::ClearSkillTargetTilesHighlight()
{
    for (ACombatGridTile* Tile : SkillTargetTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ClearHighlightVisual();
    }

    SkillTargetTiles.Empty();
}

ACombatGridTile* ACombatManager::GetTileByCoord(FIntPoint Coord) const
{
    if (!CombatGridManager)
    {
        return nullptr;
    }

    ACombatGridTile* const* FoundTile = CombatGridManager->TileMap.Find(Coord);

    if (!FoundTile)
    {
        return nullptr;
    }

    return *FoundTile;
}

void ACombatManager::RefreshTileProtectedByFront()
{
    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] RefreshTileProtectedByFront failed | CombatGridManager is null"));
        return;
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& TilePair : CombatGridManager->TileMap)
    {
        ACombatGridTile* Tile = TilePair.Value;

        if (!Tile)
        {
            continue;
        }

        Tile->SetProtectedByFront(false);
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& TilePair : CombatGridManager->TileMap)
    {
        ACombatGridTile* Tile = TilePair.Value;

        if (!Tile)
        {
            continue;
        }

        const ETileTerritory Territory = Tile->GetTerritory();
        const FIntPoint Coord = Tile->GridCoord;

        ACombatGridTile* FrontTile = nullptr;

        if (Territory == ETileTerritory::Player)
        {
            if (Coord.Y != 0)
            {
                continue;
            }

            FrontTile = GetTileByCoord(FIntPoint(Coord.X, 1));
        }
        else if (Territory == ETileTerritory::Enemy)
        {
            if (Coord.Y != 3)
            {
                continue;
            }

            FrontTile = GetTileByCoord(FIntPoint(Coord.X, 2));
        }
        else
        {
            continue;
        }

        if (!FrontTile)
        {
            continue;
        }

        AUnitBase* FrontUnit = FrontTile->GetOccupyingUnit();

        if (!FrontUnit)
        {
            continue;
        }

        if (!FrontUnit->IsUnitAlive())
        {
            continue;
        }

        Tile->SetProtectedByFront(true);

        //UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Tile Check | Coord=(%d,%d) | Territory=%d"), Coord.X, Coord.Y, static_cast<int32>(Territory));
    }
}

TArray<ACombatGridTile*> ACombatManager::CalculateSkillTargetTiles(AUnitBase* Unit) const
{
    TArray<ACombatGridTile*> Result;

    if (!Unit)
    {
        return Result;
    }

    if (!CombatGridManager)
    {
        return Result;
    }

    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());

    if (!PC)
    {
        return Result;
    }

    for (const TPair<FIntPoint, ACombatGridTile*>& TilePair : CombatGridManager->TileMap)
    {
        ACombatGridTile* Tile = TilePair.Value;

        if (!Tile)
        {
            continue;
        }

        if (PC->IsValidTileForPendingSkill(Tile))
        {
            Result.Add(Tile);
        }
    }

    return Result;
}


TArray<ACombatGridTile*> ACombatManager::CalculateReachableMoveTiles(AUnitBase* Unit) const
{
    TArray<ACombatGridTile*> Result;

    if (!Unit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CalculateReachableMoveTiles failed | Unit is null"));
        return Result;
    }

    if (!CombatGridManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CalculateReachableMoveTiles failed | CombatGridManager is null"));
        return Result;
    }

    ACombatGridTile* StartTile = Unit->CurrentTile;

    if (!StartTile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatManager] CalculateReachableMoveTiles failed | StartTile is null | Unit=%s"), *GetNameSafe(Unit));
        return Result;
    }

    //UE_LOG(LogTemp, Log, TEXT("[CombatManager] CalculateReachableMoveTiles | StartTile=(%d,%d)"), StartTile->GridCoord.X, StartTile->GridCoord.Y);

    const int32 MaxMoveRange = Unit->GetMoveRange();

    TQueue<TPair<ACombatGridTile*, int32>> SearchQueue;
    TSet<ACombatGridTile*> VisitedTiles;

    SearchQueue.Enqueue(TPair<ACombatGridTile*, int32>(StartTile, 0));
    VisitedTiles.Add(StartTile);

    while (!SearchQueue.IsEmpty())
    {
        TPair<ACombatGridTile*, int32> CurrentPair;
        SearchQueue.Dequeue(CurrentPair);

        ACombatGridTile* CurrentTile = CurrentPair.Key;
        const int32 CurrentDistance = CurrentPair.Value;

        if (!CurrentTile)
        {
            continue;
        }

        if (CurrentTile != StartTile)
        {
            Result.Add(CurrentTile);
        }

        if (CurrentDistance >= MaxMoveRange)
        {
            continue;
        }

        const TArray<ACombatGridTile*> AdjacentTiles = CombatGridManager->GetAdjacentTiles(CurrentTile);

        for (ACombatGridTile* NextTile : AdjacentTiles)
        {
            if (!NextTile)
            {
                continue;
            }

            //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Check Adjacent Tile | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);

            if (VisitedTiles.Contains(NextTile))
            {
                //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Skip Tile | Reason=Visited | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);
                continue;
            }

            if (!CanUnitEnterTile(Unit, NextTile))
            {
                //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Skip Tile | Reason=Blocked | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);
                continue;
            }

            //UE_LOG(LogTemp, Log, TEXT("[CombatManager] Add Reachable Tile | Coord=(%d,%d)"), NextTile->GridCoord.X, NextTile->GridCoord.Y);

            VisitedTiles.Add(NextTile);
            SearchQueue.Enqueue(TPair<ACombatGridTile*, int32>(NextTile, CurrentDistance + 1));
        }
    }

    return Result;
}
