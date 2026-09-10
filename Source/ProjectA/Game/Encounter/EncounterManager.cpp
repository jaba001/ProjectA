#include "Game/Encounter/EncounterManager.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Combat/CombatManager.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/SkillActor/SkillActorBase.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Game/Turn/TurnManager.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"

AEncounterManager::AEncounterManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEncounterManager::InitializeEncounter(ACombatArena* InArena, ACombatManager* InCombatManager, UPartyDefinitionDataAsset* InPartyDefinition, const TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>>& InDefinitions)
{
    Arena = InArena;
    CombatManager = InCombatManager;
    PartyDefinition = InPartyDefinition;
    Definitions = InDefinitions;
    RunState = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    if (RunState->PartyDefinition)
    {
        PartyDefinition = RunState->PartyDefinition;
    }
    if (CombatManager)
    {
        CombatManager->OnCombatResult.AddUObject(this, &AEncounterManager::HandleCombatResult);
    }
    if (Arena)
    {
        Arena->CleanupArena();
    }
    SetPlayerCombatInput(false);
}

bool AEncounterManager::RequestStartNode(FName NodeId)
{
    if (!HasAuthority() || bPreparing || PendingResult != ECombatResult::None || !RunState || !RunState->CanStartNode(NodeId))
    {
        return false;
    }
    bPreparing = true;
    FlowMessage = FText::GetEmpty();
    SetPlayerCombatInput(false);
    if (!RunState->BeginEncounter(NodeId))
    {
        bPreparing = false;
        return false;
    }
    OnFlowChanged.Broadcast();
    if (!Arena || !CombatManager || !PartyDefinition)
    {
        return FailPreparation(FText::FromString(TEXT("Gameplay setup is incomplete. Check Arena and PartyDefinition. / Gameplay 설정을 확인하세요.")));
    }
    FText ArenaError;
    if (!Arena->PrepareArena(ArenaError))
    {
        return FailPreparation(ArenaError);
    }
    const TObjectPtr<UEncounterDefinitionDataAsset>* Definition = Definitions.Find(RunState->GetCurrentEncounterId());
    if (!Definition || !IsValid(*Definition) || !SpawnEncounter(*Definition))
    {
        return FailPreparation(FlowMessage.IsEmpty() ? FText::FromString(TEXT("Encounter spawn failed. Check unit classes and spawn coordinates. / 유닛 클래스와 스폰 좌표를 확인하세요.")) : FlowMessage);
    }
    CombatManager->SetCombatGrid(Arena->Grid);
    TArray<AUnitBase*> Units;
    for (AUnitBase* Unit : SpawnedUnits)
    {
        Units.Add(Unit);
    }
    CombatManager->RegisterUnits(Units);
    FText AuthorityError;
    if (!ConfigureCombatParticipants(AuthorityError))
    {
        return FailPreparation(AuthorityError);
    }
    CombatAttemptId = FGuid::NewGuid();
    PendingTurnCheckpoint = FCombatCheckpointData();
    CheckpointUnitIds.Reset();
    EnableCombatCheckpoints();
    Arena->ActivateArena(GetWorld()->GetFirstPlayerController());
    RunState->MarkCombatStarted();
    bPreparing = false;
    CombatManager->StartCombat_Internal();
    if (!CombatManager->IsCombatActive() && !CombatManager->IsAwaitingTurnCheckpoint())
    {
        if (PendingResult != ECombatResult::None)
        {
            return true;
        }
        return FailPreparation(FText::FromString(TEXT("Combat could not start. / 전투를 시작할 수 없습니다.")));
    }
    SetPlayerCombatInput(CombatManager->IsCombatActive());
    UE_LOG(LogTemp, Log, TEXT("[Encounter] Started Node=%s Units=%d"), *NodeId.ToString(), SpawnedUnits.Num());
    OnFlowChanged.Broadcast();
    return true;
}

bool AEncounterManager::ConfigureCombatParticipants(FText& OutError)
{
    UCombatActionAuthority* Authority = CombatManager ? CombatManager->GetActionAuthority() : nullptr;
    if (!Authority || !Authority->ConfigureRun(RunState->GetRunIdentity(), RunState->GetPartyMembers(), PartyActors, OutError))
    {
        return false;
    }
    const FRunIdentityData& Identity = RunState->GetRunIdentity();
    APartyPlayerController* LocalController = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (GetNetMode() == NM_Standalone && LocalController && LocalController->IsLocalController() && Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Identity.OriginalParticipants.Num() == 1)
    {
        if (!Authority->BindParticipant(LocalController, Identity.OriginalParticipants[0].AccountId))
        {
            OutError = FText::FromString(TEXT("로컬 참가자를 전투에 연결하지 못했습니다."));
            return false;
        }
    }
    if (GetNetMode() != NM_Standalone)
    {
        AGameplayGameModeBase* Mode = GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
        if (!Mode || !Mode->ApplyCombatParticipantBindings(Authority))
        {
            OutError = FText::FromString(TEXT("원래 참가자 모두의 서버 연결 배정이 필요합니다."));
            return false;
        }
    }
    return true;
}

void AEncounterManager::EnableCombatCheckpoints()
{
    if (CombatManager && RunState->IsCheckpointSavingEnabled() && RunState->GetRunIdentity().Origin != ERunIdentityOrigin::LegacyOffline)
    {
        CombatManager->CommitTurnBoundary.BindUObject(this, &AEncounterManager::CommitTurnCheckpoint);
    }
}

bool AEncounterManager::BuildTurnCheckpoint(int32 CompletedTurnSerial, int32 NextTurnIndex, FCombatCheckpointData& OutCheckpoint, FText& OutError)
{
    if (!HasAuthority() || !CombatManager || !CombatManager->IsAwaitingTurnCheckpoint() || !RunState || RunState->GetPhase() != ERunPhase::Combat || bPreparing || PendingResult != ECombatResult::None)
    {
        OutError = FText::FromString(TEXT("다음 턴 시작 전 확정 경계에서만 전투를 저장할 수 있습니다."));
        return false;
    }
    for (TActorIterator<ASkillActorBase> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && SpawnedUnits.Contains(It->GetSourceUnit()))
        {
            OutError = FText::FromString(TEXT("아직 종료되지 않은 스킬 액터가 있어 전투를 저장하지 않습니다."));
            return false;
        }
    }
    FCombatCheckpointData Candidate;
    Candidate.AttemptId = CombatAttemptId;
    const FCombatCheckpointData& Previous = RunState->GetCombatCheckpoint();
    if (Previous.AttemptId == CombatAttemptId && Previous.Revision == MAX_int64)
    {
        OutError = FText::FromString(TEXT("체크포인트 순번의 상한에 도달했습니다."));
        return false;
    }
    Candidate.Revision = Previous.AttemptId == CombatAttemptId ? Previous.Revision + 1 : 1;
    Candidate.Identity = RunState->GetRunIdentity();
    Candidate.NodeId = RunState->GetCurrentNodeId();
    Candidate.EncounterId = RunState->GetCurrentEncounterId();
    Candidate.CompletedTurnSerial = CompletedTurnSerial;
    Candidate.NextTurnIndex = NextTurnIndex;
    Candidate.bHasOpponentSnapshot = bHasFrozenOpponent;
    Candidate.OpponentSnapshot = FrozenOpponentSnapshot;
    Candidate.OpponentCatalog = FrozenOpponentCatalog;
    for (AUnitBase* Unit : CombatManager->GetRegisteredUnits())
    {
        FCombatCheckpointUnit& State = Candidate.Units.AddDefaulted_GetRef();
        if (!IsValid(Unit) || !Unit->CaptureCheckpointState(State, OutError))
        {
            return false;
        }
        FGuid& UnitId = CheckpointUnitIds.FindOrAdd(Unit);
        if (!UnitId.IsValid())
        {
            UnitId = FGuid::NewGuid();
        }
        State.UnitId = UnitId;
        State.CharacterId = CombatManager->GetCharacterId(Unit);
        State.OwnerAccountId = CombatManager->GetOwnerAccountId(Unit);
        for (const TPair<int32, TObjectPtr<AUnitBase>>& Entry : PartyActors)
        {
            if (Entry.Value == Unit)
            {
                State.PartySlot = Entry.Key;
                break;
            }
        }
    }
    // Validation and disk publication operate on a candidate without changing the last committed record.
    // 마지막 확정 기록을 바꾸지 않고 후보 데이터의 검증과 디스크 저장을 진행합니다.
    OutCheckpoint = MoveTemp(Candidate);
    return true;
}

bool AEncounterManager::CommitTurnCheckpoint(int32 CompletedTurnSerial, int32 NextTurnIndex)
{
    FText Error;
    if (PendingTurnCheckpoint.Revision == 0 && !BuildTurnCheckpoint(CompletedTurnSerial, NextTurnIndex, PendingTurnCheckpoint, Error))
    {
        FlowMessage = Error;
        SetPlayerCombatInput(false);
        OnFlowChanged.Broadcast();
        return false;
    }
    if (!RunState->CommitCombatCheckpoint(PendingTurnCheckpoint, Error))
    {
        FlowMessage = Error;
        SetPlayerCombatInput(false);
        OnFlowChanged.Broadcast();
        return false;
    }
    PendingTurnCheckpoint = FCombatCheckpointData();
    FlowMessage = FText::GetEmpty();
    SetPlayerCombatInput(true);
    OnFlowChanged.Broadcast();
    return true;
}

bool AEncounterManager::ValidateRestoreArena(const FCombatCheckpointData& Checkpoint, FText& OutError) const
{
    if (!Arena || !Arena->Grid || !CombatManager || !PartyDefinition || !Definitions.Contains(Checkpoint.EncounterId))
    {
        OutError = FText::FromString(TEXT("저장된 전투를 복원할 아레나 또는 콘텐츠 설정이 없습니다."));
        return false;
    }
    if (!UCombatCheckpointLibrary::Validate(Checkpoint, RunState->GetPartyMembers(), OutError))
    {
        return false;
    }
    for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
    {
        if (Unit.Team == ETeam::Player)
        {
            const FRunPartyMember* Member = RunState->GetPartyMembers().FindByPredicate([&Unit](const FRunPartyMember& Entry) { return Entry.SlotIndex == Unit.PartySlot && Entry.bCreated; });
            FProfessionDefinition Profession;
            if (!Member || !PartyDefinition->ResolveProfession(Member->ClassId, Profession) || Unit.UnitClass != FSoftObjectPath(Profession.CombatClass.Get()))
            {
                OutError = FText::FromString(TEXT("저장된 유닛 클래스가 원래 캐릭터의 직업 정의와 일치하지 않습니다."));
                return false;
            }
        }
        ACombatGridTile* Tile = Unit.bHasTile ? Arena->Grid->GetTileAtCoord(Unit.GridCoord) : nullptr;
        const ETileTerritory Expected = Unit.Team == ETeam::Player ? ETileTerritory::Player : ETileTerritory::Enemy;
        if (Unit.bHasTile && (!Tile || Tile->GetTerritory() != Expected || Tile->GetOccupyingUnit()))
        {
            OutError = FText::FromString(TEXT("저장된 배치가 현재 아레나의 빈 진영 타일과 일치하지 않습니다."));
            return false;
        }
        if (Tile && FVector::DistSquaredXY(Unit.Transform.GetLocation(), Tile->GetActorLocation()) > FMath::Square(5.0f))
        {
            OutError = FText::FromString(TEXT("저장된 유닛의 실제 위치가 점유 타일의 중심과 일치하지 않습니다."));
            return false;
        }
    }
    return true;
}

bool AEncounterManager::RestoreSavedCombat(const FRunAccountId& HostAccount, FText& OutError)
{
    ON_SCOPE_EXIT
    {
        if (HasAuthority() && SpawnedUnits.IsEmpty() && !OutError.IsEmpty() && !FlowMessage.EqualTo(OutError))
        {
            FlowMessage = OutError;
            OnFlowChanged.Broadcast();
        }
    };
    if (!HasAuthority() || bPreparing || PendingResult != ECombatResult::None || !RunState || !RunState->HasCombatCheckpoint() || RunState->GetPhase() != ERunPhase::Combat || !SpawnedUnits.IsEmpty())
    {
        OutError = FText::FromString(TEXT("빈 서버 전투 월드에서 저장된 전투를 복원해야 합니다."));
        return false;
    }
    if (!RunState->ValidateCheckpointHost(HostAccount, OutError))
    {
        return false;
    }
    const FCombatCheckpointData Checkpoint = RunState->GetCombatCheckpoint();
    AGameplayGameModeBase* Mode = GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
    if (GetNetMode() == NM_Standalone)
    {
        if (Checkpoint.Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Checkpoint.Identity.OriginalParticipants.Num() != 1)
        {
            OutError = FText::FromString(TEXT("협동 기록은 기존 Host와 원래 참가자가 연결된 세션에서 복원해야 합니다."));
            return false;
        }
    }
    else if (!Mode || !Mode->HasOriginalHostConnection(HostAccount))
    {
        OutError = FText::FromString(TEXT("기존 Host 계정이 이 Listen Server의 로컬 연결에 배정되어야 합니다."));
        return false;
    }
    if (!ValidateRestoreArena(Checkpoint, OutError))
    {
        return false;
    }
    bPreparing = true;
    SetPlayerCombatInput(false);
    FlowMessage = FText::GetEmpty();
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FCombatCheckpointUnit& State : Checkpoint.Units)
    {
        UClass* UnitClass = Cast<UClass>(State.UnitClass.ResolveObject());
        AUnitBase* Unit = GetWorld()->SpawnActor<AUnitBase>(UnitClass, State.Transform, Params);
        if (!Unit)
        {
            return FailRestore(FText::FromString(TEXT("저장된 유닛을 생성하지 못했습니다.")), OutError);
        }
        SpawnedUnits.Add(Unit);
        if (!Unit->RestoreCheckpointState(State, OutError))
        {
            return FailRestore(OutError, OutError);
        }
        if (State.bHasTile)
        {
            Unit->SetCurrentTile(Arena->Grid->GetTileAtCoord(State.GridCoord));
        }
        if (State.PartySlot != INDEX_NONE)
        {
            PartyActors.Add(State.PartySlot, Unit);
        }
        CheckpointUnitIds.Add(Unit, State.UnitId);
    }
    CombatManager->SetCombatGrid(Arena->Grid);
    TArray<AUnitBase*> Units;
    for (AUnitBase* Unit : SpawnedUnits)
    {
        Units.Add(Unit);
    }
    CombatManager->RegisterUnits(Units);
    if (!ConfigureCombatParticipants(OutError))
    {
        return FailRestore(OutError, OutError);
    }
    CombatAttemptId = Checkpoint.AttemptId;
    bHasFrozenOpponent = Checkpoint.bHasOpponentSnapshot;
    FrozenOpponentSnapshot = Checkpoint.OpponentSnapshot;
    FrozenOpponentCatalog = Checkpoint.OpponentCatalog;
    PendingTurnCheckpoint = FCombatCheckpointData();
    EnableCombatCheckpoints();
    Arena->ActivateArena(GetWorld()->GetFirstPlayerController());
    bPreparing = false;
    if (!CombatManager->RestoreCombatFromBoundary(Checkpoint.CompletedTurnSerial, Checkpoint.NextTurnIndex))
    {
        return FailRestore(FText::FromString(TEXT("저장된 턴 경계를 활성화하지 못했습니다.")), OutError);
    }
    SetPlayerCombatInput(CombatManager->IsCombatActive());
    OnFlowChanged.Broadcast();
    OutError = FText::GetEmpty();
    return true;
}

bool AEncounterManager::FailRestore(const FText& Error, FText& OutError)
{
    const FText SavedError = Error;
    CleanupEncounter();
    bPreparing = false;
    FlowMessage = SavedError;
    OutError = SavedError;
    OnFlowChanged.Broadcast();
    return false;
}

bool AEncounterManager::CanRetryCombatCheckpoint() const
{
    return HasAuthority() && RunState && !bPreparing && ((CombatManager && CombatManager->IsAwaitingTurnCheckpoint()) || (PendingResult != ECombatResult::None && !RunState->GetSaveError().IsEmpty()));
}

bool AEncounterManager::RetryCombatCheckpoint(FText& OutError)
{
    if (!CanRetryCombatCheckpoint())
    {
        OutError = FText::FromString(TEXT("재시도할 전투 저장이 없습니다."));
        return false;
    }
    if (PendingResult != ECombatResult::None)
    {
        FinishEncounter();
    }
    else if (CombatManager)
    {
        CombatManager->RetryTurnCheckpoint();
    }
    if (CanRetryCombatCheckpoint())
    {
        OutError = FlowMessage;
        return false;
    }
    FlowMessage = FText::GetEmpty();
    SetPlayerCombatInput(CombatManager && CombatManager->IsCombatActive());
    OnFlowChanged.Broadcast();
    OutError = FText::GetEmpty();
    return true;
}

void AEncounterManager::SuspendForDisconnectedParticipant()
{
    if (!HasAuthority() || !CombatManager || !RunState || RunState->GetPhase() != ERunPhase::Combat)
    {
        return;
    }
    SetPlayerCombatInput(false);
    CombatManager->SuspendCombatForRecovery();
    FlowMessage = FText::FromString(TEXT("원래 참가자의 연결이 끊겨 전투를 중단했습니다. 기존 Host와 원래 참가자가 다시 모여 마지막 확정 턴부터 복원하세요."));
    OnFlowChanged.Broadcast();
}

bool AEncounterManager::SpawnEncounter(UEncounterDefinitionDataAsset* Definition)
{
    bHasFrozenOpponent = false;
    FrozenOpponentSnapshot = FPartySnapshot();
    FrozenOpponentCatalog.Reset();
    const bool bUseSnapshot = !Definition->OpponentSnapshotSlot.IsNone();
    FPartySnapshot Snapshot;
    TArray<TArray<TObjectPtr<USkillDefinitionDataAsset>>> SnapshotSkills;
    if (bUseSnapshot)
    {
        // Validate the entire saved party before spawning any encounter unit.
        // 인카운터 유닛을 생성하기 전에 저장된 파티 전체를 검증합니다.
        if (!IsValid(Definition->SnapshotCatalog))
        {
            FlowMessage = FText::FromString(TEXT("Opponent Snapshot catalog is missing. / 상대 스냅샷 카탈로그가 없습니다."));
            return false;
        }
        if (!UPartySnapshotLibrary::LoadSnapshot(Definition->OpponentSnapshotSlot, Snapshot, FlowMessage) || !Definition->SnapshotCatalog->ValidateForEncounter(Snapshot, Arena->EnemyCoords.Num(), FlowMessage))
        {
            return false;
        }
        bHasFrozenOpponent = true;
        FrozenOpponentSnapshot = Snapshot;
        FrozenOpponentCatalog = FSoftObjectPath(Definition->SnapshotCatalog);
        TSet<ACombatGridTile*> FormationTiles;
        for (const FPartySnapshotMember& Member : Snapshot.Members)
        {
            if (!Arena->EnemyCoords.IsValidIndex(Member.FormationSlot))
            {
                FlowMessage = FText::FromString(TEXT("Opponent Snapshot formation is outside this arena. / 상대 스냅샷 배치가 아레나 범위를 벗어났습니다."));
                return false;
            }
            ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Arena->EnemyCoords[Member.FormationSlot]);
            if (!Tile || Tile->GetOccupyingUnit() || Tile->GetTerritory() != ETileTerritory::Enemy || FormationTiles.Contains(Tile))
            {
                FlowMessage = FText::FromString(TEXT("Opponent Snapshot formation requires distinct empty enemy tiles. / 상대 스냅샷 배치에는 중복되지 않는 빈 적 타일이 필요합니다."));
                return false;
            }
            FormationTiles.Add(Tile);
            TArray<TObjectPtr<USkillDefinitionDataAsset>>& Skills = SnapshotSkills.AddDefaulted_GetRef();
            if (!Definition->SnapshotCatalog->ResolveSkills(Member, Skills, FlowMessage))
            {
                return false;
            }
        }
    }
    else if (Definition->EnemyUnitClasses.IsEmpty() || Definition->EnemyUnitClasses.Num() > Arena->EnemyCoords.Num())
    {
        return false;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = this;
    for (const FRunPartyMember& Member : RunState->GetPartyMembers())
    {
        if (!Member.bCreated || Member.CurrentHP == 0.f)
        {
            continue;
        }
        if (!Arena->PlayerCoords.IsValidIndex(Member.SlotIndex))
        {
            return false;
        }
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Arena->PlayerCoords[Member.SlotIndex]);
        FProfessionDefinition Profession;
        if (!PartyDefinition->ResolveProfession(Member.ClassId, Profession))
        {
            return false;
        }
        TSubclassOf<APlayerUnit> UnitClass = Profession.CombatClass;
        if (!Tile || Tile->GetOccupyingUnit() || Tile->GetTerritory() != ETileTerritory::Player || !UnitClass)
        {
            return false;
        }
        APlayerUnit* Unit = GetWorld()->SpawnActor<APlayerUnit>(UnitClass, Tile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, 90.f, 0.f), Params);
        if (!Unit)
        {
            return false;
        }
        SpawnedUnits.Add(Unit);
        PartyActors.Add(Member.SlotIndex, Unit);
        if (!Unit->ConfigureProfession(Profession.MaxHP, Profession.ActionPoints, Profession.SubActionPoints, Profession.StartingSkills))
        {
            return false;
        }
        Unit->AcquireSkillFromPool(PartyDefinition->EncounterSkillPool);
        Unit->RuntimeCharacterName = Member.CharacterName;
        Unit->SetTeam(ETeam::Player);
        Unit->SetCurrentTile(Tile);
        if (Member.CurrentHP >= 0.f && Unit->GetAttributeSet())
        {
            Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), FMath::Clamp(Member.CurrentHP, 0.f, Unit->GetAttributeSet()->GetMaxHP()));
        }
    }
    if (PartyActors.IsEmpty())
    {
        return false;
    }
    const int32 EnemyCount = bUseSnapshot ? Snapshot.Members.Num() : Definition->EnemyUnitClasses.Num();
    for (int32 Index = 0; Index < EnemyCount; ++Index)
    {
        const FPartySnapshotMember* Member = bUseSnapshot ? &Snapshot.Members[Index] : nullptr;
        const int32 FormationSlot = Member ? Member->FormationSlot : Index;
        TSubclassOf<AEnemyUnit> EnemyClass = Member ? Definition->SnapshotCatalog->EnemyClasses.FindRef(Member->ClassId) : Definition->EnemyUnitClasses[Index];
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Arena->EnemyCoords[FormationSlot]);
        if (!Tile || Tile->GetOccupyingUnit() || Tile->GetTerritory() != ETileTerritory::Enemy || !EnemyClass)
        {
            return false;
        }
        AEnemyUnit* Unit = GetWorld()->SpawnActor<AEnemyUnit>(EnemyClass, Tile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, -90.f, 0.f), Params);
        if (!Unit)
        {
            return false;
        }
        SpawnedUnits.Add(Unit);
        if (Member)
        {
            if (!Unit->ConfigureProfession(Member->Stats.MaxHP, Member->Stats.MaxActionPoints, Member->Stats.MaxSubActionPoints, SnapshotSkills[Index]))
            {
                FlowMessage = FText::FromString(TEXT("Opponent Snapshot unit configuration failed. / 상대 스냅샷 유닛 설정에 실패했습니다."));
                return false;
            }
            Unit->RuntimeCharacterName = FText::FromString(Member->CharacterName);
            if (!Unit->ConfigureMoveRange(Member->Stats.MoveRange))
            {
                return false;
            }
            // Enemy item decisions are unsupported, so snapshot actors receive no implicit healing items.
            // 적 아이템 판단은 미지원이므로 스냅샷 액터에는 암묵적인 회복 아이템을 지급하지 않습니다.
            Unit->HealingItemCount = 0;
            Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Member->Stats.CurrentHP);
        }
        Unit->SetTeam(ETeam::Enemy);
        Unit->SetCurrentTile(Tile);
    }
    return true;
}

void AEncounterManager::HandleCombatResult(ECombatResult Result)
{
    if (PendingResult != ECombatResult::None || !RunState || RunState->GetPhase() != ERunPhase::Combat || Result == ECombatResult::None)
    {
        return;
    }
    PendingResult = Result;
    SetPlayerCombatInput(false);
    // Defer destruction until damage, death and ability callbacks have unwound.
    // 피해, 사망, 어빌리티 콜백이 반환된 다음에 액터를 정리합니다.
    FinishTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AEncounterManager::FinishEncounter);
}

void AEncounterManager::FinishEncounter()
{
    for (const TPair<int32, TObjectPtr<AUnitBase>>& Entry : PartyActors)
    {
        if (IsValid(Entry.Value) && Entry.Value->GetAttributeSet())
        {
            float HP = Entry.Value->GetAttributeSet()->GetHP();
            if (!Entry.Value->IsUnitAlive())
            {
                HP = 0.f;
            }
            RunState->UpdatePartyMemberHP(Entry.Key, HP);
        }
    }
    // Publish the result only after its durable record succeeds; retry keeps the same pending result.
    // 결과 기록이 저장된 뒤 결과를 표시하며 재시도 동안 같은 대기 결과를 유지합니다.
    const ECombatResult Result = PendingResult;
    if (!RunState->CompleteEncounter(Result))
    {
        FlowMessage = RunState->GetSaveError();
        OnFlowChanged.Broadcast();
        return;
    }
    PendingResult = ECombatResult::None;
    if (GetNetMode() == NM_Standalone)
    {
        CleanupEncounter();
    }
    UE_LOG(LogTemp, Log, TEXT("[Encounter] Finished Result=%d RemainingUnits=%d"), static_cast<int32>(Result), SpawnedUnits.Num());
    OnFlowChanged.Broadcast();
}

bool AEncounterManager::ContinueRun()
{
    if (!HasAuthority() || !RunState || PendingResult != ECombatResult::None || RunState->GetPhase() != ERunPhase::Result)
    {
        return false;
    }
    if (!RunState->ContinueRun())
    {
        FlowMessage = RunState->GetSaveError();
        OnFlowChanged.Broadcast();
        return false;
    }
    if (GetNetMode() != NM_Standalone)
    {
        CleanupEncounter();
    }
    return true;
}

bool AEncounterManager::FailPreparation(const FText& Message)
{
    FlowMessage = Message;
    CleanupEncounter();
    bPreparing = false;
    RunState->AbortEncounter();
    UE_LOG(LogTemp, Error, TEXT("[Encounter] %s"), *Message.ToString());
    OnFlowChanged.Broadcast();
    return false;
}

void AEncounterManager::SetPlayerCombatInput(bool bEnabled)
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APartyPlayerController* Controller = Cast<APartyPlayerController>(It->Get()))
        {
            Controller->SetCombatContext(CombatManager, bEnabled);
        }
    }
}

void AEncounterManager::CleanupEncounter()
{
    SetPlayerCombatInput(false);
    if (CombatManager)
    {
        CombatManager->CommitTurnBoundary.Unbind();
        CombatManager->ResetCombat();
    }
    for (TActorIterator<ASkillActorBase> It(GetWorld()); It; ++It)
    {
        if (SpawnedUnits.Contains(It->GetSourceUnit()))
        {
            It->Destroy();
        }
    }
    for (AUnitBase* Unit : SpawnedUnits)
    {
        if (IsValid(Unit))
        {
            Unit->OnTurnEnd();
            Unit->CancelCurrentAction();
            Unit->SetCurrentTile(nullptr);
            AController* Controller = Unit->GetController();
            if (Controller)
            {
                Controller->UnPossess();
                Controller->Destroy();
            }
            Unit->Destroy();
        }
    }
    SpawnedUnits.Reset();
    PartyActors.Reset();
    CheckpointUnitIds.Reset();
    PendingTurnCheckpoint = FCombatCheckpointData();
    if (Arena)
    {
        Arena->CleanupArena();
    }
}

void AEncounterManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(FinishTimer);
    if (CombatManager)
    {
        CombatManager->OnCombatResult.RemoveAll(this);
    }
    CleanupEncounter();
    OnFlowChanged.Clear();
    Super::EndPlay(EndPlayReason);
}
