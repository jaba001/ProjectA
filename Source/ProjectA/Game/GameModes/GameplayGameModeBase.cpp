#include "Game/GameModes/GameplayGameModeBase.h"
#include "Combat/CombatManager.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "TimerManager.h"

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
    EncounterManager->InitializeEncounter(Arena, Combat, PartyDefinition, EncounterDefinitions);
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
