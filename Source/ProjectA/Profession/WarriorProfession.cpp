#include "Profession/WarriorProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UWarriorProfession::UWarriorProfession()
{
    ClassId = TEXT("Warrior");
    DisplayName = NSLOCTEXT("Profession", "WarriorName", "전사");
    Description = NSLOCTEXT("Profession", "WarriorDescription", "전사 계열");
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_11_Sword_One-Handed_003.SM_PP_Theme_11_Sword_One-Handed_003")), URunEquipmentCatalog::GetWeaponSlot(0) });
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Shield.SM_Shield")), URunEquipmentCatalog::GetWeaponSlot(1) });
}
