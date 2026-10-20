#include "Profession/MageProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UMageProfession::UMageProfession()
{
    ClassId = TEXT("Mage");
    DisplayName = NSLOCTEXT("Profession", "MageName", "마법사");
    Description = NSLOCTEXT("Profession", "MageDescription", "마법사 계열");
    // Trial values: Intelligence main stat 10; Speed is the agility stat.
    // 시험값: 주 능력치 지능 10이며 속도는 민첩 능력치입니다.
    Strength = 2.0f;
    Speed = 8.0f;
    Intelligence = 10.0f;
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/MageStaff_FreeWeapons/SM_Staff_01.SM_Staff_01")), URunEquipmentCatalog::GetWeaponSlot(0) });
}
