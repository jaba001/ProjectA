#include "Profession/WarriorProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UWarriorProfession::UWarriorProfession()
{
    ClassId = TEXT("Warrior");
    DisplayName = NSLOCTEXT("Profession", "WarriorName", "전사");
    Description = NSLOCTEXT("Profession", "WarriorDescription", "전사 계열");
    // Trial values: Strength main stat 10; Speed is the agility stat.
    // 시험값: 주 능력치 힘 10이며 속도는 민첩 능력치입니다.
    Strength = 10.0f;
    Speed = 5.0f;
    Intelligence = 1.0f;
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_11_Sword_One-Handed_003.SM_PP_Theme_11_Sword_One-Handed_003")), URunEquipmentCatalog::GetWeaponSlot(0) });
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Shield.SM_Shield")), URunEquipmentCatalog::GetWeaponSlot(1) });
}
