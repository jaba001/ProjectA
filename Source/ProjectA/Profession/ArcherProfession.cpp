#include "Profession/ArcherProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

UArcherProfession::UArcherProfession()
{
    ClassId = TEXT("Archer");
    DisplayName = NSLOCTEXT("Profession", "ArcherName", "궁수");
    Description = NSLOCTEXT("Profession", "ArcherDescription", "궁수 계열");
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_02_Bow_001.SM_PP_Theme_02_Bow_001")), URunEquipmentCatalog::GetWeaponSlot(1) });
}
