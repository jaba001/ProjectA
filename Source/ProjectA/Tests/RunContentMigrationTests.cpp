#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunSaveGame.h"
#include "UObject/StrongObjectPtr.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RunMigrationEligible, "ProjectA.Test.RunMigration.Eligible");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RunMigrationExcluded, "ProjectA.Test.RunMigration.Excluded");

namespace
{
    const FSoftObjectPath RemovedPaths[] = {FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_SweepingStrike.BPDA_SweepingStrike")), FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DA_SweepingStrike.DA_SweepingStrike")), FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_SweepingStrike.DA_SweepingStrike")), FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_AreaAttack.BPDA_AreaAttack")), FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/BPDA_AreaAttack.BPDA_AreaAttack")), FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_RangedAttack.BPDA_RangedAttack"))};

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
            Member.Skills = {MakeOffer(0).Skill, RemovedPaths[0], RemovedPaths[3], RemovedPaths[5]};
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
    for (FName Id : {FName(TEXT("SweepingStrike")), FName(TEXT("SkillDefinitionDataAsset:DA_SweepingStrike")), FName(TEXT("SkillDefinitionDataAsset:BPDA_SweepingStrike")), FName(TEXT("AOE")), FName(TEXT("SkillDefinitionDataAsset:BPDA_AreaAttack")), FName(TEXT("RangedAttack")), FName(TEXT("SkillDefinitionDataAsset:BPDA_RangedAttack"))}) TestTrue(TEXT("Each supported historical skill identifier is removed"), RunContentMigration::IsRemovedSkillId(Id));
    for (FName Id : {FName(TEXT("Whirlwind")), FName(TEXT("Sweep")), FName(TEXT("AreaAttack")), FName(TEXT("AnotherType:DA_SweepingStrike")), FName(TEXT("SweepingStrike_Copy")), FName(TEXT("RangedAttack_Copy"))}) TestFalse(TEXT("Other named skills and similar identifiers remain available"), RunContentMigration::IsRemovedSkillId(Id));
    for (const TCHAR* Path : {TEXT("/Game/AnotherPack/DA_SweepingStrike.DA_SweepingStrike"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_SweepingStrike_Copy.BPDA_SweepingStrike_Copy"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack.BPDA_DefaulatAttack"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Monsters/DA_Monster_Bear.DA_Monster_Bear"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_FutureVfxSkill.BPDA_FutureVfxSkill")}) TestFalse(TEXT("Retained monster attacks base skills similar names and future content stay available"), RunContentMigration::IsRemovedSkill(FSoftObjectPath(Path)));
    for (const TCHAR* Path : {TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_NS_Mage_Whirlwind.BPDA_NS_Mage_Whirlwind"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_NS_Mage_Whirlwind_d5d622a9.BPDA_NS_Mage_Whirlwind_d5d622a9"), TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Catalog/RPGEffects/ParticlesNiagara/Mage/Whirlwind/BPDA_NS_Mage_Whirlwind_d5d622a9.BPDA_NS_Mage_Whirlwind_d5d622a9")}) TestTrue(TEXT("Current and both exact historical catalog paths retire together"), RunContentMigration::IsRemovedSkill(FSoftObjectPath(Path)));
    for (FName Id : {FName(TEXT("Catalog_438db4e2237d4037")), FName(TEXT("SkillDefinitionDataAsset:BPDA_NS_Mage_Whirlwind")), FName(TEXT("SkillDefinitionDataAsset:BPDA_NS_Mage_Whirlwind_d5d622a9"))}) TestTrue(TEXT("Authored catalog and current or renamed primary identities retire together"), RunContentMigration::IsRemovedSkillId(Id));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunTwoRemovedSkillShopRepairTest, "ProjectA.Run.ContentMigration.TwoDeletedOffersAndStableProgress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunTwoRemovedSkillShopRepairTest::RunTest(const FString& Parameters)
{
    FRunMigrationFixture Fixture;
    FRunSkillShopState& OriginalShop = Fixture.Save->SkillShopState;
    OriginalShop.Catalog[4].Skill = RemovedPaths[4];
    OriginalShop.Catalog[5].Skill = RemovedPaths[5];
    const TArray<FRunSkillShopOffer> Retained = {OriginalShop.Catalog[0], OriginalShop.Catalog[1], OriginalShop.Catalog[2]};
    OriginalShop.Offers = {Retained[0], OriginalShop.Catalog[4], Retained[1], OriginalShop.Catalog[5], Retained[2]};
    TStrongObjectPtr<URunSaveGame> Repeated(DuplicateObject<URunSaveGame>(Fixture.Save.Get(), GetTransientPackage()));
    FRunPartyMember ExpectedMember = Fixture.Save->Party[0];
    ExpectedMember.Skills.SetNum(1);
    const FRunIdentityData Identity = Fixture.Save->Identity;
    FText Error;
    if (!TestTrue(TEXT("Two deleted AOE and ranged offers are replaced together"), RunContentMigration::RemoveDeletedSkills(*Fixture.Save, Error))) return false;
    const FRunSkillShopState& Shop = Fixture.Save->SkillShopState;
    if (!TestEqual(TEXT("Two replacements restore exactly five displayed offers"), Shop.Offers.Num(), 5)) return false;
    TestEqual(TEXT("Only the two deleted frozen candidates are removed"), Shop.Catalog.Num(), 7);
    for (int32 Index = 0; Index < Retained.Num(); ++Index) TestTrue(TEXT("Both holes leave retained offer identity order price text tags and weight untouched"), FRunSkillShopOffer::StaticStruct()->CompareScriptStruct(&Retained[Index], &Shop.Offers[Index], 0));
    TSet<FSoftObjectPath> Paths;
    for (const FRunSkillShopOffer& Offer : Shop.Offers) Paths.Add(Offer.Skill);
    TestEqual(TEXT("Two replacements never repeat one another or retained stock"), Paths.Num(), 5);
    for (int32 Index = Retained.Num(); Index < Shop.Offers.Num(); ++Index)
    {
        const FRunSkillShopOffer& Replacement = Shop.Offers[Index];
        const FRunSkillShopOffer* Candidate = Shop.Catalog.FindByPredicate([&Replacement](const FRunSkillShopOffer& Offer) { return Offer.Skill == Replacement.Skill; });
        TestTrue(TEXT("Each replacement respects the saved query positive weight and retained-stock exclusion"), Candidate && Shop.Query.Matches(Replacement.Tags) && Replacement.BaseWeight > 0.0f && (Replacement.Skill == MakeOffer(3).Skill || Replacement.Skill == MakeOffer(6).Skill));
        TestTrue(TEXT("Each replacement preserves its frozen price text tags and weight"), Candidate && Replacement.Price == Candidate->Price && Replacement.DisplayName.ToString() == Candidate->DisplayName.ToString() && Replacement.Description.ToString() == Candidate->Description.ToString() && Replacement.Tags == Candidate->Tags && Replacement.BaseWeight == Candidate->BaseWeight);
    }
    TestTrue(TEXT("Removing all three owned prototypes preserves gold HP ownership equipment and other party fields"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&ExpectedMember, &Fixture.Save->Party[0], 0));
    TestTrue(TEXT("Simultaneous deletion preserves Run identity and current progress"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Fixture.Save->Identity, 0) && Fixture.Save->CurrentNode == Repeated->CurrentNode && Fixture.Save->CompletedNodes == Repeated->CompletedNodes && Fixture.Save->Phase == Repeated->Phase && Fixture.Save->Result == Repeated->Result);
    TestTrue(TEXT("Two holes advance one revision and do not charge either reroll or recovery"), Shop.Revision == 12 && Shop.RerollPrice == 7 && Shop.Recovery.Price == 9);
    if (!TestTrue(TEXT("The same two-hole original candidate migrates independently"), RunContentMigration::RemoveDeletedSkills(*Repeated, Error))) return false;
    TestTrue(TEXT("Both replacements and their identifiers are deterministic"), SameShop(Shop, Repeated->SkillShopState));
    const FRunSkillShopState Once = Shop;
    TestTrue(TEXT("A second pass is idempotent after removing all three prototypes"), RunContentMigration::RemoveDeletedSkills(*Fixture.Save, Error) && SameShop(Once, Fixture.Save->SkillShopState));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRemovedSkillFailureTest, "ProjectA.Run.ContentMigration.InvalidRevisionOrSchema", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRemovedSkillFailureTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 2; ++Case)
    {
        FRunMigrationFixture Original;
        if (Case == 0) Original.Save->SkillShopState.Revision = MAX_int32;
        if (Case == 1) Original.Save->SkillShopState.SchemaVersion = 2;
        const FRunSkillShopState OriginalShop = Original.Save->SkillShopState;
        const FRunPartyMember OriginalMember = Original.Save->Party[0];
        TStrongObjectPtr<URunSaveGame> Candidate(DuplicateObject<URunSaveGame>(Original.Save.Get(), GetTransientPackage()));
        FText Error;
        TestFalse(TEXT("An exhausted revision or unsupported schema rejects the migration candidate"), RunContentMigration::RemoveDeletedSkills(*Candidate, Error));
        TestFalse(TEXT("Every rejected candidate provides a visible explanation"), Error.IsEmpty());
        TestTrue(TEXT("Migrating an isolated candidate leaves every original shop and party value unchanged"), SameShop(OriginalShop, Original.Save->SkillShopState) && FRunPartyMember::StaticStruct()->CompareScriptStruct(&OriginalMember, &Original.Save->Party[0], 0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRemovedSkillReducedCatalogTest, "ProjectA.Run.ContentMigration.ReducedCatalogAndRetiredOnlyFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRemovedSkillReducedCatalogTest::RunTest(const FString& Parameters)
{
    FRunMigrationFixture Fixture;
    FRunSkillShopState& Shop = Fixture.Save->SkillShopState;
    Shop.Catalog = {Shop.Catalog[0], Shop.Catalog[4]};
    Shop.Offers = Shop.Catalog;
    Fixture.Save->Party[0].Skills = {RemovedPaths[0]};
    FRunPartyMember ExpectedMember = Fixture.Save->Party[0];
    const FSoftObjectPath Unarmed(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack.BPDA_DefaulatAttack"));
    ExpectedMember.Skills = {Unarmed};
    const FRunSkillShopOffer Retained = Shop.Catalog[0];
    FText Error;
    if (!TestTrue(TEXT("A one-candidate catalog and retired-only party migrate successfully"), RunContentMigration::RemoveDeletedSkills(*Fixture.Save, Error))) return false;
    TestTrue(TEXT("Reduced stock keeps the existing candidate and every frozen offer field"), Shop.Catalog.Num() == 1 && Shop.Offers.Num() == 1 && FRunSkillShopOffer::StaticStruct()->CompareScriptStruct(&Retained, &Shop.Offers[0], 0));
    TestTrue(TEXT("Fallback replaces only the retired loadout without changing gold ownership HP or progress"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&ExpectedMember, &Fixture.Save->Party[0], 0));
    TestTrue(TEXT("Reducing stock advances only the revision and never charges a reroll"), Shop.Revision == 12 && Shop.RerollPrice == 7 && Shop.Recovery.Price == 9);
    const FRunSkillShopState Once = Shop;
    TestTrue(TEXT("The reduced catalog and unarmed fallback are idempotent"), RunContentMigration::RemoveDeletedSkills(*Fixture.Save, Error) && SameShop(Once, Shop));

    FRunMigrationFixture Empty;
    Empty.Save->SkillShopState.Catalog = {Empty.Save->SkillShopState.Catalog[4]};
    Empty.Save->SkillShopState.Offers = Empty.Save->SkillShopState.Catalog;
    if (!TestTrue(TEXT("A retired-only modern catalog retains its recovery service"), RunContentMigration::RemoveDeletedSkills(*Empty.Save, Error))) return false;
    TestTrue(TEXT("Empty modern stock preserves recovery and reroll prices without adding products"), Empty.Save->SkillShopState.Offers.IsEmpty() && Empty.Save->SkillShopState.Catalog.IsEmpty() && Empty.Save->SkillShopState.Revision == 12 && Empty.Save->SkillShopState.RerollPrice == 7 && Empty.Save->SkillShopState.Recovery.Price == 9);
    TestTrue(TEXT("Recovery-only modern state validates and cannot charge a reroll"), URunEncounterPoolDataAsset::ValidateSkillShop(Empty.Save->SkillShopState, Error) && !URunEncounterPoolDataAsset::RollSkillShop(Empty.Save->SkillShopState, false, Error));
    return true;
}

#endif
