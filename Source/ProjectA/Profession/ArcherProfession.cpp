#include "Profession/ArcherProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UArcherProfession::UArcherProfession()
{
    ClassId = TEXT("Archer");
    DisplayName = NSLOCTEXT("Profession", "ArcherName", "궁수");
    Description = NSLOCTEXT("Profession", "ArcherDescription", "궁수 계열");
    // Trial values: agility (Speed) main stat 10.
    // 시험값: 주 능력치 민첩(속도) 10입니다.
    Strength = 4.0f;
    Speed = 10.0f;
    Intelligence = 2.0f;
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_02_Bow_001.SM_PP_Theme_02_Bow_001")), URunEquipmentCatalog::GetWeaponSlot(1) });
}
