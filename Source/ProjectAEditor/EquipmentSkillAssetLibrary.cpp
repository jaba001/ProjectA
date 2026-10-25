#include "EquipmentSkillAssetLibrary.h"
#include "Animation/AnimMontage.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/EquipmentCombatRules.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunTypes.h"
#include "Game/Run/RunEquipmentTypes.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "GAS/CombatGameplayTags.h"

bool UEquipmentSkillAssetLibrary::ConfigureDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, UAnimMontage* Montage, float Windup, float StrikeInterval)
{
    if (!IsValid(Skill)
        || !IsValid(BasicAttack)
        || Skill == BasicAttack
        || !IsValid(Montage))
    {
        return false;
    }
    FCombatRoundSkill Profile;
    FText Error;
    if (!BasicAttack->ResolveRoundSkill(Profile, Error)) return false;
    Profile.CastMontage = Montage;
    Profile.WindupSeconds = Windup;
    Profile.Power = 10.f;
    Profile.bUseEquippedWeaponDamage = true;
    Profile.bUseWeaponTrace = false;
    Profile.bUseEffectCollision = false;
    Profile.bUseMeleeAreaCollision = false;
    Profile.MeleeArea = ESkillAreaType::Single;
    Profile.EffectTags.AddTag(ProjectACombatTags::Skill_Attack_Melee);
    Profile.RequiredEquipment.Reset();
    Profile.MeleeStrikes.Reset();
    const FGameplayTag Dagger = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger"));
    for (int32 SlotIndex : {1, 0})
    {
        FCombatEquipmentRequirement& Requirement = Profile.RequiredEquipment.AddDefaulted_GetRef();
        Requirement.Slot = URunEquipmentCatalog::GetWeaponSlot(SlotIndex);
        Requirement.ItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(Dagger);
        FCombatMeleeStrike& Strike = Profile.MeleeStrikes.AddDefaulted_GetRef();
        Strike.WeaponSlot = Requirement.Slot;
        Strike.DelaySeconds = SlotIndex == 1 ? 0.f : StrikeInterval;
    }
    if (!CombatRoundRules::IsValidSkill(Profile)) return false;
    Skill->Modify();
    Skill->bUseRoundDefinition = true;
    Skill->RoundDefinition = Profile;
    Skill->AbilityClass = BasicAttack->AbilityClass;
    Skill->ActionPointCost = Profile.ActionPointCost;
    Skill->SkillIcon = BasicAttack->SkillIcon;
    Skill->SkillId = TEXT("DoubleAttack");
    Skill->SkillName = FText::FromString(TEXT("더블어택"));
    Skill->SkillDescription = FText::FromString(TEXT("양손에 단검을 장착해야 합니다. 왼손, 오른손 순서로 각각의 무기 피해량으로 한 번씩 공격합니다. AP 비용은 기본 근접 공격과 같습니다."));
    Skill->PostEditChange();
    Skill->MarkPackageDirty();
    return true;
}

bool UEquipmentSkillAssetLibrary::ValidateDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, FString& OutReport)
{
    OutReport = TEXT("Double attack asset contract failed / 더블어택 에셋 계약 불일치");
    FCombatRoundSkill Profile;
    FCombatRoundSkill Base;
    FText Error;
    if (!IsValid(Skill)
        || !IsValid(BasicAttack)
        || !Skill->ResolveRoundSkill(Profile, Error)
        || !BasicAttack->ResolveRoundSkill(Base, Error))
    {
        return false;
    }
    if (!Profile.bUseEquippedWeaponDamage
        || Profile.MeleeStrikes.Num() != 2
        || Profile.RequiredEquipment.Num() != 2
        || Profile.ActionPointCost != Base.ActionPointCost)
    {
        return false;
    }
    const FGameplayTag Main = URunEquipmentCatalog::GetWeaponSlot(0);
    const FGameplayTag Off = URunEquipmentCatalog::GetWeaponSlot(1);
    if (Profile.MeleeStrikes[0].WeaponSlot != Off || Profile.MeleeStrikes[1].WeaponSlot != Main) return false;
    TArray<FRunEquipmentVisual> Equipment;
    FRunEquipmentVisual Right;
    Right.SlotTag = Main;
    Right.ItemTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger")));
    Right.WeaponDamage = 20.f;
    FRunEquipmentVisual Left = Right;
    Left.SlotTag = Off;
    Left.WeaponDamage = 10.f;
    Equipment = {Right, Left};
    float Damage = 0.f;
    if (!EquipmentCombatRules::Matches(Equipment, Profile.RequiredEquipment)
        || !EquipmentCombatRules::ResolveDamage(Equipment, FGameplayTag(), Damage)
        || Damage != 15.f)
    {
        return false;
    }
    if (!EquipmentCombatRules::ResolveDamage(Equipment, Off, Damage)
        || Damage != 10.f
        || !EquipmentCombatRules::ResolveDamage(Equipment, Main, Damage)
        || Damage != 20.f)
    {
        return false;
    }
    Equipment.Pop();
    if (EquipmentCombatRules::Matches(Equipment, Profile.RequiredEquipment)) return false;
    Left.ItemTags.Reset();
    Left.ItemTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield")));
    Equipment.Add(Left);
    if (EquipmentCombatRules::Matches(Equipment, Profile.RequiredEquipment)
        || !EquipmentCombatRules::ResolveDamage(Equipment, FGameplayTag(), Damage)
        || Damage != 20.f)
    {
        return false;
    }
    TArray<FRunItemDefinition> Weapons;
    if (!RunItemShopCatalog::Load(Weapons, Error)) return false;
    for (const FRunItemDefinition& Weapon : Weapons) if (Weapon.WeaponDamage != 10.f) return false;
    FRunPartyMember Member;
    Member.bCreated = true;
    Member.bHasSkillLoadout = true;
    Member.Equipment.bHasLoadout = true;
    Member.Equipment.ConditionalSkillVersion = 1;
    const FRunItemDefinition* Dagger = Weapons.FindByPredicate([](const FRunItemDefinition& Item) { return Item.Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger"))); });
    if (!Dagger) return false;
    Member.Items = {*Dagger, *Dagger};
    Member.InnateSkills = {FSoftObjectPath(BasicAttack)};
    FRunEquipmentSlot MainSlot;
    MainSlot.SlotTag = Main;
    MainSlot.ItemIndex = 0;
    FRunEquipmentSlot OffSlot;
    OffSlot.SlotTag = Off;
    OffSlot.ItemIndex = 1;
    Member.Equipment.Slots = {MainSlot, OffSlot};
    TArray<FSoftObjectPath> Skills;
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || !Skills.Contains(FSoftObjectPath(Skill))) return false;
    Member.Equipment.Slots.Pop();
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || Skills.Contains(FSoftObjectPath(Skill))) return false;
    OutReport = TEXT("Dual daggers required; left/right damage 10/20; mean 15; single dagger and shield rejected; weapon catalog damage 10; AP matches basic attack.");
    return true;
}
