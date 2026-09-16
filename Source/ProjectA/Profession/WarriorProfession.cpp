#include "Profession/WarriorProfession.h"

UWarriorProfession::UWarriorProfession()
{
    ClassId = TEXT("Warrior");
    DisplayName = NSLOCTEXT("Profession", "WarriorName", "전사");
    Description = NSLOCTEXT("Profession", "WarriorDescription", "전사 계열");
}
