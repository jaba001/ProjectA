#include "Game/Encounter/EncounterManager.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
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
#include "Game/Run/RunParticipationLibrary.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
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
    if (!HasAuthority() || bShuttingDown || bPreparing || bPreparationAbortPending || PendingResult != ECombatResult::None || !RunState || !RunState->CanStartNode(NodeId))
    {
        return false;
    }
    if (!ValidateManagedExecution(FlowMessage))
    {
        OnFlowChanged.Broadcast();
        return false;
    }
    bPreparing = true;
    PreparationFailureMessage = FText::GetEmpty();
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
    Arena->ActivateArena(GetWorld()->GetFirstPlayerController());
    if (!RunState->MarkCombatStarted())
    {
        return FailPreparation(FText::FromString(TEXT("Run 전투 상태를 활성화하지 못했습니다.")));
    }
    bPreparing = false;
    CombatManager->StartCombat_Internal();
    if (!CombatManager->IsCombatActive())
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
    if (!Authority || !Authority->ConfigureRun(RunState->GetRunIdentity(), RunState->GetPartyMembers(), PartyActors, OutError, RunState->IsManagedRun()))
    {
        return false;
    }
    if (RunState->IsManagedRun())
    {
        if (!ValidateManagedExecution(OutError, true))
        {
            return false;
        }
        // The persistent roster determines control when each encounter creates its actors.
        // 각 전투의 액터를 생성할 때 영속 참가 목록으로 조작 방식을 결정합니다.
        for (const TPair<int32, TObjectPtr<AUnitBase>>& Entry : PartyActors)
        {
            APlayerUnit* Player = Cast<APlayerUnit>(Entry.Value);
            EPartyControlMode ControlMode = EPartyControlMode::Human;
            if (!Player || !URunParticipationLibrary::ResolveControlMode(RunState->GetParticipation(), RunState->GetRunIdentity(), RunState->GetPartyMembers(), Authority->GetCharacterId(Player), ControlMode, OutError) || !Authority->SetPartyControlMode(Player, ControlMode, OutError))
            {
                return false;
            }
        }
        AGameplayGameModeBase* Mode = GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
        if (!Mode || !Mode->ApplyCombatParticipantBindings(Authority))
        {
            OutError = FText::FromString(TEXT("현재 인간 참가자의 전투 연결 배정을 완료하지 못했습니다."));
            return false;
        }
        return true;
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

bool AEncounterManager::ValidateManagedExecution(FText& OutError, bool bAllowResumePending) const
{
    if (!RunState || !RunState->IsManagedRun())
    {
        return true;
    }
    OutError = FText::FromString(TEXT("현재 Host의 관리 Run 실행 권한과 완료된 복원이 필요합니다."));
    if (!HasAuthority() || bShuttingDown || !RunState->HasManagedLease() || (!bAllowResumePending && RunState->IsManagedResumePending()))
    {
        return false;
    }
    const AGameplayGameModeBase* Mode = GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
    return Mode && Mode->ValidateManagedRunConnections(OutError);
}

bool AEncounterManager::ResumeManagedGameplay(FText& OutError)
{
    OutError = FText::FromString(TEXT("재개할 관리 Run과 현재 Host의 실행 권한이 필요합니다."));
    if (!RunState || !RunState->IsManagedRun() || !ValidateManagedExecution(OutError, true))
    {
        FlowMessage = OutError;
        OnFlowChanged.Broadcast();
        return false;
    }
    if (RunState->GetPhase() == ERunPhase::Combat)
    {
        return RestoreSavedCombat(RunState->GetRunIdentity().HostAccountId, OutError);
    }
    const ERunPhase Phase = RunState->GetPhase();
    if ((Phase != ERunPhase::Map && Phase != ERunPhase::Result && Phase != ERunPhase::EncounterChoice && Phase != ERunPhase::Shop) || !SpawnedUnits.IsEmpty() || !RunState->ConfirmManagedResumeStarted(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = FText::FromString(TEXT("진행 지도·승리 결과·인카운터 선택·상점에서만 전투 없는 관리 재개를 완료할 수 있습니다."));
        }
        FlowMessage = OutError;
        OnFlowChanged.Broadcast();
        return false;
    }
    FlowMessage = FText::GetEmpty();
    OnFlowChanged.Broadcast();
    return true;
}

bool AEncounterManager::RestoreSavedCombat(const FRunAccountId&, FText& OutError)
{
    // Keep old callers fail-closed without interpreting sequential saves as timed rounds.
    // 기존 호출은 명시적으로 거절하며 순차 턴 저장을 시간 기반 라운드로 해석하지 않습니다.
    OutError = FText::FromString(TEXT("기존 순차 턴 전투 저장은 새 라운드 전투에서 복원할 수 없습니다. 새 전투의 라운드 중간 저장·복구는 아직 지원하지 않으며 기존 저장 파일은 보존됩니다."));
    FlowMessage = OutError;
    OnFlowChanged.Broadcast();
    return false;
}

bool AEncounterManager::CanRetryCombatCheckpoint() const
{
    FText Error;
    if (!HasAuthority() || bShuttingDown || !RunState || bPreparing || !ValidateManagedExecution(Error, true))
    {
        return false;
    }
    const bool bCanResumeOutsideCombat = RunState->GetPhase() != ERunPhase::Combat && RunState->IsManagedRun() && RunState->IsManagedResumePending() && SpawnedUnits.IsEmpty();
    return bPreparationAbortPending || bCanResumeOutsideCombat || (PendingResult != ECombatResult::None && !RunState->GetSaveError().IsEmpty());
}

bool AEncounterManager::RetryCombatCheckpoint(FText& OutError)
{
    if (!CanRetryCombatCheckpoint())
    {
        OutError = FText::FromString(TEXT("재시도할 전투 저장이 없습니다."));
        return false;
    }
    if (bPreparationAbortPending)
    {
        const bool bAborted = TryAbortPreparation();
        OutError = bAborted ? FText::GetEmpty() : FlowMessage;
        return bAborted;
    }
    if (RunState->IsManagedRun() && RunState->IsManagedResumePending() && SpawnedUnits.IsEmpty())
    {
        return ResumeManagedGameplay(OutError);
    }
    if (PendingResult != ECombatResult::None)
    {
        FinishEncounter();
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
    FlowMessage = FText::FromString(TEXT("원래 참가자의 연결이 끊겨 전투를 중단했습니다. 전투 중간 복구는 미지원입니다. 메뉴에서 마지막 전투 외 저장부터 명시적으로 재개해야 하며 자동 Host 승계나 AI 전환은 수행하지 않습니다."));
    OnFlowChanged.Broadcast();
}

bool AEncounterManager::SpawnEncounter(UEncounterDefinitionDataAsset* Definition)
{
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
    if (bShuttingDown || PendingResult != ECombatResult::None || !RunState || RunState->GetPhase() != ERunPhase::Combat || Result == ECombatResult::None)
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
    if (bShuttingDown || !ValidateManagedExecution(FlowMessage))
    {
        SetPlayerCombatInput(false);
        OnFlowChanged.Broadcast();
        return;
    }
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
    if (!HasAuthority() || bShuttingDown || !RunState || PendingResult != ECombatResult::None || RunState->GetPhase() != ERunPhase::Result)
    {
        return false;
    }
    if (!ValidateManagedExecution(FlowMessage))
    {
        OnFlowChanged.Broadcast();
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

bool AEncounterManager::SelectRunEncounter(FName EncounterId)
{
    if (!HasAuthority() || bShuttingDown || bPreparing || PendingResult != ECombatResult::None || !RunState) return false;
    bool bSucceeded = false;
    if (ValidateManagedExecution(FlowMessage))
    {
        bSucceeded = RunState->SelectRunEncounter(EncounterId);
        FlowMessage = RunState->GetSaveError();
    }
    OnFlowChanged.Broadcast();
    return bSucceeded;
}

bool AEncounterManager::LeaveRunEncounter()
{
    if (!HasAuthority() || bShuttingDown || bPreparing || PendingResult != ECombatResult::None || !RunState) return false;
    bool bSucceeded = false;
    if (ValidateManagedExecution(FlowMessage))
    {
        bSucceeded = RunState->LeaveRunEncounter();
        FlowMessage = RunState->GetSaveError();
    }
    OnFlowChanged.Broadcast();
    return bSucceeded;
}

bool AEncounterManager::FailPreparation(const FText& Message)
{
    PreparationFailureMessage = Message;
    bPreparationAbortPending = true;
    CleanupEncounter();
    bPreparing = false;
    TryAbortPreparation();
    UE_LOG(LogTemp, Error, TEXT("[Encounter] %s"), *Message.ToString());
    return false;
}

bool AEncounterManager::TryAbortPreparation()
{
    // Clear the retry flag before a successful abort publishes the map transition synchronously.
    // 취소 성공이 지도 전환을 동기 통지하기 전에 재시도 플래그를 해제합니다.
    bPreparationAbortPending = false;
    FlowMessage = PreparationFailureMessage;
    if (!RunState->AbortEncounter())
    {
        bPreparationAbortPending = true;
        const FText Error = RunState->GetSaveError().IsEmpty() ? FText::FromString(TEXT("전투 준비 취소를 완료하지 못했습니다. 저장 다시 시도를 사용하세요.")) : RunState->GetSaveError();
        FlowMessage = FText::Format(FText::FromString(TEXT("{0}\n{1}")), PreparationFailureMessage, Error);
    }
    OnFlowChanged.Broadcast();
    return !bPreparationAbortPending;
}

void AEncounterManager::SetPlayerCombatInput(bool bEnabled)
{
    FText Error;
    bEnabled = bEnabled && !bShuttingDown && ValidateManagedExecution(Error);
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
    if (Arena)
    {
        Arena->CleanupArena();
    }
}

void AEncounterManager::ShutdownGameplay()
{
    if (bShuttingDown)
    {
        return;
    }
    bShuttingDown = true;
    GetWorldTimerManager().ClearTimer(FinishTimer);
    if (CombatManager)
    {
        CombatManager->OnCombatResult.RemoveAll(this);
    }
    CleanupEncounter();
    PendingResult = ECombatResult::None;
    bPreparationAbortPending = false;
    PreparationFailureMessage = FText::GetEmpty();
}

void AEncounterManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ShutdownGameplay();
    OnFlowChanged.Clear();
    Super::EndPlay(EndPlayReason);
}
