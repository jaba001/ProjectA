#include "UI/Combat/CombatPlanningRefreshState.h"

bool FCombatPlanningUnitObservation::operator==(const FCombatPlanningUnitObservation& Other) const
{
    return Unit == Other.Unit && AbilitySystem == Other.AbilitySystem && CurrentTile == Other.CurrentTile && Tags == Other.Tags && BlockedAbilityTags == Other.BlockedAbilityTags && Name == Other.Name && AP == Other.AP && SAP == Other.SAP && MoveRange == Other.MoveRange && bAlive == Other.bAlive;
}

bool FCombatPlanningTileObservation::operator==(const FCombatPlanningTileObservation& Other) const
{
    return Coord == Other.Coord && TileCoord == Other.TileCoord && Tile == Other.Tile && Occupant == Other.Occupant && Territory == Other.Territory && bProtected == Other.bProtected && bValid == Other.bValid;
}

bool FCombatPlanningRefreshState::operator==(const FCombatPlanningRefreshState& Other) const
{
    if (Controller != Other.Controller || Coordinator != Other.Coordinator || Arena != Other.Arena || Grid != Other.Grid || OwnerSlot != Other.OwnerSlot || SelectedUnit != Other.SelectedUnit || SelectedTarget != Other.SelectedTarget || TargetCoord != Other.TargetCoord) return false;
    if (bActivated != Other.bActivated || bInputEnabled != Other.bInputEnabled || bRequestPending != Other.bRequestPending || bSAPMovement != Other.bSAPMovement || bChoosingMove != Other.bChoosingMove || bHasTarget != Other.bHasTarget || RequestStatus != Other.RequestStatus || LocalStatus != Other.LocalStatus) return false;
    if (Units != Other.Units || Tiles != Other.Tiles || Skills.Num() != Other.Skills.Num() || !FCombatRoundView::StaticStruct()->CompareScriptStruct(&View, &Other.View, 0)) return false;
    for (int32 Index = 0; Index < Skills.Num(); ++Index)
    {
        if (!FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Skills[Index], &Other.Skills[Index], 0)) return false;
    }
    return true;
}
