#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Unit/PlayerUnit.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunTypes.h"
#include "Profession/ProfessionBase.h"

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
    UnarmedStartingSkill = TSoftObjectPtr<USkillDefinitionDataAsset>(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack.BPDA_DefaulatAttack")));
    for (TSubclassOf<UProfessionBase> ProfessionClass : UProfessionBase::GetPlayableClasses())
    {
        const UProfessionBase* Profession = ProfessionClass->GetDefaultObject<UProfessionBase>();
        FProfessionDefinition& Definition = Professions.Add(Profession->ClassId);
        Definition.ProfessionClass = ProfessionClass;
        Definition.DisplayName = Profession->DisplayName;
        Definition.Description = Profession->Description;
        Definition.MaxHP = Profession->MaxHP;
        Definition.Strength = Profession->Strength;
        Definition.Dexterity = Profession->Dexterity;
        Definition.Intelligence = Profession->Intelligence;
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
    if (!UProfessionBase::FindProfession(ClassId))
    {
        return Fail(NSLOCTEXT("PartyDefinition", "UnsupportedProfession", "Select Warrior, Mage, Archer or Rogue. Start a new game instead of loading previous test professions. / 전사·마법사·궁수·도적 중 선택하세요. 이전 테스트 직업으로 저장했다면 새 게임을 시작해 주세요."));
    }
    const FProfessionDefinition* Definition = Professions.Find(ClassId);
    if (!Definition)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MissingProfession", "Profession definition is missing. / 직업 정의가 없습니다."));
    }
    OutDefinition = *Definition;
    if (!OutDefinition.ProfessionClass || OutDefinition.ProfessionClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MissingProfessionClass", "A concrete ProfessionClass is required. / 생성 가능한 직업 자식 클래스가 필요합니다."));
    }
    const UProfessionBase* Profession = OutDefinition.ProfessionClass->GetDefaultObject<UProfessionBase>();
    if (Profession->ClassId != ClassId)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MismatchedProfessionClass", "ProfessionClass must use the same ClassId as its catalog entry. / 직업 클래스와 목록 항목의 ClassId가 일치해야 합니다."));
    }
    OutDefinition.DisplayName = Profession->DisplayName;
    OutDefinition.Description = Profession->Description;
    OutDefinition.CombatClass = ResolvePlayerClass(ClassId);
    if (!OutDefinition.CombatClass)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "MissingClass", "Set CombatClass, PlayerUnitClasses or FallbackPlayerUnitClass. / CombatClass, PlayerUnitClasses 또는 FallbackPlayerUnitClass를 지정하세요."));
    }
    if (OutDefinition.CombatClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
    {
        return Fail(FText::Format(NSLOCTEXT("PartyDefinition", "UnspawnableClass", "Resolved CombatClass cannot be abstract or deprecated: {0}. / 실제 CombatClass는 추상 또는 사용 중단 클래스일 수 없습니다: {0}."), FText::FromString(OutDefinition.CombatClass->GetPathName())));
    }
    if (OutDefinition.bUseUnitClassDefaults)
    {
        const APlayerUnit* Defaults = OutDefinition.CombatClass->GetDefaultObject<APlayerUnit>();
        OutDefinition.MaxHP = Profession->MaxHP;
        OutDefinition.Strength = Profession->Strength;
        OutDefinition.Dexterity = Profession->Dexterity;
        OutDefinition.Intelligence = Profession->Intelligence;
        OutDefinition.ActionPoints = Defaults->GetMaxActionPoint();
        OutDefinition.SubActionPoints = Defaults->GetMaxSubActionPoint();
        OutDefinition.StartingSkills = Defaults->GetEquippedSkillDataAssets();
    }
    if (!FMath::IsFinite(OutDefinition.MaxHP) || OutDefinition.MaxHP <= 0.0f)
    {
        return Fail(NSLOCTEXT("PartyDefinition", "InvalidHP", "Resolved MaxHP must be finite and positive. / 실제 MaxHP는 유한한 양수여야 합니다."));
    }
    for (float Attribute : {OutDefinition.Strength, OutDefinition.Dexterity, OutDefinition.Intelligence})
    {
        if (!FMath::IsFinite(Attribute) || Attribute < 0.0f || Attribute > 1000000.0f)
        {
            return Fail(NSLOCTEXT("PartyDefinition", "InvalidAttributes", "Strength, Dexterity and Intelligence must be finite values between 0 and 1000000. / 힘·민첩·지능은 0~1000000 범위의 유한한 값이어야 합니다."));
        }
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

bool UPartyDefinitionDataAsset::ResolveStartingSkills(FName ClassId, TArray<TObjectPtr<USkillDefinitionDataAsset>>& OutSkills, FText& OutError) const
{
    OutSkills.Reset();
    OutError = FText::GetEmpty();
    if (!UProfessionBase::FindProfession(ClassId))
    {
        OutError = NSLOCTEXT("PartyDefinition", "UnsupportedStartingProfession", "비무장 시작 스킬을 받을 수 없는 직업입니다.");
        return false;
    }
    USkillDefinitionDataAsset* Skill = UnarmedStartingSkill.LoadSynchronous();
    FCombatRoundSkill Definition;
    if (!IsValid(Skill))
    {
        OutError = NSLOCTEXT("PartyDefinition", "MissingUnarmedSkill", "비무장 시작 스킬 에셋을 불러올 수 없습니다.");
        return false;
    }
    if (!Skill->ResolveRoundSkill(Definition, OutError)) return false;
    OutSkills.Add(Skill);
    return true;
}

bool UPartyDefinitionDataAsset::ResolveMemberSkills(const FRunPartyMember& Member, TArray<TObjectPtr<USkillDefinitionDataAsset>>& OutSkills, FText& OutError) const
{
    OutSkills.Reset();
    FProfessionDefinition Profession;
    if (!ResolveProfession(Member.ClassId, Profession, OutError)) return false;
    if (!Member.bHasSkillLoadout)
    {
        if (!Member.Skills.IsEmpty())
        {
            OutError = NSLOCTEXT("PartyDefinition", "UnversionedMemberSkills", "이전 저장 형식에 명시되지 않은 스킬 장착 데이터가 있습니다.");
            return false;
        }
        OutSkills = MoveTemp(Profession.StartingSkills);
        return true;
    }
    if (Member.Skills.IsEmpty() || Member.Skills.Num() > 5)
    {
        OutError = NSLOCTEXT("PartyDefinition", "InvalidMemberSkillCount", "캐릭터의 저장된 스킬은 1~5개여야 합니다.");
        return false;
    }
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
    TSet<FName> SkillIds;
    for (const FSoftObjectPath& Path : Member.Skills)
    {
        USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
        FCombatRoundSkill Definition;
        if (!IsValid(Skill))
        {
            OutError = FText::Format(NSLOCTEXT("PartyDefinition", "MissingMemberSkill", "저장된 스킬 에셋을 불러올 수 없습니다: {0}"), FText::FromString(Path.ToString()));
            return false;
        }
        if (!Skill->ResolveRoundSkill(Definition, OutError)) return false;
        if (SkillIds.Contains(Definition.SkillId))
        {
            OutError = NSLOCTEXT("PartyDefinition", "DuplicateMemberSkill", "캐릭터의 저장된 스킬 식별자가 중복됩니다.");
            return false;
        }
        SkillIds.Add(Definition.SkillId);
        Skills.Add(Skill);
    }
    OutSkills = MoveTemp(Skills);
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
        else
        {
            TArray<TObjectPtr<USkillDefinitionDataAsset>> StartingSkills;
            if (!ResolveStartingSkills(Entry.Key, StartingSkills, Error))
            {
                Context.AddError(Error);
                bValid = false;
            }
        }
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

FText UPartyDefinitionDataAsset::GetProfessionDetails(FName ClassId) const
{
    FProfessionDefinition Definition;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> StartingSkills;
    FText Error;
    if (!ResolveProfession(ClassId, Definition) || !ResolveStartingSkills(ClassId, StartingSkills, Error))
    {
        return FText::FromString(TEXT("직업 전투 설정을 확인해 주세요."));
    }
    FString Skills;
    for (USkillDefinitionDataAsset* Skill : StartingSkills)
    {
        FCombatRoundSkill Resolved;
        if (Skill->ResolveRoundSkill(Resolved, Error)) Skills += FString::Printf(TEXT("\n• %s (AP %d · 보조 AP %d)"), *Skill->SkillName.ToString(), Resolved.ActionPointCost, Resolved.SubActionPointCost);
    }
    const FString Dexterity = FText::AsNumber(Definition.Dexterity).ToString();
    return FText::FromString(FString::Printf(TEXT("%s\n%s\n\nHP %.0f · 힘 %.0f · 민첩 %s · 지능 %.0f\n속도 %s (민첩 1당 1)\nAP %d · 보조 AP %d\n\n시작 스킬%s"), *Definition.DisplayName.ToString(), *Definition.Description.ToString(), Definition.MaxHP, Definition.Strength, *Dexterity, Definition.Intelligence, *Dexterity, Definition.ActionPoints, Definition.SubActionPoints, *Skills));
}
