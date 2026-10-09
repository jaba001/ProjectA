#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "GAS/CombatGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunWeaponDefaultRuleDataTest, "ProjectA.Run.WeaponSkills.DevelopmentRuleData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunWeaponDefaultRuleDataTest::RunTest(const FString& Parameters)
{
    const URunWeaponSkillRulesDataAsset* Asset = GetDefault<URunWeaponSkillRulesDataAsset>();
    FRunWeaponSkillRulesState State;
    FText Error;
    if (!TestTrue(TEXT("The development rule asset freezes valid source skill data"), Asset->BuildState(State, Error))) return false;
    TestEqual(TEXT("Every generated weapon receives one frozen skill"), State.SkillCount, 1);
    TestTrue(TEXT("New Runs freeze the CSV tuning and all twenty-five rarity weights"), State.BalanceVersion == 1 && State.SkillRarityWeights.Num() == 25 && State.Candidates.Num() == 62);
    TestEqual(TEXT("The development data exposes all five grades"), State.Rarities.Num(), 5);
    TArray<FRunItemDefinition> Catalog;
    if (!TestTrue(TEXT("The source item catalog supplies authored grades"), RunItemShopCatalog::Load(Catalog, Error))) return false;
    for (const FRunItemDefinition& Item : Catalog) TestTrue(TEXT("Every source item has a fixed grade supported by its unchanged weapon skill rules"), Item.CatalogRarityTag.IsValid() && RunWeaponSkillRules::CanGenerate(Item, State));
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
        TestEqual(TEXT("Only explicit supported weapon and shield categories receive skills"), bWeapon, FCString::Strcmp(Category, TEXT("Item.Weapon.Sword")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.Bow")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.Crossbow")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.StaffWand")) == 0 || FCString::Strcmp(Category, TEXT("Item.Weapon.Shield")) == 0);
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
            TestEqual(TEXT("Supported weapons and shields receive one skill while ammunition and other nonweapons receive none"), Copy.GrantedSkills.Num(), bWeapon ? 1 : 0);
            TestEqual(TEXT("Grade generation retains the trial 1G price"), Copy.Price, 1);
            FRunItemDefinition FixedBase = Base;
            FixedBase.CatalogRarityTag = State.Rarities[Grade].RarityTag;
            if (!TestTrue(TEXT("Authored grades use the matching weapon pool without changing default rule weights"), RunWeaponSkillRules::Generate(FixedBase, State, Random, Copy, Error))) return false;
            TestTrue(TEXT("Each fixed grade retains its metadata and existing weapon skill count"), Copy.RarityTag == FixedBase.CatalogRarityTag && Copy.CatalogRarityTag == FixedBase.CatalogRarityTag && Copy.GrantedSkills.Num() == (bWeapon ? 1 : 0));
        }
    }
    const FRunWeaponSkillCandidate* Arrow = State.Candidates.FindByPredicate([](const FRunWeaponSkillCandidate& Candidate) { return Candidate.Skill.GetAssetName() == TEXT("DA_DrGame_ProjectileHitVFX_Arrow"); });
    const FRunWeaponSkillCandidate* Crossbow = State.Candidates.FindByPredicate([](const FRunWeaponSkillCandidate& Candidate) { return Candidate.Skill.GetAssetName() == TEXT("DA_CrossbowAttack"); });
    if (!TestTrue(TEXT("Bow and crossbow basics have separate authored candidates"), Arrow && Crossbow)) return false;
    TestTrue(TEXT("Bow eligibility uses its item query rather than generic ranged attack tags"), Arrow->AllowedItemQuery.Matches(FGameplayTagContainer(BowTag)) && !Arrow->AllowedItemQuery.Matches(FGameplayTagContainer(CrossbowTag)));
    TestTrue(TEXT("Crossbow eligibility uses its own item query"), Crossbow->AllowedItemQuery.Matches(FGameplayTagContainer(CrossbowTag)) && !Crossbow->AllowedItemQuery.Matches(FGameplayTagContainer(BowTag)));
    TSet<FGameplayTag> SkillGrades;
    for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
    {
        SkillGrades.Add(Candidate.Balance.RarityTag);
        TestTrue(TEXT("Every candidate has one explicit grade and numeric tuning independent of equipment grade"), Candidate.Balance.RarityTag.IsValid() && Candidate.SelectionTags.HasTagExact(Candidate.Balance.RarityTag) && Candidate.Balance.Power > 0.f);
    }
    TestEqual(TEXT("The active authored skill catalog covers five independent skill grades"), SkillGrades.Num(), 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunWeaponGuardianEligibilityTest, "ProjectA.Run.WeaponSkills.GuardianShieldEligibilityAndFrozenQueries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunWeaponGuardianEligibilityTest::RunTest(const FString& Parameters)
{
    FRunWeaponSkillRulesState State;
    FText Error;
    if (!TestTrue(TEXT("New Runs freeze the authored guardian eligibility rules."), GetDefault<URunWeaponSkillRulesDataAsset>()->BuildState(State, Error))) return false;
    const FGameplayTag ShieldTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield"));
    const FGameplayTag StaffTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.StaffWand"));
    const FGameplayTag BookTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Spellbook"));
    const TCHAR* Categories[] = {TEXT("Sword"), TEXT("Dagger"), TEXT("Axe"), TEXT("Hammer"), TEXT("MaceClub"), TEXT("Spear"), TEXT("Scythe"), TEXT("Gauntlet"), TEXT("Bow"), TEXT("Crossbow"), TEXT("StaffWand"), TEXT("Spellbook"), TEXT("Shield"), TEXT("Firearm"), TEXT("Thrown"), TEXT("Explosive"), TEXT("ArrowBolt"), TEXT("Bullet"), TEXT("Other")};
    int32 Guardians = 0;
    int32 Healers = 0;
    for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
    {
        Guardians += Candidate.Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield) ? 1 : 0;
        Healers += Candidate.Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal) ? 1 : 0;
    }
    TestEqual(TEXT("All seven source shield-effect skills are retained."), Guardians, 7);
    TestEqual(TEXT("All six source heal-effect skills remain available."), Healers, 6);
    FGameplayTagContainer LegacyWeaponTags;
    FRunItemDefinition SavedShield;
    for (const TCHAR* Category : Categories)
    {
        const FGameplayTag CategoryTag = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Item.Weapon.%s"), Category)));
        FRunItemDefinition Base;
        Base.Asset = FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/GuardianRuleBase.GuardianRuleBase"));
        Base.DisplayName = FText::FromString(TEXT("Guardian eligibility fixture"));
        Base.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon")));
        Base.Tags.AddTag(CategoryTag);
        const bool bShield = CategoryTag == ShieldTag;
        const bool bMagic = CategoryTag == StaffTag || CategoryTag == BookTag;
        const bool bWeapon = State.WeaponQuery.Matches(Base.Tags);
        if (bShield) TestTrue(TEXT("Shields participate in the same frozen one-skill generation pipeline."), bWeapon);
        else if (bWeapon) LegacyWeaponTags.AddTag(CategoryTag);
        for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
        {
            const bool bGuardian = Candidate.Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield);
            const bool bEligible = Candidate.AllowedItemQuery.Matches(Base.Tags);
            if (bGuardian || bShield) TestEqual(TEXT("Every guardian accepts only shields and shields accept only guardians across all registered categories."), bEligible, bGuardian && bShield);
            if (Candidate.Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal)) TestEqual(TEXT("Healing retains staff and spellbook eligibility without leaking into shields."), bEligible, bMagic);
        }
        for (const FRunWeaponRarityRule& Rarity : State.Rarities)
        {
            Base.CatalogRarityTag = Rarity.RarityTag;
            for (int32 Seed : {17, 103, 2026})
            {
                FRandomStream Random(Seed);
                FRunItemDefinition Copy;
                if (!TestTrue(TEXT("Every supported category and equipment grade can generate and validate its frozen copy."), RunWeaponSkillRules::CanGenerate(Base, State) && RunWeaponSkillRules::Generate(Base, State, Random, Copy, Error) && RunWeaponSkillRules::ValidateGeneratedCopy(Copy, State, Error))) return false;
                TestTrue(TEXT("Generation preserves authored equipment rarity and the expected skill count."), Copy.RarityTag == Rarity.RarityTag && Copy.GrantedSkills.Num() == (bWeapon ? 1 : 0));
                for (const FSoftObjectPath& Skill : Copy.GrantedSkills)
                {
                    const FRunWeaponSkillCandidate* Candidate = State.Candidates.FindByPredicate([&Skill](const FRunWeaponSkillCandidate& Entry) { return Entry.Skill == Skill; });
                    if (!TestTrue(TEXT("Generated skills satisfy the same saved item query used by eligibility."), Candidate && Candidate->AllowedItemQuery.Matches(Copy.Tags))) return false;
                    TestEqual(TEXT("Seeded generation grants guardian effects exactly to shields."), Candidate->Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield), bShield);
                }
                if (bShield) SavedShield = Copy;
            }
        }
    }
    if (!TestTrue(TEXT("The authored shield creates a persisted guardian copy."), SavedShield.GrantedSkills.Num() == 1)) return false;
    TStrongObjectPtr<URunWeaponSkillRulesDataAsset> Authored(NewObject<URunWeaponSkillRulesDataAsset>());
    Authored->Rules.WeaponQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(LegacyWeaponTags);
    const FRunWeaponSkillRulesState BeforeAuthored = Authored->Rules;
    FRunWeaponSkillRulesState Refreshed;
    if (!TestTrue(TEXT("A serialized authoring asset with the old weapon query gains shield eligibility when building a new Run."), Authored->BuildState(Refreshed, Error) && Refreshed.WeaponQuery.Matches(FGameplayTagContainer(ShieldTag)) && RunWeaponSkillRules::ValidateGeneratedCopy(SavedShield, Refreshed, Error))) return false;
    TestTrue(TEXT("New-run rule construction preserves the original authoring asset values."), FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Authored->Rules, &BeforeAuthored, 0));
    const UPartyDefinitionDataAsset* Party = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestTrue(TEXT("The actual starting party references its saved weapon-rule asset."), Party && Party->WeaponSkillRules)) return false;
    FRunWeaponSkillRulesState PartyRules;
    TestTrue(TEXT("The actual party-linked asset builds shield-compatible new-run rules despite serialized historical fields."), Party->WeaponSkillRules->BuildState(PartyRules, Error) && PartyRules.WeaponQuery.Matches(FGameplayTagContainer(ShieldTag)) && RunWeaponSkillRules::ValidateGeneratedCopy(SavedShield, PartyRules, Error));
    for (bool bEmptySkillQuery : {false, true})
    {
        TStrongObjectPtr<URunWeaponSkillRulesDataAsset> Invalid(NewObject<URunWeaponSkillRulesDataAsset>());
        if (!TestFalse(TEXT("The authoring fixture contains its default tag override."), Invalid->ItemQueryOverrides.IsEmpty())) return false;
        if (bEmptySkillQuery) Invalid->ItemQueryOverrides[0].SkillQuery = FGameplayTagQuery();
        else Invalid->ItemQueryOverrides[0].AllowedItemQuery = FGameplayTagQuery();
        FRunWeaponSkillRulesState Preserved = State;
        TestFalse(TEXT("An empty override condition or item query cannot publish partial new-run rules."), Invalid->BuildState(Preserved, Error));
        TestTrue(TEXT("Rejected authoring leaves the complete previous output unchanged and reports a reason."), !Error.IsEmpty() && FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Preserved, &State, 0));
    }
    for (bool bLegacy : {false, true})
    {
        TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
        Save->WeaponSkillRules = State;
        FRunItemDefinition SavedCopy = SavedShield;
        if (bLegacy)
        {
            // Reconstruct a historical magic-weapon guardian policy only in the disposable in-memory fixture.
            // 메모리 검증용 fixture에만 기존 마법 무기 수호 정책을 구성합니다.
            Save->WeaponSkillRules.WeaponQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(LegacyWeaponTags);
            FGameplayTagContainer MagicTags(StaffTag);
            MagicTags.AddTag(BookTag);
            for (FRunWeaponSkillCandidate& Candidate : Save->WeaponSkillRules.Candidates)
            {
                if (Candidate.Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield)) Candidate.AllowedItemQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MagicTags);
            }
            SavedCopy.Tags.RemoveTag(ShieldTag);
            SavedCopy.Tags.AddTag(StaffTag);
        }
        Save->Party.AddDefaulted_GetRef().Items.Add(SavedCopy);
        if (!TestTrue(TEXT("Each fixture is valid under its own frozen guardian eligibility policy."), RunWeaponSkillRules::Validate(Save->WeaponSkillRules, Error) && RunWeaponSkillRules::ValidateGeneratedCopy(SavedCopy, Save->WeaponSkillRules, Error))) return false;
        TArray<uint8> Bytes;
        if (!TestTrue(TEXT("Native SaveGame serializes the full item queries and existing generated result."), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
        TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
        if (!TestTrue(TEXT("Native SaveGame restores the complete frozen eligibility policy and copy."), Restored.IsValid() && Restored->Party.Num() == 1 && Restored->Party[0].Items.Num() == 1 && FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Restored->WeaponSkillRules, &Save->WeaponSkillRules, 0) && RunItemShopCatalog::IsSameDefinition(Restored->Party[0].Items[0], SavedCopy))) return false;
        TestTrue(TEXT("The restored policy validates its saved copy without adopting current creation rules."), RunWeaponSkillRules::Validate(Restored->WeaponSkillRules, Error) && RunWeaponSkillRules::ValidateGeneratedCopy(Restored->Party[0].Items[0], Restored->WeaponSkillRules, Error));
        TestEqual(TEXT("Only a historical staff guardian differs from new-run shield-only validation."), RunWeaponSkillRules::ValidateGeneratedCopy(Restored->Party[0].Items[0], State, Error), !bLegacy);
        TestEqual(TEXT("Saved weapon eligibility retains its original shield participation."), Restored->WeaponSkillRules.WeaponQuery.Matches(FGameplayTagContainer(ShieldTag)), !bLegacy);
    }
    return true;
}

#endif
