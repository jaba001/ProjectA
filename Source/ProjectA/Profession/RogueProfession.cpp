#include "Profession/RogueProfession.h"

URogueProfession::URogueProfession()
{
    ClassId = TEXT("Rogue");
    DisplayName = NSLOCTEXT("Profession", "RogueName", "도적");
    Description = NSLOCTEXT("Profession", "RogueDescription", "도적 계열");
}
