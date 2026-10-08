#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "GAS/CombatGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "Types/GameplayTagCandidateSelection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProbabilityEligible, "Validation.ItemRarity.Eligible");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProbabilityExcluded, "Validation.ItemRarity.Excluded");

    const TCHAR* ColorNames[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
    const TCHAR* ColorTags[] = {TEXT("Item.Rarity.White"), TEXT("Item.Rarity.Green"), TEXT("Item.Rarity.Blue"), TEXT("Item.Rarity.Purple"), TEXT("Item.Rarity.Orange")};
    const int32 DefaultBasisPoints[] = {5000, 3000, 1500, 400, 100};

    FString MakeProbabilityCsv(const TArray<FString>& Percentages)
    {
        FString Csv = TEXT("등급,등급 태그,확률(%),적용 범위,비고\n");
        for (int32 Index = 0; Index < 5; ++Index) Csv += FString::Printf(TEXT("%s,%s,%s,아이템상점,개발 시험값\n"), ColorNames[Index], ColorTags[Index], *Percentages[Index]);
        return Csv;
    }

    FString DefaultProbabilityCsv()
    {
        return MakeProbabilityCsv({TEXT("50"), TEXT("30"), TEXT("15"), TEXT("4"), TEXT("1")});
    }

    FRunItemDefinition MakeProbabilityItem(int32 Index, int32 Grade, FGameplayTag Eligibility = TAG_ProbabilityEligible, FName Category = TEXT("Item.Weapon.Sword"))
    {
        FRunItemDefinition Item;
        Item.Asset = FSoftObjectPath(FString::Printf(TEXT("/Game/User_JeHoon/Validation/T12/ProbabilityItem_%d.ProbabilityItem_%d"), Index, Index));
        Item.DisplayName = FText::FromString(FString::Printf(TEXT("Probability item %d"), Index));
        Item.Tags.AddTag(RunItemShopCatalog::GetWeaponTag());
        Item.Tags.AddTag(FGameplayTag::RequestGameplayTag(Category));
        Item.Tags.AddTag(Eligibility);
        Item.CatalogRarityTag = FGameplayTag::RequestGameplayTag(FName(ColorTags[Grade]));
        return Item;
    }

    TArray<FRunItemDefinition> MakeProbabilityCatalog(const TArray<int32>& Counts)
    {
        TArray<FRunItemDefinition> Catalog;
        for (int32 Grade = 0; Grade < Counts.Num(); ++Grade)
        {
            for (int32 Index = 0; Index < Counts[Grade]; ++Index) Catalog.Add(MakeProbabilityItem(Catalog.Num(), Grade));
        }
        return Catalog;
    }

    bool SameProbabilityState(const FRunItemRarityProbabilityState& Left, const FRunItemRarityProbabilityState& Right)
    {
        return FRunItemRarityProbabilityState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }

    struct FProbabilitySkillFixture
    {
        TStrongObjectPtr<UPackage> Package;
        TStrongObjectPtr<USkillDefinitionDataAsset> Skill;
        FRunWeaponSkillRulesState Rules;

        FProbabilitySkillFixture()
        {
            Package.Reset(CreatePackage(*(TEXT("/Game/User_JeHoon/Validation/T12/ProbabilitySkills_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            Package->SetFlags(RF_Transient);
            Skill.Reset(NewObject<USkillDefinitionDataAsset>(Package.Get(), TEXT("SwordSkill"), RF_Transient));
            Skill->bUseRoundDefinition = true;
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Element_Physical);
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Shape_Slash);
            Skill->RoundDefinition.Kind = ECombatRoundSkillKind::Melee;
            Skill->RoundDefinition.Approach = ECombatRoundApproach::None;
            Rules.SchemaVersion = 1;
            Rules.SkillCount = 1;
            Rules.WeaponQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag());
            FRunWeaponSkillCandidate& Candidate = Rules.Candidates.AddDefaulted_GetRef();
            Candidate.Skill = FSoftObjectPath(Skill.Get());
            Candidate.Tags = Skill->RoundDefinition.EffectTags;
            Candidate.AllowedItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Sword")));
            Candidate.BaseWeight = 1.0f;
            for (int32 Grade = 0; Grade < 5; ++Grade)
            {
                FRunWeaponRarityRule& Rarity = Rules.Rarities.AddDefaulted_GetRef();
                Rarity.RarityTag = FGameplayTag::RequestGameplayTag(FName(ColorTags[Grade]));
                Rarity.DisplayName = FText::FromString(ColorNames[Grade]);
                Rarity.BaseWeight = 1.0f;
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemRarityProbabilityCsvTest, "ProjectA.Run.Shop.RarityProbabilities.CsvValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemRarityProbabilityCsvTest::RunTest(const FString& Parameters)
{
    const FString Csv = DefaultProbabilityCsv();
    FRunItemRarityProbabilityState State;
    FText Error;
    if (!TestTrue(TEXT("The complete five-grade policy loads"), RunItemRarityProbabilities::LoadFromString(Csv, State, Error) && State.SchemaVersion == 1 && State.Entries.Num() == 5)) return false;
    for (int32 Grade = 0; Grade < 5; ++Grade)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(ColorTags[Grade]));
        const FRunItemRarityProbability* Entry = State.Entries.FindByPredicate([Tag](const FRunItemRarityProbability& Candidate) { return Candidate.RarityTag == Tag; });
        if (!TestNotNull(TEXT("Every known grade occurs exactly once"), Entry)) return false;
        TestEqual(TEXT("Percentages are stored exactly in basis points"), Entry->ProbabilityBasisPoints, DefaultBasisPoints[Grade]);
    }
    FRunItemRarityProbabilityState Decimal;
    TestTrue(TEXT("Two decimal places totaling exactly one hundred percent are supported"), RunItemRarityProbabilities::LoadFromString(MakeProbabilityCsv({TEXT("49.99"), TEXT("30.01"), TEXT("15.00"), TEXT("4"), TEXT("1")}), Decimal, Error));
    FRunItemRarityProbabilityState Quoted;
    TestTrue(TEXT("Quoted notes may contain commas"), RunItemRarityProbabilities::LoadFromString(Csv.Replace(TEXT("개발 시험값"), TEXT("\"개발, 시험값\"")), Quoted, Error));
    const FRunItemRarityProbabilityState Before = State;
    TArray<FString> InvalidCsv;
    for (const FString& Value : {FString(TEXT("")), FString(TEXT(" ")), FString(TEXT("-1")), FString(TEXT("+50")), FString(TEXT("NaN")), FString(TEXT("inf")), FString(TEXT("1e2")), FString(TEXT("50x")), FString(TEXT(".5")), FString(TEXT("50.")), FString(TEXT("100.01")), FString(TEXT("50.001"))}) InvalidCsv.Add(MakeProbabilityCsv({Value, TEXT("30"), TEXT("15"), TEXT("4"), TEXT("1")}));
    InvalidCsv.Add(MakeProbabilityCsv({TEXT("50"), TEXT("30"), TEXT("15"), TEXT("4"), TEXT("0")}));
    InvalidCsv.Add(MakeProbabilityCsv({TEXT("50"), TEXT("30"), TEXT("15"), TEXT("4"), TEXT("1.01")}));
    InvalidCsv.Add(MakeProbabilityCsv({TEXT("0"), TEXT("0"), TEXT("0"), TEXT("0"), TEXT("0")}));
    InvalidCsv.Add(Csv.Replace(TEXT("주황색,Item.Rarity.Orange"), TEXT("보라색,Item.Rarity.Purple")));
    InvalidCsv.Add(Csv.Replace(TEXT("주황색,Item.Rarity.Orange,1,아이템상점,개발 시험값\n"), TEXT("")));
    InvalidCsv.Add(Csv + TEXT("주황색,Item.Rarity.Orange,0,아이템상점,개발 시험값\n"));
    InvalidCsv.Add(Csv.Replace(TEXT("주황색"), TEXT("검은색")));
    InvalidCsv.Add(Csv.Replace(TEXT("Item.Rarity.Orange"), TEXT("Item.Rarity.Unregistered")));
    InvalidCsv.Add(Csv.Replace(TEXT("흰색,Item.Rarity.White"), TEXT("흰색,Item.Rarity.Blue")));
    InvalidCsv.Add(Csv.Replace(TEXT("확률(%)"), TEXT("확률")));
    InvalidCsv.Add(Csv.Replace(TEXT("아이템상점"), TEXT("전투보상")));
    InvalidCsv.Add(Csv.Replace(TEXT("개발 시험값"), TEXT(" ")));
    InvalidCsv.Add(Csv.Replace(TEXT("개발 시험값"), TEXT("잘못된\t비고")));
    InvalidCsv.Add(Csv.Replace(TEXT("개발 시험값"), *FString::ChrN(257, TEXT('가'))));
    InvalidCsv.Add(Csv.Replace(TEXT("개발 시험값"), TEXT("개발,시험값")));
    for (int32 Index = 0; Index < InvalidCsv.Num(); ++Index)
    {
        TestFalse(*FString::Printf(TEXT("Malformed probability CSV %d is rejected"), Index), RunItemRarityProbabilities::LoadFromString(InvalidCsv[Index], State, Error));
        TestTrue(TEXT("Rejected policy input preserves every previously loaded field"), SameProbabilityState(State, Before));
    }
    FRunItemRarityProbabilityState InvalidState = State;
    InvalidState.SchemaVersion = 2;
    TestFalse(TEXT("Unknown saved policy versions cannot fall back to uniform selection"), RunItemRarityProbabilities::Validate(InvalidState, Error));
    InvalidState = State;
    InvalidState.Entries[0].ProbabilityBasisPoints = -1;
    TestFalse(TEXT("Saved negative probabilities are rejected"), RunItemRarityProbabilities::Validate(InvalidState, Error));
    InvalidState = State;
    InvalidState.SchemaVersion = 0;
    TestFalse(TEXT("Legacy policies cannot conceal nonempty weighted entries"), RunItemRarityProbabilities::Validate(InvalidState, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemRarityGradeFirstTest, "ProjectA.Run.Shop.RarityProbabilities.GradeFirstSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemRarityGradeFirstTest::RunTest(const FString& Parameters)
{
    FRunItemRarityProbabilityState Policy;
    FText Error;
    if (!TestTrue(TEXT("The reference policy loads"), RunItemRarityProbabilities::LoadFromString(DefaultProbabilityCsv(), Policy, Error))) return false;
    const TArray<FRunItemDefinition> Balanced = MakeProbabilityCatalog({5, 5, 5, 5, 5});
    const TArray<FRunItemDefinition> Uneven = MakeProbabilityCatalog({5, 8, 16, 32, 64});
    for (int32 Trial = 0; Trial < 128; ++Trial)
    {
        FRandomStream BalancedRandom(1777 + Trial * 7919);
        FRandomStream UnevenRandom = BalancedRandom;
        TArray<int32> BalancedChoice;
        TArray<int32> UnevenChoice;
        if (!TestTrue(TEXT("Both catalog shapes can select one eligible asset"), RunItemRarityProbabilities::Select(Balanced, Policy, FGameplayTagQuery(), 1, false, BalancedRandom, BalancedChoice, Error) && RunItemRarityProbabilities::Select(Uneven, Policy, FGameplayTagQuery(), 1, false, UnevenRandom, UnevenChoice, Error))) return false;
        if (!TestTrue(TEXT("One-slot selections return exactly one catalog index"), BalancedChoice.Num() == 1 && UnevenChoice.Num() == 1 && Balanced.IsValidIndex(BalancedChoice[0]) && Uneven.IsValidIndex(UnevenChoice[0]))) return false;
        TestEqual(TEXT("Adding same-grade assets cannot change a fixed-seed grade draw"), Balanced[BalancedChoice[0]].CatalogRarityTag, Uneven[UnevenChoice[0]].CatalogRarityTag);
    }
    // Measure first slots separately because later slots renormalize after a grade is exhausted.
    // 등급 소진 뒤에는 후속 슬롯 확률이 재정규화되므로 첫 슬롯 분포만 별도로 확인합니다.
    constexpr int32 Trials = 4096;
    int32 GradeCounts[5] = {};
    FRandomStream Random(880301);
    for (int32 Trial = 0; Trial < Trials; ++Trial)
    {
        TArray<int32> Choice;
        if (!RunItemRarityProbabilities::Select(Uneven, Policy, FGameplayTagQuery(), 1, false, Random, Choice, Error) || Choice.Num() != 1 || !Uneven.IsValidIndex(Choice[0])) return false;
        for (int32 Grade = 0; Grade < 5; ++Grade) if (Uneven[Choice[0]].CatalogRarityTag == FGameplayTag::RequestGameplayTag(FName(ColorTags[Grade]))) ++GradeCounts[Grade];
    }
    for (int32 Grade = 0; Grade < 5; ++Grade)
    {
        const double Probability = static_cast<double>(DefaultBasisPoints[Grade]) / 10000.0;
        const double Expected = Trials * Probability;
        const double Tolerance = 6.0 * FMath::Sqrt(Trials * Probability * (1.0 - Probability)) + 2.0;
        TestTrue(*FString::Printf(TEXT("Grade %d follows its configured probability within a six-sigma tolerance"), Grade), FMath::Abs(GradeCounts[Grade] - Expected) <= Tolerance);
    }
    FRunItemRarityProbabilityState WhiteOnly;
    if (!RunItemRarityProbabilities::LoadFromString(MakeProbabilityCsv({TEXT("100"), TEXT("0"), TEXT("0"), TEXT("0"), TEXT("0")}), WhiteOnly, Error)) return false;
    const TArray<FRunItemDefinition> WhiteCatalog = MakeProbabilityCatalog({8, 0, 0, 0, 0});
    int32 AssetCounts[8] = {};
    FRandomStream AssetRandom(204201);
    for (int32 Trial = 0; Trial < Trials; ++Trial)
    {
        TArray<int32> Choice;
        if (!RunItemRarityProbabilities::Select(WhiteCatalog, WhiteOnly, FGameplayTagQuery(), 1, false, AssetRandom, Choice, Error) || Choice.Num() != 1 || !WhiteCatalog.IsValidIndex(Choice[0])) return false;
        ++AssetCounts[Choice[0]];
    }
    const double AssetTolerance = 6.0 * FMath::Sqrt(Trials * 0.125 * 0.875) + 2.0;
    for (int32 Count : AssetCounts) TestTrue(TEXT("Eligible assets within the chosen grade remain equally likely"), FMath::Abs(Count - Trials / 8.0) <= AssetTolerance);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemRarityEligibilityTest, "ProjectA.Run.Shop.RarityProbabilities.EligibilityAndAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemRarityEligibilityTest::RunTest(const FString& Parameters)
{
    FRunItemRarityProbabilityState Policy;
    FRunItemRarityProbabilityState WhiteOnly;
    FText Error;
    if (!RunItemRarityProbabilities::LoadFromString(DefaultProbabilityCsv(), Policy, Error) || !RunItemRarityProbabilities::LoadFromString(MakeProbabilityCsv({TEXT("100"), TEXT("0"), TEXT("0"), TEXT("0"), TEXT("0")}), WhiteOnly, Error)) return false;
    FRandomStream Random(6617);
    const TArray<FRunItemDefinition> Exhausting = MakeProbabilityCatalog({1, 4, 0, 0, 0});
    TArray<int32> Selected;
    if (!TestTrue(TEXT("A grade exhausted mid-display is removed while remaining positive grades fill five slots"), RunItemRarityProbabilities::Select(Exhausting, Policy, FGameplayTagQuery(), 5, false, Random, Selected, Error))) return false;
    TSet<FSoftObjectPath> Seen;
    for (int32 Index : Selected)
    {
        if (!TestTrue(TEXT("Selected indices are valid and never repeat an asset"), Exhausting.IsValidIndex(Index) && !Seen.Contains(Exhausting[Index].Asset))) return false;
        Seen.Add(Exhausting[Index].Asset);
    }
    TestEqual(TEXT("All five eligible assets are used exactly once when no alternatives exist"), Seen.Num(), 5);
    TArray<FRunItemDefinition> Queried = MakeProbabilityCatalog({10, 0, 5, 0, 0});
    for (int32 Index = 0; Index < 10; ++Index)
    {
        Queried[Index].Tags.RemoveTag(TAG_ProbabilityEligible);
        Queried[Index].Tags.AddTag(TAG_ProbabilityExcluded);
    }
    const FGameplayTagQuery EligibleQuery = FGameplayTagQuery::MakeQuery_MatchTag(TAG_ProbabilityEligible);
    if (!TestTrue(TEXT("The item query removes unavailable grades before renormalization"), RunItemRarityProbabilities::Select(Queried, Policy, EligibleQuery, 5, false, Random, Selected, Error))) return false;
    for (int32 Index : Selected) TestTrue(TEXT("Every query-filtered result is an eligible blue asset"), Queried.IsValidIndex(Index) && Index >= 10);
    const FGameplayTagQuery BlueQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Blue")));
    if (!TestTrue(TEXT("Authored grade tags participate in the common item query"), RunItemRarityProbabilities::Select(Queried, Policy, BlueQuery, 5, false, Random, Selected, Error))) return false;
    for (int32 Index : Selected) TestTrue(TEXT("A grade query never admits another grade's assets"), Queried.IsValidIndex(Index) && Index >= 10);
    FProbabilitySkillFixture Fixture;
    if (!TestTrue(TEXT("The skill eligibility fixture is valid"), RunWeaponSkillRules::Validate(Fixture.Rules, Error))) return false;
    TArray<FRunItemDefinition> SkillFiltered;
    for (int32 Index = 0; Index < 10; ++Index) SkillFiltered.Add(MakeProbabilityItem(Index, 0, TAG_ProbabilityEligible, TEXT("Item.Weapon.Bow")));
    for (int32 Index = 10; Index < 15; ++Index) SkillFiltered.Add(MakeProbabilityItem(Index, 2));
    if (!TestTrue(TEXT("A fixed grade without compatible weapon skills is excluded before drawing"), RunItemRarityProbabilities::Select(SkillFiltered, Policy, FGameplayTagQuery(), 5, false, Random, Selected, Error, &Fixture.Rules))) return false;
    for (int32 Index : Selected) TestTrue(TEXT("Unsupported bows cannot consume a slot or redirect to unrelated skills"), SkillFiltered.IsValidIndex(Index) && Index >= 10);
    const auto ExpectAtomicFailure = [this, &Error](const TArray<FRunItemDefinition>& Catalog, const FRunItemRarityProbabilityState& Weights, const FGameplayTagQuery& Query, int32 Count)
    {
        FRandomStream FailedRandom(7129);
        const int32 BeforeSeed = FailedRandom.GetCurrentSeed();
        TArray<int32> Output{91, 92};
        TestFalse(TEXT("Insufficient or invalid selection cannot expose a partial display"), RunItemRarityProbabilities::Select(Catalog, Weights, Query, Count, false, FailedRandom, Output, Error));
        TestTrue(TEXT("Failed selection leaves the previous output unchanged"), Output == TArray<int32>{91, 92});
        TestEqual(TEXT("Failed selection leaves the caller's random stream unchanged"), FailedRandom.GetCurrentSeed(), BeforeSeed);
    };
    ExpectAtomicFailure(Exhausting, WhiteOnly, FGameplayTagQuery(), 5);
    ExpectAtomicFailure(Queried, WhiteOnly, EligibleQuery, 5);
    ExpectAtomicFailure(Exhausting, Policy, FGameplayTagQuery::MakeQuery_MatchTag(TAG_ProbabilityExcluded), 1);
    ExpectAtomicFailure(Exhausting, Policy, FGameplayTagQuery(), 0);
    ExpectAtomicFailure(Exhausting, Policy, FGameplayTagQuery(), 6);
    TArray<FRunItemDefinition> InvalidCatalog = Exhausting;
    InvalidCatalog[1].Asset = InvalidCatalog[0].Asset;
    ExpectAtomicFailure(InvalidCatalog, Policy, FGameplayTagQuery(), 1);
    InvalidCatalog = Exhausting;
    InvalidCatalog[0].CatalogRarityTag = FGameplayTag();
    ExpectAtomicFailure(InvalidCatalog, Policy, FGameplayTagQuery(), 1);
    InvalidCatalog[0].CatalogRarityTag = TAG_ProbabilityEligible;
    ExpectAtomicFailure(InvalidCatalog, Policy, FGameplayTagQuery(), 1);
    FRunItemRarityProbabilityState InvalidPolicy = Policy;
    InvalidPolicy.Entries[0].ProbabilityBasisPoints -= 1;
    ExpectAtomicFailure(Exhausting, InvalidPolicy, FGameplayTagQuery(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemRarityFrozenSaveTest, "ProjectA.Run.Shop.RarityProbabilities.FrozenSaveAndLegacyPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemRarityFrozenSaveTest::RunTest(const FString& Parameters)
{
    FText Error;
    const TArray<FRunItemDefinition> Catalog = MakeProbabilityCatalog({5, 8, 16, 32, 64});
    const FRunItemRarityProbabilityState Legacy;
    TArray<FGameplayTagWeightedCandidate> LegacyCandidates;
    for (const FRunItemDefinition& Item : Catalog)
    {
        FGameplayTagWeightedCandidate& Candidate = LegacyCandidates.AddDefaulted_GetRef();
        Candidate.Tags = Item.Tags;
        Candidate.BaseWeight = 1.0f;
    }
    for (int32 Trial = 0; Trial < 32; ++Trial)
    {
        FRandomStream ExpectedRandom(512 + Trial * 3571);
        FRandomStream ActualRandom = ExpectedRandom;
        TArray<int32> Expected;
        TArray<int32> Actual;
        if (!TestTrue(TEXT("Legacy and original uniform selection both succeed"), GameplayTagCandidateSelection::Select(LegacyCandidates, FGameplayTagQuery(), 5, false, ExpectedRandom, Expected) && RunItemRarityProbabilities::Select(Catalog, Legacy, FGameplayTagQuery(), 5, false, ActualRandom, Actual, Error))) return false;
        TestTrue(TEXT("An absent saved probability policy preserves the original per-asset uniform results"), Actual == Expected);
        TestEqual(TEXT("Legacy selection consumes the original number of random draws"), ActualRandom.GetCurrentSeed(), ExpectedRandom.GetCurrentSeed());
    }
    FRunItemShopState Shop;
    Shop.SchemaVersion = 1;
    Shop.Catalog = Catalog;
    if (!RunItemRarityProbabilities::LoadFromString(DefaultProbabilityCsv(), Shop.RarityProbabilities, Error)) return false;
    FProbabilitySkillFixture Fixture;
    if (!TestTrue(TEXT("The real shop roll creates five weighted generated offers"), RunItemShopCatalog::Roll(Shop, false, FGameplayTagQuery(), Error, &Fixture.Rules))) return false;
    Shop.Offers[0].bSold = true;
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->ItemShopState = Shop;
    Save->WeaponSkillRules = Fixture.Rules;
    Save->Party.AddDefaulted_GetRef().Items.Add(Shop.Offers[0].Item);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The frozen policy and displayed stock serialize with the Run"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Weighted Run data can be read back"), Restored.Get())) return false;
    FRunItemRarityProbabilityState ChangedSource;
    if (!RunItemRarityProbabilities::LoadFromString(MakeProbabilityCsv({TEXT("0"), TEXT("0"), TEXT("0"), TEXT("0"), TEXT("100")}), ChangedSource, Error)) return false;
    TestTrue(TEXT("A newly loaded source policy cannot rewrite a saved Run's basis points"), SameProbabilityState(Restored->ItemShopState.RarityProbabilities, Shop.RarityProbabilities) && !SameProbabilityState(Restored->ItemShopState.RarityProbabilities, ChangedSource));
    TestTrue(TEXT("Saved weighted stock validates against its frozen rules"), RunItemShopCatalog::Validate(Restored->ItemShopState, Error, &Restored->WeaponSkillRules));
    if (!TestTrue(TEXT("Stock version revision price and catalog sizes survive together"), Shop.SchemaVersion == Restored->ItemShopState.SchemaVersion && Shop.Revision == Restored->ItemShopState.Revision && Shop.RerollPrice == Restored->ItemShopState.RerollPrice && Shop.Catalog.Num() == Restored->ItemShopState.Catalog.Num() && Shop.Offers.Num() == Restored->ItemShopState.Offers.Num())) return false;
    for (int32 Index = 0; Index < Shop.Catalog.Num(); ++Index) TestTrue(TEXT("Frozen catalog metadata survives without consulting source CSV"), RunItemShopCatalog::IsSameDefinition(Shop.Catalog[Index], Restored->ItemShopState.Catalog[Index]));
    for (int32 Index = 0; Index < Shop.Offers.Num(); ++Index)
    {
        const FRunItemShopOffer& Original = Shop.Offers[Index];
        const FRunItemShopOffer& Loaded = Restored->ItemShopState.Offers[Index];
        TestTrue(TEXT("Each displayed offer retains its identity sold flag and complete generated item"), Original.OfferId == Loaded.OfferId && Original.bSold == Loaded.bSold && RunItemShopCatalog::IsSameDefinition(Original.Item, Loaded.Item));
    }
    TestTrue(TEXT("Purchased item grade identity and skill results remain exactly as displayed"), Restored->Party.Num() == 1 && Restored->Party[0].Items.Num() == 1 && RunItemShopCatalog::IsSameDefinition(Shop.Offers[0].Item, Restored->Party[0].Items[0]));
    FRandomStream OriginalRandom(1673);
    FRandomStream RestoredRandom = OriginalRandom;
    TArray<int32> OriginalNext;
    TArray<int32> RestoredNext;
    if (!TestTrue(TEXT("Restored and original frozen policies can select the next display"), RunItemRarityProbabilities::Select(Shop.Catalog, Shop.RarityProbabilities, FGameplayTagQuery(), 5, false, OriginalRandom, OriginalNext, Error, &Fixture.Rules) && RunItemRarityProbabilities::Select(Restored->ItemShopState.Catalog, Restored->ItemShopState.RarityProbabilities, FGameplayTagQuery(), 5, false, RestoredRandom, RestoredNext, Error, &Restored->WeaponSkillRules))) return false;
    TestTrue(TEXT("Frozen selection is unchanged after a different source CSV is parsed"), OriginalNext == RestoredNext);
    const FRunItemShopState BeforeFailure = Shop;
    TestFalse(TEXT("A shop refresh with no eligible assets is rejected"), RunItemShopCatalog::Roll(Shop, false, FGameplayTagQuery::MakeQuery_MatchTag(TAG_ProbabilityExcluded), Error, &Fixture.Rules));
    TestTrue(TEXT("Failed real shop refresh preserves sold stock revision catalog and frozen policy"), FRunItemShopState::StaticStruct()->CompareScriptStruct(&Shop, &BeforeFailure, 0));
    Save->ItemShopState.RarityProbabilities = FRunItemRarityProbabilityState();
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes)) return false;
    TStrongObjectPtr<URunSaveGame> LegacyRestored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Legacy uniform policy data remains loadable"), LegacyRestored.Get())) return false;
    TestTrue(TEXT("A version-zero saved policy stays empty instead of adopting current CSV probabilities"), LegacyRestored->ItemShopState.RarityProbabilities.SchemaVersion == 0 && LegacyRestored->ItemShopState.RarityProbabilities.Entries.IsEmpty() && RunItemShopCatalog::Validate(LegacyRestored->ItemShopState, Error, &LegacyRestored->WeaponSkillRules));
    return true;
}

#endif
