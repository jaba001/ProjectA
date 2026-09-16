#include "Profession/ProfessionBase.h"
#include "Profession/WarriorProfession.h"
#include "Profession/MageProfession.h"
#include "Profession/ArcherProfession.h"
#include "Profession/RogueProfession.h"

UProfessionBase::UProfessionBase() : MaxHP(100.0f), Strength(10.0f), Dexterity(10.0f), Intelligence(10.0f)
{
}

TArray<TSubclassOf<UProfessionBase>> UProfessionBase::GetPlayableClasses()
{
    return { UWarriorProfession::StaticClass(), UMageProfession::StaticClass(), UArcherProfession::StaticClass(), URogueProfession::StaticClass() };
}

const UProfessionBase* UProfessionBase::FindProfession(FName InClassId)
{
    for (const TSubclassOf<UProfessionBase>& ProfessionClass : GetPlayableClasses())
    {
        const UProfessionBase* Profession = ProfessionClass.GetDefaultObject();
        if (Profession && Profession->ClassId == InClassId) return Profession;
    }
    return nullptr;
}

TArray<FName> UProfessionBase::GetPlayableIds()
{
    const TArray<TSubclassOf<UProfessionBase>> Classes = GetPlayableClasses();
    TArray<FName> Ids;
    Ids.Reserve(Classes.Num());
    for (const TSubclassOf<UProfessionBase>& ProfessionClass : Classes)
    {
        if (const UProfessionBase* Profession = ProfessionClass.GetDefaultObject()) Ids.Add(Profession->ClassId);
    }
    return Ids;
}
