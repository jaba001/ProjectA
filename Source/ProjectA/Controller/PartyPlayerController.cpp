#include "PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Blueprint/UserWidget.h"
#include "Combat/CombatManager.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Abilities/GameplayAbility.h"

APartyPlayerController::APartyPlayerController()
{
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bShowMouseCursor = true;     
}

void APartyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    InitializeCombatManager();
    InitializeHUD();
}

void APartyPlayerController::InitializeCombatManager()
{
    CombatManager = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));

    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] CombatManager not found"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] CombatManager initialized"));
}

void APartyPlayerController::InitializeHUD()
{
    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] HUDWidgetClass is null"));
        return;
    }

    HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);

    if (!HUDWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] HUDWidget creation failed"));
        return;
    }

    HUDWidget->AddToViewport();
    //UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] HUDWidget initialized"));
}

AUnitBase* APartyPlayerController::GetActiveUnit() const
{
    if (!CombatManager)
        return nullptr;

    return CombatManager->GetCurrentUnit();
}

void APartyPlayerController::RequestEndTurn()
{
    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] RequestEndTurn failed | CombatManager is null"));
        return;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] RequestEndTurn failed | ActiveUnit is null"));
        return;
    }

    if (ActiveUnit->IsBusy())
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] RequestEndTurn blocked | ActiveUnit is busy"));
        return;
    }

    CancelTileInputMode();
    CombatManager->RequestEndTurn();
}

bool APartyPlayerController::CanUseActiveUnitAction() const
{
    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    if (!ActiveUnit->IsUnitAlive())
    {
        return false;
    }

    if (!ActiveUnit->IsActiveTurn())
    {
        return false;
    }

    if (ActiveUnit->IsBusy())
    {
        return false;
    }

    return true;
}

bool APartyPlayerController::CanUseActiveUnitActionPoint(int32 Cost) const
{
    if (!CanUseActiveUnitAction())
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    return ActiveUnit->HasEnoughActionPoint(Cost);
}

bool APartyPlayerController::CanUseActiveUnitSubActionPoint(int32 Cost) const
{
    if (!CanUseActiveUnitAction())
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    return ActiveUnit->HasEnoughSubActionPoint(Cost);
}

void APartyPlayerController::SetSelectedTile(ACombatGridTile* InTile)
{
    SelectedTile = InTile;

    if (!InTile)
    {
        return;
    }

    AUnitBase* OccupyingUnit = InTile->GetOccupyingUnit();

    if (OccupyingUnit)
    {
        //UE_LOG(LogTemp, Log, TEXT("[PC] Selected Tile (%d,%d) | Unit=%s"), InTile->GridCoord.X, InTile->GridCoord.Y, *OccupyingUnit->GetName());
    }
    else
    {
        //UE_LOG(LogTemp, Log, TEXT("[PC] Selected Tile (%d,%d) | Unit=None"), InTile->GridCoord.X, InTile->GridCoord.Y);
    }
}

ACombatGridTile* APartyPlayerController::GetSelectedTile() const
{
    return SelectedTile;
}

void APartyPlayerController::ClearSelectedTile()
{
    SelectedTile = nullptr;
    //UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] SelectedTile cleared by ClearSelectedTile"));
}

void APartyPlayerController::SetTileInputMode(ETileInputMode NewMode)
{
    CurrentTileInputMode = NewMode;
    //UE_LOG(LogTemp, Log, TEXT("[PartyPlayerController] TileInputMode Changed | Mode=%d"), static_cast<uint8>(CurrentTileInputMode));
}

void APartyPlayerController::EnterMoveMode()
{
    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterMoveMode failed | CombatManager is null"));
        return;
    }

    if (!CanUseActiveUnitSubActionPoint(1))
    {
        return;
    }

    CombatManager->ClearMovableTilesHighlight();
    ClearSelectedTile();

    SetTileInputMode(ETileInputMode::Move);
    CombatManager->RefreshReachableMoveTiles();
    CombatManager->HighlightMovableTiles();
}

void APartyPlayerController::EnterSkillMode(USkillDefinitionDataAsset* SkillData)
{

    if (!CombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | CombatManager is null"));
        return;
    }

    if (!SkillData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | SkillData is null"));
        return;
    }

    if (!SkillData->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | AbilityClass is null"));
        return;
    }

    if (SkillData->ActionPointCost <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PartyPlayerController] EnterSkillMode failed | Invalid ActionPointCost=%d"), SkillData->ActionPointCost);
        return;
    }

    if (!CanUseActiveUnitActionPoint(SkillData->ActionPointCost))
    {
        return;
    }

    CombatManager->ClearMovableTilesHighlight();
    CombatManager->ClearSkillTargetTilesHighlight();
    ClearSelectedTile();

    PendingSkillData = SkillData;

    SetTileInputMode(ETileInputMode::Skill);
    CombatManager->RefreshSkillTargetTiles();
    CombatManager->HighlightSkillTargetTiles();
}

void APartyPlayerController::CancelTileInputMode()
{
    if (CombatManager)
    {
        CombatManager->ClearMovableTilesHighlight();
        CombatManager->ClearSkillTargetTilesHighlight();
    }

    PendingSkillData = nullptr;
    SetTileInputMode(ETileInputMode::None);
    ClearSelectedTile();
}

bool APartyPlayerController::IsValidTileForPendingSkill(ACombatGridTile* Tile) const
{
    if (!PendingSkillData || !Tile)
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    AUnitBase* TargetUnit = Tile->GetOccupyingUnit();
    const ETeam ActiveTeam = ActiveUnit->GetTeam();
    const ETileTerritory TileTerritory = Tile->GetTerritory();

    // Check whether the tile belongs to enemy territory for the active unit.
    auto IsEnemyTileForActiveTeam = [&]() -> bool
        {
            return (ActiveTeam == ETeam::Player && TileTerritory == ETileTerritory::Enemy)
                || (ActiveTeam == ETeam::Enemy && TileTerritory == ETileTerritory::Player);
        };

    // Check whether the tile belongs to ally territory for the active unit.
    auto IsAllyTileForActiveTeam = [&]() -> bool
        {
            return (ActiveTeam == ETeam::Player && TileTerritory == ETileTerritory::Player)
                || (ActiveTeam == ETeam::Enemy && TileTerritory == ETileTerritory::Enemy);
        };

    // Check whether the tile has a living target unit.
    auto HasAliveTargetUnit = [&]() -> bool
        {
            return TargetUnit && TargetUnit->IsUnitAlive();
        };

    // Check whether the target unit is an enemy of the active unit.
    auto IsEnemyTargetUnit = [&]() -> bool
        {
            return HasAliveTargetUnit() && TargetUnit->GetTeam() != ActiveTeam;
        };

    // Check whether the target unit is an ally of the active unit.
    auto IsAllyTargetUnit = [&]() -> bool
        {
            return HasAliveTargetUnit() && TargetUnit->GetTeam() == ActiveTeam;
        };

    // Check whether front protection blocks this tile.
    auto IsBlockedByFrontProtection = [&]() -> bool
        {
            return !PendingSkillData->bIgnoreFront && Tile->GetProtectedByFront();
        };

    // Dead units on a tile should never be treated as valid targets.
    if (TargetUnit && !TargetUnit->IsUnitAlive())
    {
        return false;
    }

    switch (PendingSkillData->TargetRule)
    {
    case ESkillTargetRule::EnemyUnit:
    {
        if (!IsEnemyTargetUnit())
        {
            return false;
        }

        if (IsBlockedByFrontProtection())
        {
            return false;
        }

        return true;
    }
    case ESkillTargetRule::AllyUnit:
    {
        return IsAllyTargetUnit();
    }
    case ESkillTargetRule::AnyUnit:
    {
        return HasAliveTargetUnit();
    }
    case ESkillTargetRule::EnemyTile:
    {
        return IsEnemyTileForActiveTeam();
    }
    case ESkillTargetRule::AllyTile:
    {
        return IsAllyTileForActiveTeam();
    }
    case ESkillTargetRule::AnyTile:
    {
        return TileTerritory != ETileTerritory::None;
    }
    default:
    {
        return false;
    }
    }
}