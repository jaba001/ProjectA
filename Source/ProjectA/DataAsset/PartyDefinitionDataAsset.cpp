#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Unit/PlayerUnit.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

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
    FText Error;
    return ResolveProfession(ClassId, OutDefinition, Error);
}

bool UPartyDefinitionDataAsset::ResolveProfession(FName ClassId, FProfessionDefinition& OutDefinition, FText& OutError) const
{
    OutError = FText::GetEmpty();
    const auto Fail = [this, ClassId, &OutError](const FText& Reason)
    {
        OutError = FText::Format(NSLOCTEXT("PartyDefinition", "ProfessionError", "{0} [{1}]: {2}"), FText::FromString(GetPathName()), FText::FromName(ClassId), Reason);
        return false;
    };
    const FProfessionDefinition* Definition = Professions.Find(ClassId);
    if (!Definition)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MissingProfession", "Profession definition is missing. / 직업 정의가 없습니다."));
    }
    OutDefinition = *Definition;
    OutDefinition.CombatClass = ResolvePlayerClass(ClassId);
    if (!OutDefinition.CombatClass)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MissingClass", "Set CombatClass, PlayerUnitClasses or FallbackPlayerUnitClass. / CombatClass, PlayerUnitClasses 또는 FallbackPlayerUnitClass를 지정하세요."));
    }
    if (OutDefinition.bUseUnitClassDefaults)
    {
        const APlayerUnit* Defaults = OutDefinition.CombatClass->GetDefaultObject<APlayerUnit>();
        OutDefinition.MaxHP = Defaults->GetInitialMaxHP();
        OutDefinition.ActionPoints = Defaults->GetMaxActionPoint();
        OutDefinition.SubActionPoints = Defaults->GetMaxSubActionPoint();
        OutDefinition.StartingSkills = Defaults->GetEquippedSkillDataAssets();
    }
    if (!FMath::IsFinite(OutDefinition.MaxHP) || OutDefinition.MaxHP <= 0.0f)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "InvalidHP", "Resolved MaxHP must be finite and positive. / 실제 MaxHP는 유한한 양수여야 합니다."));
    }
    if (OutDefinition.ActionPoints <= 0 || OutDefinition.SubActionPoints < 0)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "InvalidAP", "Resolved ActionPoints must be positive and SubActionPoints nonnegative. / 실제 ActionPoints는 양수, SubActionPoints는 0 이상이어야 합니다."));
    }
    if (OutDefinition.StartingSkills.IsEmpty())
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MissingSkills", "The resolved loadout requires at least one starting skill. / 실제 시작 스킬이 한 개 이상 필요합니다."));
    }
    TSet<FName> SkillIds;
    for (int32 Index = 0; Index < OutDefinition.StartingSkills.Num(); ++Index)
    {
        USkillDefinitionDataAsset* Skill = OutDefinition.StartingSkills[Index];
        if (!IsValid(Skill))
        {
            return Fail(FText::Format(NSLOCTEXT("PartyDefinition", "InvalidSkillReference", "StartingSkills[{0}] is missing or invalid. / StartingSkills[{0}] 참조가 없거나 유효하지 않습니다."), FText::AsNumber(Index)));
        }
        FCombatRoundSkill Resolved;
        FText Error;
        if (!Skill->ResolveRoundSkill(Resolved, Error)) return Fail(Error);
        if (SkillIds.Contains(Resolved.SkillId))
        {
            return Fail(FText::Format(NSLOCTEXT("PartyDefinition", "DuplicateSkill", "Starting skill asset ID is duplicated: {0}. / 시작 스킬 에셋 ID가 중복됩니다: {0}."), FText::FromName(Resolved.SkillId)));
        }
        SkillIds.Add(Resolved.SkillId);
    }
    return true;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UPartyDefinitionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    bool bValid = ParentResult != EDataValidationResult::Invalid;
    if (Professions.IsEmpty())
    {
        Context.AddError(NSLOCTEXT("PartyDefinition", "EmptyProfessions", "At least one profession definition is required. / 직업 정의가 한 개 이상 필요합니다."));
        bValid = false;
    }
    for (const TPair<FName, FProfessionDefinition>& Entry : Professions)
    {
        if (Entry.Key.IsNone())
        {
            Context.AddError(NSLOCTEXT("PartyDefinition", "EmptyProfessionId", "A profession ID cannot be None. / 직업 ID는 None일 수 없습니다."));
            bValid = false;
            continue;
        }
        FProfessionDefinition Definition;
        FText Error;
        if (!ResolveProfession(Entry.Key, Definition, Error))
        {
            Context.AddError(Error);
            bValid = false;
        }
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

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
        FCombatRoundSkill Resolved;
        FText Error;
        if (Skill->ResolveRoundSkill(Resolved, Error)) Skills += FString::Printf(TEXT("\n• %s (AP %d · 보조 AP %d)"), *Skill->SkillName.ToString(), Resolved.ActionPointCost, Resolved.SubActionPointCost);
    }
    return FText::FromString(FString::Printf(TEXT("%s\n%s\n\nHP %.0f · AP %d · 보조 AP %d\n\n시작 스킬%s"), *Definition.DisplayName.ToString(), *Definition.Description.ToString(), Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, *Skills));
}
