#include "Game/GameModes/CombatGameModeBase.h"
#include "Combat/CombatManager.h"
#include "Unit/UnitBase.h"
#include "Engine/World.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"

ACombatGameModeBase::ACombatGameModeBase()
{
}

void ACombatGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // 서버 전용
    if (!HasAuthority()) return;   

    SpawnCombat();
}

void ACombatGameModeBase::SpawnCombat()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (!CombatManagerClass)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatManagerClass is null"));
        return;
    }

    // Grid 찾기
    ACombatGridManager* Grid =
        Cast<ACombatGridManager>(
            UGameplayStatics::GetActorOfClass(
                World,
                ACombatGridManager::StaticClass()
            )
        );

    if (!Grid)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatGridManager not found"));
        return;
    }

    // CombatManager Spawn
    FActorSpawnParameters ManagerParams;
    ManagerParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    CombatManager =
        World->SpawnActor<ACombatManager>(
            CombatManagerClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            ManagerParams
        );

    if (!CombatManager) return;

    TArray<AUnitBase*> AllUnits;

    FActorSpawnParameters UnitParams;
    UnitParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // =========================
    // Player Spawn
    // =========================
    TArray<ACombatGridTile*> PlayerTiles =
        Grid->GetTilesByCoords(PlayerCoords);

    for (int32 i = 0; i < PlayerUnitClasses.Num(); i++)
    {
        if (!PlayerUnitClasses[i]) continue;
        if (!PlayerTiles.IsValidIndex(i)) continue;
        if (!PlayerTiles[i]) continue;

        FVector SpawnLocation = PlayerTiles[i]->GetActorLocation();
        SpawnLocation.Z += 100.f;

        AUnitBase* Unit =
            World->SpawnActor<AUnitBase>(
                PlayerUnitClasses[i],
                SpawnLocation,
                FRotator(0.f, 90.f, 0.f),
                UnitParams
            );

        if (Unit)
        {
            Unit->SetTeam(ETeam::Player);
            Unit->SetCurrentTile(PlayerTiles[i]);
            AllUnits.Add(Unit);
        }
    }

    // =========================
    // Enemy Spawn
    // =========================
    TArray<ACombatGridTile*> EnemyTiles =
        Grid->GetTilesByCoords(EnemyCoords);

    for (int32 i = 0; i < EnemyUnitClasses.Num(); i++)
    {
        if (!EnemyUnitClasses[i]) continue;
        if (!EnemyTiles.IsValidIndex(i)) continue;
        if (!EnemyTiles[i]) continue;

        FVector SpawnLocation = EnemyTiles[i]->GetActorLocation();
        SpawnLocation.Z += 100.f;

        AUnitBase* Unit =
            World->SpawnActor<AUnitBase>(
                EnemyUnitClasses[i],
                SpawnLocation,
                FRotator(0.f, -90.f, 0.f),
                UnitParams
            );

        if (Unit)
        {
            Unit->SetTeam(ETeam::Enemy);   
            Unit->SetCurrentTile(EnemyTiles[i]);
            AllUnits.Add(Unit);
        }
    }

    // CombatManager 등록
    CombatManager->RegisterUnits(AllUnits);

    // GameMode는 서버 전용 → 직접 호출
    CombatManager->StartCombat_Internal();
}