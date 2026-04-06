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
    if (!PendingSkillData)
    {
        return false;
    }

    if (!Tile)
    {
        return false;
    }

    AUnitBase* ActiveUnit = GetActiveUnit();

    if (!ActiveUnit)
    {
        return false;
    }

    if (PendingSkillData->TargetingType == ESkillTargetingType::Unit)
    {
        AUnitBase* TargetUnit = Tile->GetOccupyingUnit();

        if (!TargetUnit)
        {
            return false;
        }

        if (!TargetUnit->IsUnitAlive())
        {
            return false;
        }

        switch (PendingSkillData->TargetTeamRule)
        {
        case ESkillTargetTeamRule::EnemyOnly:
        {
            if (TargetUnit->GetTeam() == ActiveUnit->GetTeam())
            {
                return false;
            }

            break;
        }
        case ESkillTargetTeamRule::AllyOnly:
        {
            if (TargetUnit->GetTeam() != ActiveUnit->GetTeam())
            {
                return false;
            }

            break;
        }
        case ESkillTargetTeamRule::AnyUnit:
        {
            break;
        }
        case ESkillTargetTeamRule::EmptyTileOnly:
        {
            return false;
        }
        default:
        {
            return false;
        }
        }

        if (!PendingSkillData->bIgnoreFront && Tile->GetProtectedByFront())
        {
            return false;
        }

        return true;
    }

    if (PendingSkillData->TargetingType == ESkillTargetingType::Tile)
    {
        switch (PendingSkillData->TargetTeamRule)
        {
        case ESkillTargetTeamRule::EmptyTileOnly:
        {
            return Tile->GetOccupyingUnit() == nullptr;
        }
        case ESkillTargetTeamRule::EnemyOnly:
        case ESkillTargetTeamRule::AllyOnly:
        case ESkillTargetTeamRule::AnyUnit:
        {
            return false;
        }
        default:
        {
            return false;
        }
        }
    }

    return false;
}