#include "Profession/RogueProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

URogueProfession::URogueProfession()
{
    ClassId = TEXT("Rogue");
    DisplayName = NSLOCTEXT("Profession", "RogueName", "도적");
    Description = NSLOCTEXT("Profession", "RogueDescription", "도적 계열");
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Dagger_1.SM_Dagger_1")), URunEquipmentCatalog::GetWeaponSlot(0) });
}
