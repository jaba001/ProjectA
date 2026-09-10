#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Unit/PlayerUnit.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/Library/CombatTargetingLibrary.h"

TSubclassOf<APlayerUnit> UPartyDefinitionDataAsset::ResolvePlayerClass(FName ClassId) const
{
    const FProfessionDefinition* Profession = Professions.Find(ClassId);
    if (Profession && Profession->CombatClass)
    {
        return Profession->CombatClass;
    }
    const TSubclassOf<APlayerUnit>* Found = PlayerUnitClasses.Find(ClassId);
    if (Found && *Found)
    {
        return *Found;
    }

    return FallbackPlayerUnitClass;
}

UPartyDefinitionDataAsset::UPartyDefinitionDataAsset()
{
    const TArray<FName> Ids = { TEXT("StableHand"), TEXT("Scholar"), TEXT("Herbalist"), TEXT("Hunter") };
    const TArray<FString> Names = { TEXT("마구간지기"), TEXT("학자"), TEXT("약초상"), TEXT("사냥꾼") };
    const TArray<FString> Descriptions = { TEXT("동물을 돌보며 여정을 준비해 온 마구간지기."), TEXT("지식과 관찰로 미지의 세계를 탐구하는 학자."), TEXT("풀과 약초의 쓰임을 익혀 온 약초상."), TEXT("야생에서 흔적을 읽고 먹잇감을 추적하는 사냥꾼.") };
    for (int32 Index = 0; Index < Ids.Num(); ++Index)
    {
        FProfessionDefinition& Definition = Professions.Add(Ids[Index]);
        Definition.DisplayName = FText::FromString(Names[Index]);
        Definition.Description = FText::FromString(Descriptions[Index]);
    }
}

bool UPartyDefinitionDataAsset::ResolveProfession(FName ClassId, FProfessionDefinition& OutDefinition) const
{
    const FProfessionDefinition* Definition = Professions.Find(ClassId);
    if (!Definition)
    {
        return false;
    }
    OutDefinition = *Definition;
    OutDefinition.CombatClass = ResolvePlayerClass(ClassId);
    if (!OutDefinition.CombatClass)
    {
        return false;
    }
    if (OutDefinition.bUseUnitClassDefaults)
    {
        const APlayerUnit* Defaults = OutDefinition.CombatClass->GetDefaultObject<APlayerUnit>();
        OutDefinition.MaxHP = Defaults->GetInitialMaxHP();
        OutDefinition.ActionPoints = Defaults->GetMaxActionPoint();
        OutDefinition.SubActionPoints = Defaults->GetMaxSubActionPoint();
        OutDefinition.StartingSkills.Reset();
        for (TSubclassOf<UGameplayAbility> Ability : Defaults->GetAvailableSkillAbilityClasses())
        {
            if (USkillDefinitionDataAsset* Skill = Defaults->FindSkillDataByAbilityClass(Ability))
            {
                OutDefinition.StartingSkills.AddUnique(Skill);
            }
        }
    }
    if (!FMath::IsFinite(OutDefinition.MaxHP) || OutDefinition.MaxHP <= 0.0f || OutDefinition.ActionPoints <= 0 || OutDefinition.SubActionPoints < 0 || OutDefinition.StartingSkills.IsEmpty())
    {
        return false;
    }
    TSet<UClass*> SkillClasses;
    for (USkillDefinitionDataAsset* Skill : OutDefinition.StartingSkills)
    {
        if (!UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->AbilityClass || Skill->ActionPointCost <= 0 || SkillClasses.Contains(Skill->AbilityClass))
        {
            return false;
        }
        SkillClasses.Add(Skill->AbilityClass);
    }
    return true;
}

FText UPartyDefinitionDataAsset::GetProfessionDetails(FName ClassId) const
{
    FProfessionDefinition Definition;
    if (!ResolveProfession(ClassId, Definition))
    {
        return FText::FromString(TEXT("직업 전투 설정을 확인해 주세요."));
    }
    FString Skills;
    for (USkillDefinitionDataAsset* Skill : Definition.StartingSkills)
    {
        Skills += FString::Printf(TEXT("\n• %s (AP %d)"), *Skill->SkillName.ToString(), Skill->ActionPointCost);
    }
    return FText::FromString(FString::Printf(TEXT("%s\n%s\n\nHP %.0f · AP %d · 보조 AP %d\n\n시작 스킬%s"), *Definition.DisplayName.ToString(), *Definition.Description.ToString(), Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, *Skills));
}
