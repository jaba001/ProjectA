#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunSaveGame.h"
#include "UObject/StrongObjectPtr.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RunMigrationEligible, "ProjectA.Test.RunMigration.Eligible");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RunMigrationExcluded, "ProjectA.Test.RunMigration.Excluded");

namespace
{
    const FSoftObjectPath RemovedPaths[] =
    {
        FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_SweepingStrike.BPDA_SweepingStrike")),
        FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DA_SweepingStrike.DA_SweepingStrike")),
        FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_SweepingStrike.DA_SweepingStrike"))
    };

    FRunSkillShopOffer MakeOffer(int32 Index, FGameplayTag Tag = TAG_RunMigrationEligible, float Weight = 1.0f)
    {
        FRunSkillShopOffer Offer;
        Offer.OfferId = FName(*FString::Printf(TEXT("Fixture_%02d"), Index));
        Offer.Skill = FSoftObjectPath(*FString::Printf(TEXT("/Game/RunMigrationFixture/Skill_%02d.Skill_%02d"), Index, Index));
        Offer.DisplayName = FText::FromString(FString::Printf(TEXT("Migration Skill %d"), Index));
        Offer.Description = FText::FromString(FString::Printf(TEXT("Preserved description %d"), Index));
        Offer.Price = 10 + Index;
        Offer.Tags.AddTag(Tag);
        Offer.BaseWeight = Weight;
        return Offer;
    }

    struct FRunMigrationFixture
    {
        TStrongObjectPtr<URunSaveGame> Save{NewObject<URunSaveGame>()};
        TArray<FRunSkillShopOffer> RetainedOffers;

        FRunMigrationFixture()
        {
            Save->Identity.RunId = FGuid(1, 2, 3, 4);
            Save->Identity.HostEpoch = 3;
            Save->Identity.HostAccountId.Provider = TEXT("MigrationFixture");
            Save->Identity.HostAccountId.Subject = TEXT("OriginalOwner");
            FRunPartyMember& Member = Save->Party.AddDefaulted_GetRef();
            Member.bCreated = Member.bHasSkillLoadout = true;
            Member.SlotIndex = 2;
            Member.CharacterId = FGuid(5, 6, 7, 8);
            Member.OwnerAccountId = Save->Identity.HostAccountId;
            Member.Gold = 127;
            Member.CurrentHP = 73.0f;
            Member.Skills = {MakeOffer(0).Skill, RemovedPaths[0], RemovedPaths[1], RemovedPaths[2]};
            Member.Items.AddDefaulted();
            FRunNodeDefinition& Node = Save->Nodes.AddDefaulted_GetRef();
            Node.NodeId = TEXT("Combat_03");
            Node.EncounterId = TEXT("FixtureEncounter");
            Save->CompletedNodes = {TEXT("Combat_01"), TEXT("Combat_02")};
            Save->CurrentNode = TEXT("Combat_02");
            Save->Phase = ERunPhase::Shop;
            Save->Result = ECombatResult::Victory;
            FRunSkillShopState& Shop = Save->SkillShopState;
            Shop.SchemaVersion = 1;
            Shop.Revision = 11;
            Shop.RerollPrice = 7;
            Shop.Recovery.Price = 9;
            Shop.Query = FGameplayTagQuery::MakeQuery_MatchTag(TAG_RunMigrationEligible);
            for (int32 Index = 0; Index < 7; ++Index) Shop.Catalog.Add(MakeOffer(Index, TAG_RunMigrationEligible, Index == 5 ? 2.0f : Index == 6 ? 7.0f : 1.0f));
            Shop.Catalog[4].Skill = RemovedPaths[0];
            Shop.Catalog.Add(MakeOffer(7, TAG_RunMigrationExcluded, 10000.0f));
            Shop.Catalog.Add(MakeOffer(8, TAG_RunMigrationEligible, 0.0f));
            RetainedOffers = {Shop.Catalog[0], Shop.Catalog[1], Shop.Catalog[2], Shop.Catalog[3]};
            Shop.Offers = {RetainedOffers[0], RetainedOffers[1], Shop.Catalog[4], RetainedOffers[2], RetainedOffers[3]};
        }
    };

    bool SameShop(const FRunSkillShopState& Left, const FRunSkillShopState& Right)
    {
        return FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRemovedSkillIdentityTest, "ProjectA.Run.ContentMigration.ExactRemovedIdentities", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRemovedSkillIdentityTest::RunTest(const FString& Parameters)
{
    for (const FSoftObjectPath& Path : RemovedPaths) TestTrue(TEXT("Each exact historical or current skill path is removed"), RunContentMigration::IsRemovedSkill(Path));
    for (FName Id : {FName(TEXT("SweepingStrike")), FName(TEXT("SkillDefinitionDataAsset:DA_SweepingStrike")), FName(TEXT("SkillDefinitionDataAsset:BPDA_SweepingStrike"))}) TestTrue(TEXT("Each supported historical skill identifier is removed"), RunContentMigration::IsRemovedSkillId(Id));
    for (FName Id : {FName(TEXT("Whirlwind")), FName(TEXT("Sweep")), FName(TEXT("AnotherType:DA_SweepingStrike")), FName(TEXT("SweepingStrike_Copy"))}) TestFalse(TEXT("Other named skills and similar identifiers remain available"), RunContentMigration::IsRemovedSkillId(Id));
    for (const TCHAR* Path : {TEXT("/Game/AnotherPack/DA_SweepingStrike.DA_SweepingStrike"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_SweepingStrike_Copy.BPDA_SweepingStrike_Copy"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_NS_Mage_Whirlwind.BPDA_NS_Mage_Whirlwind"), TEXT("/Game/RPGEffects/ParticlesNiagara/Mage/Whirlwind/NS_Mage_Whirlwind.NS_Mage_Whirlwind")}) TestFalse(TEXT("Other packages derived names and original whirlwind effects are preserved"), RunContentMigration::IsRemovedSkill(FSoftObjectPath(Path)));
    TestFalse(TEXT("An empty path cannot be treated as deleted content"), RunContentMigration::IsRemovedSkill(FSoftObjectPath()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRemovedSkillShopRepairTest, "ProjectA.Run.ContentMigration.TaggedShopRepairAndIdempotence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRemovedSkillShopRepairTest::RunTest(const FString& Parameters)
{
    FRunMigrationFixture Fixture;
    TStrongObjectPtr<URunSaveGame> Repeated(DuplicateObject<URunSaveGame>(Fixture.Save.Get(), GetTransientPackage()));
    FRunPartyMember ExpectedMember = Fixture.Save->Party[0];
    ExpectedMember.Skills.SetNum(1);
    const FRunIdentityData Identity = Fixture.Save->Identity;
    FText Error;
    if (!TestTrue(TEXT("A deleted displayed skill is repaired from frozen candidates"), RunContentMigration::RemoveDeletedSkills(*Fixture.Save, Error))) return false;
    TestTrue(TEXT("Success clears the migration explanation"), Error.IsEmpty());
    const FRunSkillShopState& Shop = Fixture.Save->SkillShopState;
    if (!TestEqual(TEXT("Repair retains all five displayed slots"), Shop.Offers.Num(), 5)) return false;
    TestEqual(TEXT("Only the removed catalog candidate is discarded"), Shop.Catalog.Num(), 8);
    for (int32 Index = 0; Index < Fixture.RetainedOffers.Num(); ++Index) TestTrue(TEXT("Retained offers keep order identity tags weights and prices without rerolling"), FRunSkillShopOffer::StaticStruct()->CompareScriptStruct(&Fixture.RetainedOffers[Index], &Shop.Offers[Index], 0));
    const FRunSkillShopOffer& Replacement = Shop.Offers.Last();
    TestTrue(TEXT("The replacement satisfies the saved tag query and has positive weight"), Shop.Query.Matches(Replacement.Tags) && Replacement.BaseWeight > 0.0f && (Replacement.Skill == MakeOffer(5).Skill || Replacement.Skill == MakeOffer(6).Skill));
    const FRunSkillShopOffer* Candidate = Shop.Catalog.FindByPredicate([&Replacement](const FRunSkillShopOffer& Offer) { return Offer.Skill == Replacement.Skill; });
    TestTrue(TEXT("Replacement preserves its authored price description tags and weight"), Candidate && Replacement.Price == Candidate->Price && Replacement.Description.ToString() == Candidate->Description.ToString() && Replacement.Tags == Candidate->Tags && Replacement.BaseWeight == Candidate->BaseWeight);
    TSet<FSoftObjectPath> Paths;
    for (const FRunSkillShopOffer& Offer : Shop.Offers) Paths.Add(Offer.Skill);
    TestEqual(TEXT("Repair never duplicates a retained offer"), Paths.Num(), 5);
    TestTrue(TEXT("No gold is spent and all other party state is preserved"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&ExpectedMember, &Fixture.Save->Party[0], 0));
    TestTrue(TEXT("Run identity ownership and host epoch remain unchanged"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Fixture.Save->Identity, 0));
    TestTrue(TEXT("Run node progress phase and result remain unchanged"), Fixture.Save->Nodes.Num() == 1 && Fixture.Save->Nodes[0].NodeId == TEXT("Combat_03") && Fixture.Save->Nodes[0].EncounterId == TEXT("FixtureEncounter") && Fixture.Save->CompletedNodes == TArray<FName>{TEXT("Combat_01"), TEXT("Combat_02")} && Fixture.Save->CurrentNode == TEXT("Combat_02") && Fixture.Save->Phase == ERunPhase::Shop && Fixture.Save->Result == ECombatResult::Victory);
    TestTrue(TEXT("Repair advances one revision without changing reroll or recovery prices"), Shop.Revision == 12 && Shop.RerollPrice == 7 && Shop.Recovery.Price == 9);
    if (!TestTrue(TEXT("The same original save can be independently migrated"), RunContentMigration::RemoveDeletedSkills(*Repeated, Error))) return false;
    TestTrue(TEXT("The same original save gives the same full shop state and replacement identifier"), SameShop(Shop, Repeated->SkillShopState));
    const FRunSkillShopState Once = Shop;
    TestTrue(TEXT("Repeated migration is successful and changes no shop or party value"), RunContentMigration::RemoveDeletedSkills(*Fixture.Save, Error) && SameShop(Once, Fixture.Save->SkillShopState) && FRunPartyMember::StaticStruct()->CompareScriptStruct(&ExpectedMember, &Fixture.Save->Party[0], 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRemovedSkillCatalogAndLegacyTest, "ProjectA.Run.ContentMigration.CatalogOnlyAndLegacyFixedOffers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRemovedSkillCatalogAndLegacyTest::RunTest(const FString& Parameters)
{
    FRunMigrationFixture CatalogOnly;
    CatalogOnly.Save->SkillShopState.Offers = CatalogOnly.RetainedOffers;
    CatalogOnly.Save->SkillShopState.Offers.Add(CatalogOnly.Save->SkillShopState.Catalog[5]);
    FRunSkillShopState ExpectedCatalog = CatalogOnly.Save->SkillShopState;
    ExpectedCatalog.Catalog.RemoveAt(4);
    ++ExpectedCatalog.Revision;
    FText Error;
    TestTrue(TEXT("Catalog-only removal preserves all displayed offers and advances exactly one revision"), RunContentMigration::RemoveDeletedSkills(*CatalogOnly.Save, Error) && SameShop(ExpectedCatalog, CatalogOnly.Save->SkillShopState));
    FRunMigrationFixture Legacy;
    Legacy.Save->SkillShopState.Offers = {Legacy.RetainedOffers[0], Legacy.Save->SkillShopState.Catalog[4], Legacy.RetainedOffers[1], Legacy.RetainedOffers[2]};
    Legacy.Save->SkillShopState.Catalog.Reset();
    Legacy.Save->SkillShopState.Query = FGameplayTagQuery();
    Legacy.Save->SkillShopState.Revision = 0;
    Legacy.Save->SkillShopState.RerollPrice = 1;
    FRunSkillShopState ExpectedLegacy = Legacy.Save->SkillShopState;
    ExpectedLegacy.Offers.RemoveAt(1);
    TestTrue(TEXT("A legacy fixed shop removes just the deleted offer and preserves revision zero and price one"), RunContentMigration::RemoveDeletedSkills(*Legacy.Save, Error) && SameShop(ExpectedLegacy, Legacy.Save->SkillShopState));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRemovedSkillFailureTest, "ProjectA.Run.ContentMigration.UnrepairableCandidatesAndEmptyLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRemovedSkillFailureTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 4; ++Case)
    {
        FRunMigrationFixture Original;
        if (Case == 0) Original.Save->SkillShopState.Catalog.SetNum(5);
        if (Case == 1)
        {
            Original.Save->SkillShopState.Catalog[5].BaseWeight = 0.0f;
            Original.Save->SkillShopState.Catalog[6].Tags = FGameplayTagContainer(TAG_RunMigrationExcluded);
        }
        if (Case == 2) Original.Save->SkillShopState.Revision = MAX_int32;
        if (Case == 3) Original.Save->Party[0].Skills = {RemovedPaths[0]};
        const FRunSkillShopState OriginalShop = Original.Save->SkillShopState;
        const FRunPartyMember OriginalMember = Original.Save->Party[0];
        TStrongObjectPtr<URunSaveGame> Candidate(DuplicateObject<URunSaveGame>(Original.Save.Get(), GetTransientPackage()));
        FText Error;
        TestFalse(TEXT("An insufficient catalog filtered replacements exhausted revision or deleted-only loadout rejects the candidate"), RunContentMigration::RemoveDeletedSkills(*Candidate, Error));
        TestFalse(TEXT("Every rejected candidate provides a visible explanation"), Error.IsEmpty());
        TestTrue(TEXT("Migrating an isolated candidate leaves every original shop and party value unchanged"), SameShop(OriginalShop, Original.Save->SkillShopState) && FRunPartyMember::StaticStruct()->CompareScriptStruct(&OriginalMember, &Original.Save->Party[0], 0));
    }
    return true;
}

#endif
