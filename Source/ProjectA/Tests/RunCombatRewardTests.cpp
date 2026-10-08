#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunCombatRewards.h"
#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "GAS/CombatGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const TCHAR* RewardGrades[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
    const int32 RewardWeights[] = {5000, 3000, 1500, 400, 100};

    struct FCombatRewardFixture
    {
        TStrongObjectPtr<UPackage> Package;
        TStrongObjectPtr<USkillDefinitionDataAsset> Skill;
        FRunItemShopState Shop;
        FRunWeaponSkillRulesState Rules;

        FCombatRewardFixture()
        {
            Package.Reset(CreatePackage(*(TEXT("/Game/User_JeHoon/Validation/T12/CombatRewardSkills_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            Package->SetFlags(RF_Transient);
            Skill.Reset(NewObject<USkillDefinitionDataAsset>(Package.Get(), TEXT("DaggerSkill"), RF_Transient));
            Skill->bUseRoundDefinition = true;
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Element_Physical);
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Shape_Slash);
            Skill->RoundDefinition.Kind = ECombatRoundSkillKind::Melee;
            Skill->RoundDefinition.Approach = ECombatRoundApproach::None;
            Rules.SchemaVersion = 1;
            Rules.SkillCount = 1;
            Rules.WeaponQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger")));
            FRunWeaponSkillCandidate& Candidate = Rules.Candidates.AddDefaulted_GetRef();
            Candidate.Skill = FSoftObjectPath(Skill.Get());
            Candidate.Tags = Skill->RoundDefinition.EffectTags;
            Candidate.AllowedItemQuery = Rules.WeaponQuery;
            Candidate.BaseWeight = 1.0f;
            Shop.SchemaVersion = 1;
            Shop.RarityProbabilities.SchemaVersion = 1;
            for (int32 Grade = 0; Grade < UE_ARRAY_COUNT(RewardGrades); ++Grade)
            {
                const FGameplayTag Tag = RunItemShopCatalog::ResolveRarityTag(RewardGrades[Grade]);
                FRunWeaponRarityRule& Rarity = Rules.Rarities.AddDefaulted_GetRef();
                Rarity.RarityTag = Tag;
                Rarity.DisplayName = FText::FromString(RewardGrades[Grade]);
                Rarity.BaseWeight = 1.0f;
                FRunItemRarityProbability& Probability = Shop.RarityProbabilities.Entries.AddDefaulted_GetRef();
                Probability.RarityTag = Tag;
                Probability.ProbabilityBasisPoints = RewardWeights[Grade];
                for (int32 Index = 0; Index < 3; ++Index) Shop.Catalog.Add(Item(Shop.Catalog.Num(), Grade));
            }
        }

        FRunItemDefinition Item(int32 Index, int32 Grade, FName Category = TEXT("Item.Weapon.Dagger")) const
        {
            FRunItemDefinition Result;
            Result.Asset = FSoftObjectPath(FString::Printf(TEXT("/Game/User_JeHoon/Validation/T12/CombatRewardItem_%d.CombatRewardItem_%d"), Index, Index));
            Result.DisplayName = FText::FromString(FString::Printf(TEXT("Reward item %d"), Index));
            Result.Tags.AddTag(RunItemShopCatalog::GetWeaponTag());
            Result.Tags.AddTag(FGameplayTag::RequestGameplayTag(Category));
            Result.CatalogRarityTag = RunItemShopCatalog::ResolveRarityTag(RewardGrades[Grade]);
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCombatRewardBuildTest, "ProjectA.Run.Reward.Items.CommonFrozenPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunCombatRewardBuildTest::RunTest(const FString& Parameters)
{
    FCombatRewardFixture Fixture;
    FText Error;
    const TArray<int32> GoldRange{12, 5, 9};
    const FGameplayTagQuery FullQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag());
    for (int32 PolicyVersion = 0; PolicyVersion <= 1; ++PolicyVersion)
    {
        FRunItemShopState Shop = Fixture.Shop;
        if (PolicyVersion == 0)
        {
            Shop.RarityProbabilities = FRunItemRarityProbabilityState();
            for (FRunItemDefinition& Item : Shop.Catalog) Item.CatalogRarityTag = FGameplayTag();
        }
        const FRunItemShopState OriginalShop = Shop;
        FRandomStream ExpectedRandom(9173);
        FRandomStream ActualRandom = ExpectedRandom;
        TArray<int32> Selected;
        if (!RunItemRarityProbabilities::Select(Shop.Catalog, Shop.RarityProbabilities, FullQuery, 3, false, ExpectedRandom, Selected, Error, &Fixture.Rules, true)) return false;
        TArray<FRunItemDefinition> ExpectedItems;
        for (const int32 Index : Selected)
        {
            FRunItemDefinition Generated;
            if (!RunWeaponSkillRules::Generate(Shop.Catalog[Index], Fixture.Rules, ExpectedRandom, Generated, Error)) return false;
            ExpectedItems.Add(MoveTemp(Generated));
        }
        const int32 ExpectedGold = ExpectedRandom.RandRange(5, 12);
        FRunGoldRewardState Reward;
        if (!TestTrue(TEXT("Combat rewards reuse the saved uniform or weighted item policy"), RunCombatRewards::Build(TEXT("Combat_01"), Shop, Fixture.Rules, GoldRange, ActualRandom, Reward, Error))) return false;
        TestTrue(TEXT("The result has three item choices and one independent gold award"), Reward.SchemaVersion == 2 && Reward.NodeId == TEXT("Combat_01") && Reward.GoldChoices.IsEmpty() && Reward.Claims.IsEmpty() && Reward.ItemChoices.Num() == 3 && Reward.BonusGold == ExpectedGold);
        TestEqual(TEXT("The reward consumes exactly the common item, skill and gold random draws"), ActualRandom.GetCurrentSeed(), ExpectedRandom.GetCurrentSeed());
        TSet<FSoftObjectPath> Assets;
        TSet<FGuid> Ids;
        for (int32 Index = 0; Index < Reward.ItemChoices.Num(); ++Index)
        {
            const FRunItemDefinition& Item = Reward.ItemChoices[Index];
            const FRunItemDefinition& Expected = ExpectedItems[Index];
            TestTrue(TEXT("Each card preserves the common selection's asset grade and granted skill"), Item.Asset == Expected.Asset && Item.RarityTag == Expected.RarityTag && Item.GrantedSkills == Expected.GrantedSkills);
            TestTrue(TEXT("Every generated card has a distinct asset and valid independent identity"), Item.GenerationVersion == 1 && Item.ItemInstanceId.IsValid() && !Assets.Contains(Item.Asset) && !Ids.Contains(Item.ItemInstanceId));
            Assets.Add(Item.Asset);
            Ids.Add(Item.ItemInstanceId);
        }
        TestTrue(TEXT("Generating rewards never modifies the frozen shop or current display"), FRunItemShopState::StaticStruct()->CompareScriptStruct(&Shop, &OriginalShop, 0));
    }
    FRunItemShopState Mixed = Fixture.Shop;
    Mixed.Catalog = {Fixture.Item(0, 0), Fixture.Item(1, 0, TEXT("Item.Weapon.Shield")), Fixture.Item(2, 0, TEXT("Item.Weapon.Bow"))};
    Mixed.SelectionVersion = 1;
    Mixed.ActiveEncounterId = TEXT("UnrelatedOrangeShop");
    Mixed.ActiveItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::ResolveRarityTag(TEXT("주황색")));
    Mixed.ActiveStockPolicyVersion = 1;
    FRandomStream Random(107);
    FRunGoldRewardState MixedReward;
    if (!TestTrue(TEXT("Combat rewards include supported shields and bows and ignore an unrelated shop filter"), RunCombatRewards::Build(TEXT("Combat_01"), Mixed, Fixture.Rules, {7, 7, 7}, Random, MixedReward, Error) && MixedReward.ItemChoices.Num() == 3 && MixedReward.BonusGold == 7)) return false;
    for (const FRunItemDefinition& Item : MixedReward.ItemChoices) TestEqual(TEXT("Only items matching the frozen skill weapon query receive skills"), Item.GrantedSkills.Num(), Fixture.Rules.WeaponQuery.Matches(Item.Tags) ? 1 : 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCombatRewardAtomicityTest, "ProjectA.Run.Reward.Items.InvalidInputsAndAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunCombatRewardAtomicityTest::RunTest(const FString& Parameters)
{
    FCombatRewardFixture Fixture;
    FRunGoldRewardState Previous;
    Previous.SchemaVersion = 1;
    Previous.NodeId = TEXT("ExistingReward");
    Previous.GoldChoices = {5, 7, 10};
    const auto ExpectFailure = [this, &Previous](FName NodeId, const FRunItemShopState& Shop, const FRunWeaponSkillRulesState& Rules, const TArray<int32>& GoldRange)
    {
        FText Error;
        FRunGoldRewardState Output = Previous;
        FRandomStream Random(319);
        const int32 OriginalSeed = Random.GetCurrentSeed();
        TestFalse(TEXT("Invalid or insufficient reward inputs are rejected"), RunCombatRewards::Build(NodeId, Shop, Rules, GoldRange, Random, Output, Error));
        TestFalse(TEXT("Rejected reward generation provides an explanation"), Error.IsEmpty());
        TestTrue(TEXT("Failure preserves all prior output fields"), FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Previous, &Output, 0));
        TestEqual(TEXT("Failure preserves the caller random stream"), Random.GetCurrentSeed(), OriginalSeed);
    };
    ExpectFailure(NAME_None, Fixture.Shop, Fixture.Rules, {5, 7, 10});
    ExpectFailure(TEXT("Combat_01"), Fixture.Shop, Fixture.Rules, {});
    ExpectFailure(TEXT("Combat_01"), Fixture.Shop, Fixture.Rules, {0, 7, 10});
    ExpectFailure(TEXT("Combat_01"), Fixture.Shop, Fixture.Rules, {5, 7, 1001});
    FRunItemShopState Invalid = Fixture.Shop;
    Invalid.SchemaVersion = 0;
    ExpectFailure(TEXT("Combat_01"), Invalid, Fixture.Rules, {5, 7, 10});
    Invalid = Fixture.Shop;
    Invalid.Catalog.SetNum(2);
    ExpectFailure(TEXT("Combat_01"), Invalid, Fixture.Rules, {5, 7, 10});
    Invalid = Fixture.Shop;
    Invalid.Catalog[1].Asset = Invalid.Catalog[0].Asset;
    ExpectFailure(TEXT("Combat_01"), Invalid, Fixture.Rules, {5, 7, 10});
    Invalid = Fixture.Shop;
    Invalid.RarityProbabilities.Entries[0].ProbabilityBasisPoints = -1;
    ExpectFailure(TEXT("Combat_01"), Invalid, Fixture.Rules, {5, 7, 10});
    FRunWeaponSkillRulesState Insufficient = Fixture.Rules;
    Insufficient.SkillCount = 2;
    ExpectFailure(TEXT("Combat_01"), Fixture.Shop, Insufficient, {5, 7, 10});
    ExpectFailure(TEXT("Combat_01"), Fixture.Shop, FRunWeaponSkillRulesState(), {5, 7, 10});
    FText Error;
    FRandomStream Random(431);
    FRunGoldRewardState Reward;
    if (!RunCombatRewards::Build(TEXT("Combat_01"), Fixture.Shop, Fixture.Rules, {5, 7, 10}, Random, Reward, Error)) return false;
    for (int32 Mutation = 0; Mutation < 11; ++Mutation)
    {
        FRunGoldRewardState Altered = Reward;
        switch (Mutation)
        {
        case 0: Altered.SchemaVersion = 1; break;
        case 1: Altered.NodeId = NAME_None; break;
        case 2: Altered.GoldChoices.Add(5); break;
        case 3: Altered.ItemChoices.Pop(); break;
        case 4: Altered.BonusGold = 4; break;
        case 5: Altered.BonusGold = 11; break;
        case 6: Altered.ItemChoices[1] = Altered.ItemChoices[0]; break;
        case 7: Altered.ItemChoices[1].ItemInstanceId = Altered.ItemChoices[0].ItemInstanceId; break;
        case 8: Altered.ItemChoices[0].DisplayName = FText::FromString(TEXT("Changed reward")); break;
        case 9: Altered.ItemChoices[0].GrantedSkills.Reset(); break;
        case 10: Altered.ItemChoices[0].Asset = FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/Unknown.Unknown")); break;
        }
        TestFalse(TEXT("Malformed saved choices cannot pass frozen reward validation"), RunCombatRewards::Validate(Altered, Fixture.Shop, Fixture.Rules, {5, 7, 10}, Error));
    }
    FRunItemShopState ZeroProbability = Fixture.Shop;
    const FGameplayTag BlockedGrade = Reward.ItemChoices[0].CatalogRarityTag;
    const int32 BlockedIndex = ZeroProbability.RarityProbabilities.Entries.IndexOfByPredicate([BlockedGrade](const FRunItemRarityProbability& Entry) { return Entry.RarityTag == BlockedGrade; });
    const int32 ReceivingIndex = (BlockedIndex + 1) % ZeroProbability.RarityProbabilities.Entries.Num();
    ZeroProbability.RarityProbabilities.Entries[ReceivingIndex].ProbabilityBasisPoints += ZeroProbability.RarityProbabilities.Entries[BlockedIndex].ProbabilityBasisPoints;
    ZeroProbability.RarityProbabilities.Entries[BlockedIndex].ProbabilityBasisPoints = 0;
    TestFalse(TEXT("Saved rewards cannot contain a grade excluded by their frozen probabilities"), RunCombatRewards::Validate(Reward, ZeroProbability, Fixture.Rules, {5, 7, 10}, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCombatRewardSaveTest, "ProjectA.Run.Reward.Items.FrozenSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunCombatRewardSaveTest::RunTest(const FString& Parameters)
{
    FCombatRewardFixture Fixture;
    FText Error;
    FRandomStream Random(809);
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->ItemShopState = Fixture.Shop;
    Save->WeaponSkillRules = Fixture.Rules;
    if (!RunCombatRewards::Build(TEXT("Combat_01"), Save->ItemShopState, Save->WeaponSkillRules, {5, 7, 10}, Random, Save->GoldRewardState, Error)) return false;
    FRunGoldRewardClaim& Claim = Save->GoldRewardState.Claims.AddDefaulted_GetRef();
    Claim.CharacterId = FGuid::NewGuid();
    Claim.ChoiceIndex = 1;
    FRunPartyMember& Member = Save->Party.AddDefaulted_GetRef();
    Member.CharacterId = Claim.CharacterId;
    Member.Items.Add(Save->GoldRewardState.ItemChoices[Claim.ChoiceIndex]);
    Member.Gold = Save->GoldRewardState.BonusGold;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Frozen reward cards gold and claim serialize with their catalog and rules"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The item reward SaveGame can be restored"), Restored.Get())) return false;
    if (!TestTrue(TEXT("Restored item rewards validate without rebuilding or consulting source CSV"), RunCombatRewards::Validate(Restored->GoldRewardState, Restored->ItemShopState, Restored->WeaponSkillRules, {5, 7, 10}, Error))) return false;
    TestTrue(TEXT("Version node and random gold survive exactly"), Restored->GoldRewardState.SchemaVersion == 2 && Restored->GoldRewardState.NodeId == Save->GoldRewardState.NodeId && Restored->GoldRewardState.BonusGold == Save->GoldRewardState.BonusGold);
    for (int32 Index = 0; Index < Save->GoldRewardState.ItemChoices.Num(); ++Index) TestTrue(TEXT("Every displayed card preserves its generated ID grade skill and name"), RunItemShopCatalog::IsSameDefinition(Save->GoldRewardState.ItemChoices[Index], Restored->GoldRewardState.ItemChoices[Index]));
    TestTrue(TEXT("The selected exact copy and independent gold survive together"), Restored->GoldRewardState.Claims.Num() == 1 && Restored->GoldRewardState.Claims[0].ChoiceIndex == 1 && Restored->GoldRewardState.Claims[0].CharacterId == Claim.CharacterId && Restored->Party.Num() == 1 && Restored->Party[0].Items.Num() == 1 && RunItemShopCatalog::IsSameDefinition(Restored->GoldRewardState.ItemChoices[1], Restored->Party[0].Items[0]) && Restored->Party[0].Gold == Restored->GoldRewardState.BonusGold);
    Save->GoldRewardState = FRunGoldRewardState();
    Save->GoldRewardState.SchemaVersion = 1;
    Save->GoldRewardState.GoldChoices = {5, 7, 10};
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes)) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Historical gold-only reward values remain serializable"), Legacy.Get())) return false;
    TestTrue(TEXT("New item and bonus fields remain empty in a legacy reward"), Legacy->GoldRewardState.SchemaVersion == 1 && Legacy->GoldRewardState.GoldChoices == TArray<int32>{5, 7, 10} && Legacy->GoldRewardState.ItemChoices.IsEmpty() && Legacy->GoldRewardState.BonusGold == 0);
    return true;
}

#endif
