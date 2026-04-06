#include "Combat/Library/CombatTargetingLibrary.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"

TArray<AUnitBase*> UCombatTargetingLibrary::CollectUniqueAliveUnitsFromTiles(
    const TArray<ACombatGridTile*>& TargetTiles,
    AUnitBase* SourceUnit)
{
    TArray<AUnitBase*> ResultUnits;
    TSet<AUnitBase*> UniqueUnits;

    for (ACombatGridTile* TargetTile : TargetTiles)
    {
        if (!TargetTile)
        {
            continue;
        }

        AUnitBase* OccupyingUnit = TargetTile->GetOccupyingUnit();

        if (!OccupyingUnit)
        {
            continue;
        }

        if (OccupyingUnit == SourceUnit)
        {
            continue;
        }

        if (!OccupyingUnit->IsUnitAlive())
        {
            continue;
        }

        if (UniqueUnits.Contains(OccupyingUnit))
        {
            continue;
        }

        UniqueUnits.Add(OccupyingUnit);
        ResultUnits.Add(OccupyingUnit);
    }

    return ResultUnits;
}