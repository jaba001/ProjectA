#include "Profession/MageProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UMageProfession::UMageProfession()
{
    ClassId = TEXT("Mage");
    DisplayName = NSLOCTEXT("Profession", "MageName", "마법사");
    Description = NSLOCTEXT("Profession", "MageDescription", "마법사 계열");
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/MageStaff_FreeWeapons/SM_Staff_01.SM_Staff_01")), URunEquipmentCatalog::GetWeaponSlot(0) });
}
