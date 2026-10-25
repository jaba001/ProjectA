#include "Game/Run/EquipmentCombatRules.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentTypes.h"

UEquipmentCombatRules::UEquipmentCombatRules()
{
    ConditionalSkills.Add(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_DoubleAttack.DA_DoubleAttack")));
    BasicWeaponAttacks.Add(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_MeleeAttack.DA_MeleeAttack")));
    BasicWeaponAttacks.Add(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack")));
}

bool EquipmentCombatRules::Matches(const TArray<FRunEquipmentVisual>& Equipment, const TArray<FCombatEquipmentRequirement>& Requirements)
{
    for (const FCombatEquipmentRequirement& Requirement : Requirements)
    {
        if (!Requirement.Slot.IsValid() || Requirement.ItemQuery.IsEmpty()) return false;
        const FRunEquipmentVisual* Item = Equipment.FindByPredicate([&Requirement](const FRunEquipmentVisual& Visual) { return Visual.SlotTag == Requirement.Slot; });
        if (!Item || !Requirement.ItemQuery.Matches(Item->ItemTags)) return false;
    }
    return true;
}

bool EquipmentCombatRules::ResolveDamage(const TArray<FRunEquipmentVisual>& Equipment, FGameplayTag Slot, float& OutDamage)
{
    OutDamage = 0.f;
    const FGameplayTag Weapon = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon"));
    const FGameplayTag Shield = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield"));
    int32 Count = 0;
    for (const FRunEquipmentVisual& Item : Equipment)
    {
        if (Item.SlotTag != URunEquipmentCatalog::GetWeaponSlot(0) && Item.SlotTag != URunEquipmentCatalog::GetWeaponSlot(1)) continue;
        if ((Slot.IsValid() && Item.SlotTag != Slot)
            || !Item.ItemTags.HasTag(Weapon)
            || Item.ItemTags.HasTag(Shield))
        {
            continue;
        }
        if (!FMath::IsFinite(Item.WeaponDamage)
            || Item.WeaponDamage < 0.f
            || Item.WeaponDamage > 1000000.f)
        {
            return false;
        }
        OutDamage += Item.WeaponDamage;
        ++Count;
    }
    if (Count == 0) return false;
    OutDamage /= Count;
    return true;
}
