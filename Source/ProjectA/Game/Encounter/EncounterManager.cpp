#include "Game/Encounter/EncounterManager.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
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
    UCombatActionAuthority* ActionAuthority = CombatManager->GetActionAuthority();
    if (!ActionAuthority)
    {
        return FailPreparation(FText::FromString(TEXT("Combat action authority is unavailable. / 전투 행동 검증 객체가 없습니다.")));
    }
    FText AuthorityError;
    const FRunIdentityData& Identity = RunState->GetRunIdentity();
    if (!ActionAuthority->ConfigureRun(Identity, RunState->GetPartyMembers(), PartyActors, AuthorityError))
    {
        return FailPreparation(AuthorityError);
    }
    // Bind only the existing local solo participant; other connections require explicit server assignment.
    // 기존 로컬 1인 참가자만 연결하며 다른 접속은 명시적인 서버 배정이 필요합니다.
    APartyPlayerController* LocalController = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (GetNetMode() == NM_Standalone && LocalController && LocalController->IsLocalController() && Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Identity.OriginalParticipants.Num() == 1)
    {
        if (!ActionAuthority->BindParticipant(LocalController, Identity.OriginalParticipants[0].AccountId))
        {
            return FailPreparation(FText::FromString(TEXT("The local participant could not be bound to combat. / 로컬 참가자를 전투에 연결하지 못했습니다.")));
        }
    }
    if (GetNetMode() != NM_Standalone)
    {
        AGameplayGameModeBase* Mode = GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
        if (!Mode || !Mode->ApplyCombatParticipantBindings(ActionAuthority))
        {
            return FailPreparation(FText::FromString(TEXT("Every original participant must have a trusted server connection. / 원래 참가자 모두의 서버 연결 배정이 필요합니다.")));
        }
    }
    Arena->ActivateArena(GetWorld()->GetFirstPlayerController());
    RunState->MarkCombatStarted();
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
    SetPlayerCombatInput(true);
    UE_LOG(LogTemp, Log, TEXT("[Encounter] Started Node=%s Units=%d"), *NodeId.ToString(), SpawnedUnits.Num());
    OnFlowChanged.Broadcast();
    return true;
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
    // Keep final network actor state available through the result screen, until explicit Continue or travel.
    // 네트워크 최종 액터 상태는 결과 화면에서 유지하며 명시적인 Continue나 레벨 이동 때 정리합니다.
    if (GetNetMode() == NM_Standalone)
    {
        CleanupEncounter();
    }
    const ECombatResult Result = PendingResult;
    PendingResult = ECombatResult::None;
    RunState->CompleteEncounter(Result);
    UE_LOG(LogTemp, Log, TEXT("[Encounter] Finished Result=%d RemainingUnits=%d"), static_cast<int32>(Result), SpawnedUnits.Num());
    OnFlowChanged.Broadcast();
}

bool AEncounterManager::ContinueRun()
{
    if (!HasAuthority() || !RunState || PendingResult != ECombatResult::None || RunState->GetPhase() != ERunPhase::Result)
    {
        return false;
    }
    if (GetNetMode() != NM_Standalone)
    {
        CleanupEncounter();
    }
    return RunState->ContinueRun();
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
