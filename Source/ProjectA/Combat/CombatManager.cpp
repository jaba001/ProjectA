#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Net/UnrealNetwork.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Controller/PartyPlayerController.h"
#include "Unit/UnitBase.h"
#include "Unit/PlayerUnit.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"

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
    DOREPLIFETIME(ACombatManager, RoundCoordinator);
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
    NewView.CombatResult = CombatView.CombatResult;
    NewView.ViewRevision = CombatView.ViewRevision + 1;
    NewView.bSuspendedForRecovery = bSuspendedForRecovery;
    if (ActionAuthority && !CombatUnits.IsEmpty())
    {
        NewView.CombatInstanceId = ActionAuthority->GetCombatInstanceId();
        NewView.RunId = ActionAuthority->GetRunIdentity().RunId;
        NewView.HostEpoch = ActionAuthority->GetRunIdentity().HostEpoch;
    }
    else
    {
        NewView.CombatInstanceId = CombatView.CombatInstanceId;
        NewView.RunId = CombatView.RunId;
        NewView.HostEpoch = CombatView.HostEpoch;
    }
    if (IsValid(RoundCoordinator))
    {
        // The compatibility serial reports a round, never an individual unit turn.
        // 호환 일련번호는 개별 유닛 턴이 아닌 라운드를 표시합니다.
        NewView.TurnSerial = RoundCoordinator->GetView().RoundNumber;
        NewView.bCombatActive = !bSuspendedForRecovery && RoundCoordinator->IsRoundSessionActive();
    }
    for (AUnitBase* Unit : CombatUnits)
    {
        if (!IsValid(Unit))
        {
            continue;
        }
        FCombatUnitView& Entry = NewView.Units.AddDefaulted_GetRef();
        Entry.Unit = Unit;
        if (const APlayerUnit* Player = Cast<APlayerUnit>(Unit))
        {
            Entry.PartyControlMode = Player->GetPartyControlMode();
        }
        if (ActionAuthority)
        {
            Entry.RuntimeUnitId = ActionAuthority->GetUnitId(Unit);
            Entry.CharacterId = ActionAuthority->GetCharacterId(Unit);
            Entry.OwnerAccountId = ActionAuthority->GetOwnerAccountId(Unit);
        }
    }
    CombatView = MoveTemp(NewView);
    CurrentTurnIndex = INDEX_NONE;
    ForceNetUpdate();
    OnRep_CombatView();
}

void ACombatManager::OnRep_RoundCoordinator()
{
    OnCombatViewChanged.Broadcast();
}

bool ACombatManager::IsPartyAIControlled(const AUnitBase* Unit) const
{
    if (HasAuthority())
    {
        const APlayerUnit* Player = Cast<APlayerUnit>(Unit);
        return Player && Player->IsServerAIControlled();
    }
    const FCombatUnitView* Entry = CombatView.Units.FindByPredicate([Unit](const FCombatUnitView& Value) { return Value.Unit == Unit; });
    return Entry && Entry->PartyControlMode == EPartyControlMode::ServerAI;
}

void ACombatManager::SuspendCombatForRecovery()
{
    if (!HasAuthority())
    {
        return;
    }
    // A disconnect pauses the session without inventing a result or a resumable mid-round save.
    // 연결 종료는 결과나 재개 가능한 라운드 중간 저장을 만들지 않고 세션을 정지합니다.
    bSuspendedForRecovery = true;
    ClearPlayerSelection();
    if (IsValid(RoundCoordinator))
    {
        RoundCoordinator->SuspendRound();
    }
    ClearMovableTilesHighlight();
    ClearSkillTargetTilesHighlight();
    PublishCombatView();
}

int32 ACombatManager::GetTurnSerial() const
{
    return HasAuthority() && IsValid(RoundCoordinator) ? RoundCoordinator->GetView().RoundNumber : CombatView.TurnSerial;
}

ECombatResult ACombatManager::GetCombatResult() const
{
    return CombatView.CombatResult;
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


void ACombatManager::Server_StartCombat_Implementation()
{
    if (!HasAuthority()) return;

    StartCombat_Internal();
}

void ACombatManager::StartCombat_Internal()
{
    if (!HasAuthority() || IsCombatActive() || bSuspendedForRecovery || CombatUnits.IsEmpty() || !ActionAuthority)
    {
        return;
    }
    if (IsValid(RoundCoordinator))
    {
        RoundCoordinator->OnCombatFinished.RemoveAll(this);
        RoundCoordinator->OnRoundStateChanged.RemoveAll(this);
        RoundCoordinator->StopRound();
        RoundCoordinator->Destroy();
        RoundCoordinator = nullptr;
    }
    ActionAuthority->BeginCombat();
    if (!ActionAuthority->GetCombatInstanceId().IsValid())
    {
        return;
    }
    PublishCombatView();
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    RoundCoordinator = GetWorld()->SpawnActor<ACombatRoundCoordinator>(ACombatRoundCoordinator::StaticClass(), FTransform::Identity, Params);
    if (!IsValid(RoundCoordinator))
    {
        return;
    }
    RoundCoordinator->OnCombatFinished.AddUObject(this, &ACombatManager::HandleCombatResult);
    RoundCoordinator->OnRoundStateChanged.AddUObject(this, &ACombatManager::PublishCombatView);
    FText Error;
    if (!RoundCoordinator->InitializeFromCombat(this, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("[CombatManager] Timed-round initialization failed: %s"), *Error.ToString());
        RoundCoordinator->OnCombatFinished.RemoveAll(this);
        RoundCoordinator->OnRoundStateChanged.RemoveAll(this);
        RoundCoordinator->StopRound();
        RoundCoordinator->Destroy();
        RoundCoordinator = nullptr;
        PublishCombatView();
        return;
    }
    PublishCombatView();
    OnRep_RoundCoordinator();
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
        }
    }
    ActionAuthority->RegisterUnits(CombatUnits);
    CombatView.CombatResult = ECombatResult::None;
    PublishCombatView();
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
    return nullptr;
}

bool ACombatManager::IsCombatActive() const
{
    return HasAuthority() ? !bSuspendedForRecovery && IsValid(RoundCoordinator) && RoundCoordinator->IsRoundSessionActive() : CombatView.bCombatActive;
}

void ACombatManager::HandleUnitDied(AUnitBase* Unit)
{
    // The round coordinator waits for pending attacks and returns before declaring the final result.
    // 라운드 조정자가 잔여 공격과 복귀를 기다린 뒤 최종 결과를 선언합니다.
    RefreshTileProtectedByFront();
    PublishCombatView();
}

void ACombatManager::HandleCombatResult(ECombatResult Result)
{
    if (!HasAuthority() || Result == ECombatResult::None || CombatView.CombatResult != ECombatResult::None)
    {
        return;
    }
    CombatView.CombatResult = Result;
    ClearPlayerSelection();
    ClearMovableTilesHighlight();
    ClearSkillTargetTilesHighlight();
    PublishCombatView();
    OnCombatResult.Broadcast(Result);
}

void ACombatManager::EndCombat()
{
    if (!HasAuthority())
    {
        return;
    }
    ClearPlayerSelection();
    if (IsValid(RoundCoordinator))
    {
        RoundCoordinator->StopRound();
    }
    ClearMovableTilesHighlight();
    ClearSkillTargetTilesHighlight();
    ReachableMoveTiles.Reset();
    SkillTargetTiles.Reset();
    PublishCombatView();
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
        }
    }
    CombatUnits.Reset();
    if (ActionAuthority)
    {
        ActionAuthority->Reset();
    }
    if (IsValid(RoundCoordinator))
    {
        RoundCoordinator->OnCombatFinished.RemoveAll(this);
        RoundCoordinator->OnRoundStateChanged.RemoveAll(this);
        RoundCoordinator->Destroy();
    }
    RoundCoordinator = nullptr;
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
    // Retired grid protection must not suggest a targeting restriction in real-time combat.
    // 실시간 전투에 존재하지 않는 대상 제한을 표시하지 않도록 기존 격자 보호를 해제합니다.
    if (!HasAuthority() || !CombatGridManager) return;
    for (const TPair<FIntPoint, ACombatGridTile*>& Pair : CombatGridManager->TileMap)
    {
        if (IsValid(Pair.Value) && Pair.Value->GetProtectedByFront()) Pair.Value->SetProtectedByFront(false);
    }
}
