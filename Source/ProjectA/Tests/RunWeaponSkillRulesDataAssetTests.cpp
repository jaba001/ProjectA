#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunWeaponSkillRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunWeaponDefaultRuleDataTest, "ProjectA.Run.WeaponSkills.DevelopmentRuleData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunWeaponDefaultRuleDataTest::RunTest(const FString& Parameters)
{
    const URunWeaponSkillRulesDataAsset* Asset = GetDefault<URunWeaponSkillRulesDataAsset>();
    FRunWeaponSkillRulesState State;
    FText Error;
    if (!TestTrue(TEXT("The development rule asset freezes valid source skill data"), Asset->BuildState(State, Error))) return false;
    TestEqual(TEXT("Every generated weapon receives one trial skill"), State.SkillCount, 1);
    TestEqual(TEXT("The development data exposes all five grades"), State.Rarities.Num(), 5);
    for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
    {
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Candidate.Skill.TryLoad());
        FCombatRoundSkill Definition;
        if (!TestTrue(TEXT("Every candidate retains its source GAS execution tags"), Skill && Skill->ResolveRoundSkill(Definition, Error) && Candidate.Tags == Definition.EffectTags)) return false;
    }
    const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon"));
    const FGameplayTag BowTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Bow"));
    const FGameplayTag CrossbowTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Crossbow"));
    for (const TCHAR* Category : {TEXT("Item.Weapon.Sword"), TEXT("Item.Weapon.Bow"), TEXT("Item.Weapon.Crossbow"), TEXT("Item.Weapon.StaffWand"), TEXT("Item.Weapon.Shield"), TEXT("Item.Weapon.ArrowBolt"), TEXT("Item.Weapon.Bullet"), TEXT("Item.Weapon.Other")})
    {
        FRunItemDefinition Base;
        Base.Asset = FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/WeaponRuleBase.WeaponRuleBase"));
        Base.DisplayName = FText::FromString(TEXT("Development item"));
        Base.Tags.AddTag(WeaponTag);
        Base.Tags.AddTag(FGameplayTag::RequestGameplayTag(FName(Category)));
        const bool bWeapon = State.WeaponQuery.Matches(Base.Tags);
        TestEqual(TEXT("Only explicit supported weapon categories receive skills"), bWeapon, FCString::Strcmp(Category, TEXT("Item.Weapon.Sword")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.Bow")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.Crossbow")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.StaffWand")) == 0);
        for (int32 Grade = 0; Grade < State.Rarities.Num(); ++Grade)
        {
            FRunWeaponSkillRulesState SingleGrade = State;
            for (int32 Index = 0; Index < SingleGrade.Rarities.Num(); ++Index) SingleGrade.Rarities[Index].BaseWeight = Index == Grade ? 1.0f : 0.0f;
            TestEqual(TEXT("Every grade begins with equal trial weight"), State.Rarities[Grade].BaseWeight, 1.0f);
            TestFalse(TEXT("A generated grade has an explicit text name"), State.Rarities[Grade].DisplayName.IsEmpty());
            FRandomStream Random(31 + Grade);
            FRunItemDefinition Copy;
            if (!TestTrue(TEXT("Each supported item grade has suitable candidates without unrelated filling"), RunWeaponSkillRules::Generate(Base, SingleGrade, Random, Copy, Error))) return false;
            TestEqual(TEXT("The forced grade is preserved in the generated copy"), Copy.RarityTag, State.Rarities[Grade].RarityTag);
            TestEqual(TEXT("Shields ammunition and other nonweapons receive a grade without skills"), Copy.GrantedSkills.Num(), bWeapon ? 1 : 0);
            TestEqual(TEXT("Grade generation retains the trial 1G price"), Copy.Price, 1);
        }
    }
    const FRunWeaponSkillCandidate* Arrow = State.Candidates.FindByPredicate([](const FRunWeaponSkillCandidate& Candidate) { return Candidate.Skill.GetAssetName() == TEXT("DA_DrGame_ProjectileHitVFX_Arrow"); });
    const FRunWeaponSkillCandidate* Crossbow = State.Candidates.FindByPredicate([](const FRunWeaponSkillCandidate& Candidate) { return Candidate.Skill.GetAssetName() == TEXT("DA_CrossbowAttack"); });
    if (!TestTrue(TEXT("Bow and crossbow basics have separate authored candidates"), Arrow && Crossbow)) return false;
    TestTrue(TEXT("Bow eligibility uses its item query rather than generic ranged attack tags"), Arrow->AllowedItemQuery.Matches(FGameplayTagContainer(BowTag)) && !Arrow->AllowedItemQuery.Matches(FGameplayTagContainer(CrossbowTag)));
    TestTrue(TEXT("Crossbow eligibility uses its own item query"), Crossbow->AllowedItemQuery.Matches(FGameplayTagContainer(CrossbowTag)) && !Crossbow->AllowedItemQuery.Matches(FGameplayTagContainer(BowTag)));
    const FGameplayTagContainer StaffTags(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.StaffWand")));
    TArray<TSet<FSoftObjectPath>> GradePools;
    for (const FRunWeaponRarityRule& Rarity : State.Rarities)
    {
        TSet<FSoftObjectPath>& Pool = GradePools.AddDefaulted_GetRef();
        for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
        {
            FGameplayTagContainer Tags = Candidate.Tags;
            Tags.AppendTags(Candidate.SelectionTags);
            if (Candidate.AllowedItemQuery.Matches(StaffTags) && Rarity.SkillQuery.Matches(Tags)) Pool.Add(Candidate.Skill);
        }
    }
    for (int32 Grade = 1; Grade < GradePools.Num(); ++Grade) TestTrue(TEXT("The magic weapon's colored pools add distinct content beyond the basic white pool"), GradePools[Grade].Num() > GradePools[0].Num());
    return true;
}

#endif
