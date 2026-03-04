#include "Game/GameModes/CombatGameModeBase.h"
#include "Combat/CombatManager.h"
#include "Engine/World.h"

ACombatGameModeBase::ACombatGameModeBase()
{
    // GameMode는 서버 전용이므로 Replicate 필요 없음
}

void ACombatGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 실행 (GameMode는 원래 서버에만 존재하지만 명시적으로)
    if (!HasAuthority()) return;

    StartCombat();
}

void ACombatGameModeBase::StartCombat()
{
    if (!HasAuthority()) return;

    if (!CombatManagerClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    CombatManager = World->SpawnActor<ACombatManager>(CombatManagerClass);

    if (CombatManager)
    {
        // 서버에서 전투 시작 요청
        CombatManager->Server_StartCombat();
    }
}