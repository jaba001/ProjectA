#include "Game/GameModes/GameplayGameModeBase.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Engine/GameInstance.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AGameplayGameModeBase::AGameplayGameModeBase()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = AGameplayPlayerController::StaticClass();
    GameStateClass = AGameplayGameState::StaticClass();
    CombatManagerClass = ACombatManager::StaticClass();
    EncounterManagerClass = AEncounterManager::StaticClass();
}

void AGameplayGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        // Wait for placed grids and the player controller to complete BeginPlay.
        // 배치된 그리드와 플레이어 컨트롤러의 BeginPlay 완료를 기다립니다.
        InitializeTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGameplayGameModeBase::InitializeGameplay);
    }
}

void AGameplayGameModeBase::InitializeGameplay()
{
    FString SlotOverride;
    const bool bUseSnapshot = FParse::Value(FCommandLine::Get(), TEXT("ProjectAOpponentSnapshot="), SlotOverride) || FParse::Param(FCommandLine::Get(), TEXT("ProjectAOpponentSnapshot"));
    const FName SnapshotSlot = SlotOverride.Len() <= 64 ? FName(*SlotOverride) : NAME_None;
    if (bUseSnapshot && UPartySnapshotLibrary::GetSaveSlotName(SnapshotSlot).IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[Gameplay] Invalid ProjectAOpponentSnapshot slot. Use 1-64 ASCII letters, digits or underscores. / 상대 Snapshot 슬롯에는 영문, 숫자, 밑줄로 이루어진 1~64자 식별자가 필요합니다."));
        return;
    }
    ACombatArena* Arena = nullptr;
    for (TActorIterator<ACombatArena> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(ArenaTag))
        {
            Arena = *It;
            break;
        }
        if (!Arena)
        {
            Arena = *It;
        }
    }
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACombatManager* Combat = GetWorld()->SpawnActor<ACombatManager>(CombatManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    EncounterManager = GetWorld()->SpawnActor<AEncounterManager>(EncounterManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!EncounterManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[Gameplay] EncounterManagerClass is missing or could not spawn."));
        return;
    }
    TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>> RuntimeDefinitions = EncounterDefinitions;
    if (bUseSnapshot)
    {
        // Use a runtime definition so local experiments never mutate the authored PvE assets.
        // 로컬 검증이 작성된 PvE 에셋을 수정하지 않도록 런타임 정의를 사용합니다.
        UEncounterDefinitionDataAsset* SnapshotDefinition = NewObject<UEncounterDefinitionDataAsset>(this);
        SnapshotDefinition->OpponentSnapshotSlot = SnapshotSlot;
        SnapshotDefinition->SnapshotCatalog = LocalOpponentCatalog;
        for (TPair<FName, TObjectPtr<UEncounterDefinitionDataAsset>>& Entry : RuntimeDefinitions)
        {
            Entry.Value = SnapshotDefinition;
        }
    }
    EncounterManager->InitializeEncounter(Arena, Combat, PartyDefinition, RuntimeDefinitions);
    if (AGameplayGameState* State = GetGameState<AGameplayGameState>())
    {
        State->InitializeServerView(EncounterManager, Arena);
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(It->Get()))
        {
            Controller->InitializeGameplay(EncounterManager);
        }
    }
    URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    if (Run->IsManagedRun())
    {
        // The trusted development caller owns this local server; never claim an account from a UI field.
        // 신뢰된 개발 호출자가 이 로컬 서버를 소유하며 UI 입력으로 계정을 주장하지 않습니다.
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APartyPlayerController* Controller = Cast<APartyPlayerController>(It->Get());
            if (Controller && Controller->IsLocalController())
            {
                AssignRunParticipant(Controller, Run->GetLocalCaller());
            }
        }
        FText Error;
        if (!EncounterManager->ResumeManagedGameplay(Error))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Gameplay] Managed Run is waiting for resume: %s"), *Error.ToString());
        }
    }
    else if (GetNetMode() == NM_Standalone && Run->HasCombatCheckpoint())
    {
        FText Error;
        if (!EncounterManager->RestoreSavedCombat(Run->GetRunIdentity().HostAccountId, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("[Gameplay] Combat checkpoint restore failed: %s"), *Error.ToString());
        }
    }
}

void AGameplayGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (EncounterManager)
    {
        if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(NewPlayer))
        {
            Controller->InitializeGameplay(EncounterManager);
        }
    }
}

bool AGameplayGameModeBase::AssignRunParticipant(APartyPlayerController* Controller, const FRunAccountId& AccountId)
{
    if (!HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != GetWorld() || !GetGameInstance())
    {
        return false;
    }
    URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    if (!URunIdentityLibrary::IsOriginalParticipant(Run->GetRunIdentity(), AccountId) || (Run->IsManagedRun() && (!Run->HasManagedLease() || Run->GetLocalCaller() != Run->GetRunIdentity().HostAccountId || !Run->GetParticipation().HumanParticipants.Contains(AccountId) || (Controller->IsLocalController() ? AccountId != Run->GetLocalCaller() : AccountId == Run->GetRunIdentity().HostAccountId))))
    {
        return false;
    }
    const TWeakObjectPtr<APartyPlayerController> Key(Controller);
    if (const FRunAccountId* Existing = RunParticipants.Find(Key))
    {
        return *Existing == AccountId;
    }
    for (auto It = RunParticipants.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
        else if (It.Value() == AccountId)
        {
            return false;
        }
    }
    RunParticipants.Add(Key, AccountId);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AGameplayPlayerController* GameplayController = Cast<AGameplayPlayerController>(It->Get()); GameplayController && GameplayController->IsLocalController())
        {
            GameplayController->RefreshRunFlowPermissions();
        }
    }
    return true;
}

bool AGameplayGameModeBase::CanControlRunFlow(const APartyPlayerController* Controller, bool bAllowResumePending) const
{
    if (!HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != GetWorld() || !Controller->HasAuthority() || !Controller->IsLocalController() || !GetGameInstance())
    {
        return false;
    }
    const URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    if (!Run || !Run->GetRunIdentity().RunId.IsValid() || !URunIdentityLibrary::IsOriginalParticipant(Run->GetRunIdentity(), Run->GetRunIdentity().HostAccountId))
    {
        return false;
    }
    if (Run->IsManagedRun())
    {
        FText Error;
        if ((!bAllowResumePending && Run->IsManagedResumePending()) || !ValidateManagedRunConnections(Error))
        {
            return false;
        }
    }
    else if (GetNetMode() != NM_ListenServer)
    {
        return false;
    }
    for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : RunParticipants)
    {
        if (Entry.Key.Get() == Controller)
        {
            return Entry.Value == Run->GetRunIdentity().HostAccountId;
        }
    }
    return false;
}

bool AGameplayGameModeBase::ApplyCombatParticipantBindings(UCombatActionAuthority* Authority)
{
    if (!HasAuthority() || !Authority)
    {
        return false;
    }
    const FRunIdentityData& Identity = Authority->GetRunIdentity();
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (Run && Run->IsManagedRun())
    {
        FText Error;
        if (!ValidateManagedRunConnections(Error) || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Run->GetRunIdentity(), 0))
        {
            return false;
        }
        for (const FRunAccountId& Human : Run->GetParticipation().HumanParticipants)
        {
            for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : RunParticipants)
            {
                if (Entry.Key.IsValid() && Entry.Value == Human && !Authority->BindParticipant(Entry.Key.Get(), Human))
                {
                    return false;
                }
            }
        }
        return true;
    }
    if (Identity.Origin == ERunIdentityOrigin::LegacyOffline || Identity.OriginalParticipants.Num() < 2 || Identity.OriginalParticipants.Num() > 4)
    {
        return false;
    }
    for (const FRunParticipantData& Participant : Identity.OriginalParticipants)
    {
        APartyPlayerController* Match = nullptr;
        for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : RunParticipants)
        {
            if (Entry.Key.IsValid() && Entry.Value == Participant.AccountId)
            {
                Match = Entry.Key.Get();
                break;
            }
        }
        if (!Match || !Authority->BindParticipant(Match, Participant.AccountId))
        {
            return false;
        }
    }
    return true;
}

bool AGameplayGameModeBase::ValidateManagedRunConnections(FText& OutError) const
{
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    OutError = FText::FromString(TEXT("관리 Run을 실행할 현재 Host의 유효한 로컬 권한이 필요합니다."));
    if (!HasAuthority() || !Run || !Run->IsManagedRun() || !Run->HasManagedLease() || Run->GetRunIdentity().Origin != ERunIdentityOrigin::LocalDevelopment || Run->GetLocalCaller() != Run->GetRunIdentity().HostAccountId || (GetNetMode() != NM_Standalone && GetNetMode() != NM_ListenServer))
    {
        return false;
    }
    if (!URunParticipationLibrary::Validate(Run->GetParticipation(), Run->GetRunIdentity(), Run->GetPartyMembers(), OutError))
    {
        return false;
    }
    if (GetNetMode() == NM_Standalone && (Run->GetParticipation().HumanParticipants.Num() != 1 || Run->GetParticipation().HumanParticipants[0] != Run->GetLocalCaller()))
    {
        OutError = FText::FromString(TEXT("싱글 재개에는 현재 Host 한 명만 인간 참가자로 남아 있어야 합니다."));
        return false;
    }
    for (const FRunAccountId& Human : Run->GetParticipation().HumanParticipants)
    {
        APartyPlayerController* Match = nullptr;
        for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : RunParticipants)
        {
            if (Entry.Key.IsValid() && Entry.Value == Human)
            {
                Match = Entry.Key.Get();
                break;
            }
        }
        if (!Match || Match->GetWorld() != GetWorld() || Match->IsLocalController() != (Human == Run->GetRunIdentity().HostAccountId))
        {
            OutError = FText::FromString(TEXT("현재 인간 참가자 모두의 서버 연결 배정이 필요합니다. 불참자는 자동으로 AI 전환되지 않습니다."));
            return false;
        }
    }
    OutError = FText::GetEmpty();
    return true;
}

bool AGameplayGameModeBase::HasOriginalHostConnection(const FRunAccountId& HostAccount) const
{
    if (!HasAuthority() || GetNetMode() != NM_ListenServer)
    {
        return false;
    }
    for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : RunParticipants)
    {
        if (Entry.Key.IsValid() && Entry.Key->IsLocalController() && Entry.Value == HostAccount)
        {
            return true;
        }
    }
    return false;
}

void AGameplayGameModeBase::Logout(AController* Exiting)
{
    APartyPlayerController* Participant = Cast<APartyPlayerController>(Exiting);
    if (Participant && RunParticipants.Contains(Participant))
    {
        // A lost original participant pauses combat without changing the host or assigning AI.
        // 원래 참가자의 연결이 끊기면 Host 변경이나 AI 배정 없이 전투를 멈춥니다.
        if (IsValid(EncounterManager))
        {
            EncounterManager->SuspendForDisconnectedParticipant();
        }
        RunParticipants.Remove(Participant);
    }
    Super::Logout(Exiting);
}

void AGameplayGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(InitializeTimer);
    URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (Run && Run->IsManagedRun())
    {
        // Stop actors and callbacks before releasing the execution lease retained by the GameInstance.
        // GameInstance가 보유한 실행 lease를 반환하기 전에 액터와 콜백을 중단합니다.
        if (IsValid(EncounterManager))
        {
            EncounterManager->ShutdownGameplay();
        }
        Run->CloseManagedRun();
    }
    Super::EndPlay(EndPlayReason);
}
