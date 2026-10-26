#include "Profession/RogueProfession.h"
#include "Game/Run/RunEquipmentCatalog.h"

URogueProfession::URogueProfession()
{
    ClassId = TEXT("Rogue");
    DisplayName = NSLOCTEXT("Profession", "RogueName", "도적");
    Description = NSLOCTEXT("Profession", "RogueDescription", "도적 계열");
    // Trial values: agility (Speed) main stat 10 with secondary Strength for daggers.
    // 시험값: 주 능력치 민첩(속도) 10이며 단검용 보조 힘을 가집니다.
    Strength = 5.0f;
    Speed = 10.0f;
    Intelligence = 1.0f;
    // Equip separate owned copies of the original dagger in both hands.
    // 원본 단검의 서로 다른 보유 사본을 양손에 각각 장착합니다.
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Dagger_1.SM_Dagger_1")), URunEquipmentCatalog::GetWeaponSlot(0) });
    StartingEquipment.Add({ FSoftObjectPath(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Dagger_1.SM_Dagger_1")), URunEquipmentCatalog::GetWeaponSlot(1) });
}
