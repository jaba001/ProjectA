#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Unit/PlayerUnit.h"

TSubclassOf<APlayerUnit> UPartyDefinitionDataAsset::ResolvePlayerClass(FName ClassId) const
{
    const TSubclassOf<APlayerUnit>* Found = PlayerUnitClasses.Find(ClassId);
    if (Found && *Found)
    {
        return *Found;
    }

    return FallbackPlayerUnitClass;
}
