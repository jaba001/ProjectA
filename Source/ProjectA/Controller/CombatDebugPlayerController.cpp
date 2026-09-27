#include "Controller/CombatDebugPlayerController.h"
#include "Game/Development/CombatDebugLoadout.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "UI/Debug/CombatDebugWidget.h"

void ACombatDebugPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController() || !ACombatDebugGameMode::IsDebugWorld(GetWorld())) return;
    DebugLoadout = NewObject<UCombatDebugLoadout>(this);
    DebugWidget = CreateWidget<UCombatDebugWidget>(this);
    if (DebugWidget) DebugWidget->AddToViewport();
}

UCombatDebugLoadout* ACombatDebugPlayerController::GetDebugLoadout()
{
    if (!ACombatDebugGameMode::IsDebugWorld(GetWorld())) return nullptr;
    if (DebugLoadout && GetCombatManager()) DebugLoadout->Initialize(GetCombatManager());
    return DebugLoadout;
}

void ACombatDebugPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (DebugWidget) DebugWidget->RemoveFromParent();
    DebugWidget = nullptr;
    DebugLoadout = nullptr;
    Super::EndPlay(EndPlayReason);
}
