#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunSkillBalance.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillBalanceCsvTest, "ProjectA.Run.SkillBalance.CsvAtomicityAndFrozenSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillBalanceCsvTest::RunTest(const FString& Parameters)
{
    FText Error;
    FRunWeaponSkillRulesState State;
    if (!TestTrue(TEXT("The actual two CSV files build the complete active weapon skill catalog."), GetDefault<URunWeaponSkillRulesDataAsset>()->BuildState(State, Error))) return false;
    TestTrue(TEXT("The new policy stores sixty-two skill balances and twenty-five equipment-to-skill rarity weights."), State.BalanceVersion == 1 && State.Candidates.Num() == 62 && State.SkillRarityWeights.Num() == 25);
    FString BalanceCsv;
    FString ProbabilityCsv;
    if (!FFileHelper::LoadFileToString(BalanceCsv, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/SKILL_BALANCE.csv"))) || !FFileHelper::LoadFileToString(ProbabilityCsv, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/SKILL_RARITY_PROBABILITIES.csv")))) return false;
    const FRunWeaponSkillRulesState Before = State;
    const FString BadTotal = ProbabilityCsv.Replace(TEXT("\"80\""), TEXT("\"81\""));
    TestFalse(TEXT("A probability row that no longer sums to one hundred is rejected."), RunSkillBalance::LoadFromStrings(BalanceCsv, BadTotal, State, Error));
    TestTrue(TEXT("CSV rejection explains the error and leaves every frozen rule unchanged."), !Error.IsEmpty() && FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&State, &Before, 0));
    const FString BadId = BalanceCsv.Replace(TEXT("SkillDefinitionDataAsset:"), TEXT("UnknownSkillType:"));
    TestFalse(TEXT("Unknown primary asset IDs cannot silently bind to an existing object path."), RunSkillBalance::LoadFromStrings(BadId, ProbabilityCsv, State, Error));
    TestTrue(TEXT("An invalid skill row also preserves the complete old state."), !Error.IsEmpty() && FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&State, &Before, 0));
    FRunWeaponSkillRulesState Invalid = State;
    Invalid.SkillRarityWeights[1] = Invalid.SkillRarityWeights[0];
    TestFalse(TEXT("Duplicated rarity pairs cannot replace a missing pair."), RunWeaponSkillRules::Validate(Invalid, Error));
    Invalid = State;
    Invalid.Candidates[0].Balance.Power = -1.f;
    TestFalse(TEXT("A malformed frozen power is rejected before generation or saving."), RunWeaponSkillRules::Validate(Invalid, Error));
    Invalid = State;
    Invalid.BalanceVersion = 0;
    TestFalse(TEXT("Version zero cannot hide new tuning or probability data."), RunWeaponSkillRules::Validate(Invalid, Error));
    Invalid = State;
    const FGameplayTag OtherGrade = RunSkillBalance::ResolveRarityTag(Invalid.Candidates[0].Balance.RarityTag == RunSkillBalance::ResolveRarityTag(TEXT("흰색")) ? TEXT("파란색") : TEXT("흰색"));
    Invalid.Candidates[0].SelectionTags.AddTag(OtherGrade);
    TestFalse(TEXT("One skill cannot impersonate two rarity groups through extra selection tags."), RunWeaponSkillRules::Validate(Invalid, Error));
    FRunWeaponSkillRulesState Custom = State;
    const FGameplayTagQuery AuthoredQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Skill.Effect.Heal")));
    Custom.Rarities[0].SkillQuery = AuthoredQuery;
    TestTrue(TEXT("CSV tuning preserves independently authored GAS candidate conditions."), RunSkillBalance::LoadFromStrings(BalanceCsv, ProbabilityCsv, Custom, Error) && Custom.Rarities[0].SkillQuery == AuthoredQuery);

    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->WeaponSkillRules = State;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Frozen tuning and weights serialize using Unreal SaveGame."), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The frozen skill policy restores."), Restored.Get())) return false;
    TestTrue(TEXT("A restored policy preserves all values and validates without reloading CSV."), FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&State, &Restored->WeaponSkillRules, 0) && RunWeaponSkillRules::Validate(Restored->WeaponSkillRules, Error));
    TStrongObjectPtr<URunWeaponSkillRulesDataAsset> Legacy(NewObject<URunWeaponSkillRulesDataAsset>());
    Legacy->bUseCsvBalance = false;
    FRunWeaponSkillRulesState LegacyState;
    TestTrue(TEXT("Explicit legacy rules retain their original pools and asset values."), Legacy->BuildState(LegacyState, Error) && LegacyState.BalanceVersion == 0 && LegacyState.SkillRarityWeights.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillBalanceWeightedGenerationTest, "ProjectA.Run.SkillBalance.EquipmentWeightsAndFrozenCopies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillBalanceWeightedGenerationTest::RunTest(const FString& Parameters)
{
    FText Error;
    FRunWeaponSkillRulesState State;
    if (!GetDefault<URunWeaponSkillRulesDataAsset>()->BuildState(State, Error)) return false;
    FRunItemDefinition Base;
    Base.Asset = FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/SkillBalanceWeapon.SkillBalanceWeapon"));
    Base.DisplayName = FText::FromString(TEXT("Skill balance weapon"));
    Base.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.StaffWand")));
    FRandomStream Random(73117);
    const TCHAR* Names[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
    const float ExpectedMeans[] = {1.22f, 1.72f, 2.45f, 3.27f, 3.99f};
    FRunItemDefinition Copy;
    for (int32 Grade = 0; Grade < 5; ++Grade)
    {
        Base.CatalogRarityTag = RunItemShopCatalog::ResolveRarityTag(Names[Grade]);
        float Total = 0.f;
        for (int32 Sample = 0; Sample < 512; ++Sample)
        {
            if (!RunWeaponSkillRules::Generate(Base, State, Random, Copy, Error)) return false;
            if (!TestTrue(TEXT("Generated stock freezes a single skill grade and its displayed numeric values."), Copy.RarityTag == Base.CatalogRarityTag && Copy.SkillBalanceVersion == 1 && Copy.GrantedSkills.Num() == 1 && Copy.GrantedSkillBalances.Num() == 1 && RunWeaponSkillRules::ValidateGeneratedCopy(Copy, State, Error))) return false;
            for (int32 SkillGrade = 0; SkillGrade < 5; ++SkillGrade) if (Copy.GrantedSkillBalances[0].RarityTag == RunSkillBalance::ResolveRarityTag(Names[SkillGrade])) Total += static_cast<float>(SkillGrade + 1);
        }
        // A fixed seed and broad tolerance detect catalog-count bias without a flaky statistical threshold.
        // 고정 시드와 넓은 허용차로 불안정한 통계 경계 없이 후보 수 편향을 검출합니다.
        TestTrue(TEXT("The mean selected skill grade follows equipment weights rather than the number of skills per grade."), FMath::Abs(Total / 512.f - ExpectedMeans[Grade]) < 0.18f);
    }
    const FRunItemDefinition Original = Copy;
    Copy.GrantedSkillBalances[0].Power += 1.f;
    TestFalse(TEXT("Changing only displayed skill power cannot pass frozen-copy validation."), RunWeaponSkillRules::ValidateGeneratedCopy(Copy, State, Error));
    Copy = Original;
    Copy.GrantedSkillBalances.Reset();
    TestFalse(TEXT("Omitting the per-copy balance payload is rejected."), RunWeaponSkillRules::ValidateGeneratedCopy(Copy, State, Error));
    Base.Tags.Reset();
    Base.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Crossbow")));
    if (!TestTrue(TEXT("Even orange equipment can renormalize to the sole compatible white crossbow skill."), RunWeaponSkillRules::Generate(Base, State, Random, Copy, Error) && Copy.GrantedSkillBalances[0].RarityTag == RunSkillBalance::ResolveRarityTag(TEXT("흰색")))) return false;
    const FRunItemDefinition BeforeFailure = Copy;
    const int32 BeforeSeed = Random.GetCurrentSeed();
    FRunWeaponSkillRulesState Insufficient = State;
    Insufficient.SkillCount = 2;
    TestFalse(TEXT("An item cannot fill two skill slots by duplicating its only eligible skill."), RunWeaponSkillRules::Generate(Base, Insufficient, Random, Copy, Error));
    TestTrue(TEXT("Failed generation preserves the caller's copy and random stream."), Random.GetCurrentSeed() == BeforeSeed && RunItemShopCatalog::IsSameDefinition(Copy, BeforeFailure));
    Base.Tags.Reset();
    Base.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Sword")));
    if (!TestTrue(TEXT("Multiple requested skill slots use distinct compatible skills."), RunWeaponSkillRules::Generate(Base, Insufficient, Random, Copy, Error) && Copy.GrantedSkills.Num() == 2 && Copy.GrantedSkills[0] != Copy.GrantedSkills[1])) return false;
    return true;
}

#endif
