#include "Profession/ArcherProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UArcherProfession::UArcherProfession()
{
    ClassId = TEXT("Archer");
    DisplayName = NSLOCTEXT("Profession", "ArcherName", "궁수");
    Description = NSLOCTEXT("Profession", "ArcherDescription", "궁수 계열");
    // Trial values: balanced Strength and Intelligence with agility (Speed) 7.
    // 시험값: 힘·지능을 고르게 두고 민첩(속도) 7입니다.
    Strength = 5.0f;
    Speed = 7.0f;
    Intelligence = 4.0f;
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_02_Bow_001.SM_PP_Theme_02_Bow_001")), URunEquipmentCatalog::GetWeaponSlot(1) });
}
