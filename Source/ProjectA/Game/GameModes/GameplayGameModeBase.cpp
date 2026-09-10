#include "Game/GameModes/GameplayGameModeBase.h"
#include "Combat/CombatManager.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AGameplayGameModeBase::AGameplayGameModeBase()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = AGameplayPlayerController::StaticClass();
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
    AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetWorld()->GetFirstPlayerController());
    if (Controller)
    {
        Controller->InitializeGameplay(EncounterManager);
    }
}

void AGameplayGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(InitializeTimer);
    Super::EndPlay(EndPlayReason);
}
