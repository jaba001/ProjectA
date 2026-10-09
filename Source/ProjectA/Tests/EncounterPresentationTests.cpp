#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/EncounterStageVisualCatalog.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterPresentationTagsTest, "ProjectA.Run.EncounterPresentation.TagBasedStageMatching", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterPresentationTagsTest::RunTest(const FString& Parameters)
{
    FRunEncounterOffer Offer;
    Offer.EncounterId = TEXT("AnySavedIdentity");
    Offer.DisplayName = FText::FromString(TEXT("Unrelated visible name"));
    Offer.EncounterTag = FRunEncounterOffer::GetRarityItemShopTag();
    Offer.SelectionGroupTag = FRunEncounterOffer::GetRarityItemShopTag();
    const FGameplayTagContainer ItemShops(FRunEncounterOffer::GetItemShopTag());
    const FGameplayTagContainer RarityShops(FRunEncounterOffer::GetRarityItemShopTag());
    const FGameplayTagContainer TagShops(FRunEncounterOffer::GetTagItemShopTag());
    TestTrue(TEXT("A child item-shop tag matches its authored parent stage"), EncounterPresentation::MatchesOffer(Offer, ItemShops, TagShops));
    TestFalse(TEXT("An excluded parent rejects its child even when required tags match"), EncounterPresentation::MatchesOffer(Offer, RarityShops, ItemShops));
    Offer.EncounterId = TEXT("CompletelyDifferentIdentity");
    Offer.DisplayName = FText::FromString(TEXT("Recovery or weapon names do not classify scenery"));
    TestTrue(TEXT("Changing saved IDs or display names cannot change explicit tag matching"), EncounterPresentation::MatchesOffer(Offer, ItemShops, TagShops));
    Offer.EncounterTag = FRunEncounterOffer::GetItemShopTag();
    TestTrue(TEXT("Selection-group tags also participate in presentation matching"), EncounterPresentation::MatchesOffer(Offer, RarityShops, TagShops));
    Offer.SelectionGroupTag = FRunEncounterOffer::GetTagItemShopTag();
    TestFalse(TEXT("An excluded group prevents a generic market from swallowing a specialized stage"), EncounterPresentation::MatchesOffer(Offer, ItemShops, TagShops));
    Offer.EncounterTag = FRunEncounterOffer::GetRecoveryTag();
    Offer.SelectionGroupTag = FGameplayTag();
    const FGameplayTagContainer Recovery(FRunEncounterOffer::GetRecoveryTag());
    TestTrue(TEXT("The recovery service selects its own authored stage"), EncounterPresentation::MatchesOffer(Offer, Recovery, FGameplayTagContainer()));
    TestFalse(TEXT("A recovery service cannot match an item-shop stage"), EncounterPresentation::MatchesOffer(Offer, ItemShops, FGameplayTagContainer()));
    FGameplayTagContainer Both = ItemShops;
    Both.AddTag(FRunEncounterOffer::GetRecoveryTag());
    TestFalse(TEXT("Multiple required tags use all-match semantics"), EncounterPresentation::MatchesOffer(Offer, Both, FGameplayTagContainer()));
    Offer = FRunEncounterOffer();
    Offer.EncounterId = TEXT("Shop_02");
    TestTrue(TEXT("Legacy untagged offers retain the common resolved-tag compatibility"), EncounterPresentation::MatchesOffer(Offer, ItemShops, FGameplayTagContainer()));
    Offer.Type = static_cast<ERunEncounterType>(255);
    TestFalse(TEXT("An unresolved encounter cannot enter an unrestricted stage"), EncounterPresentation::MatchesOffer(Offer, FGameplayTagContainer(), FGameplayTagContainer()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterLibraryProfilesTest, "ProjectA.Run.EncounterPresentation.LibraryProfilesAndCookReferences", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterLibraryProfilesTest::RunTest(const FString& Parameters)
{
    const UEncounterStageVisualCatalog* Catalog = GetDefault<UEncounterStageVisualCatalog>();
    TestEqual(TEXT("The native catalog defines the five authored service appearances"), Catalog->Profiles.Num(), 5);
    const FGameplayTag StageTags[] = {FRunEncounterOffer::GetBasicItemShopTag(), FRunEncounterOffer::GetRarityItemShopTag(), FRunEncounterOffer::GetTagItemShopTag(), FRunEncounterOffer::GetRecoveryTag(), FRunEncounterOffer::GetConsumableShopTag(), FRunEncounterOffer::GetRevivalTag()};
    TSet<FName> ResolvedProfiles;
    const FEncounterStageVisualProfile* Market = nullptr;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(StageTags); ++Index)
    {
        const FEncounterStageVisualProfile* Profile = Catalog->Resolve(FGameplayTagContainer(StageTags[Index]));
        if (!TestNotNull(TEXT("Every authored encounter group resolves a library appearance through tags"), Profile)) return false;
        TestFalse(TEXT("A resolved visual profile has an explicit identity"), Profile->ProfileId.IsNone());
        if (Index == 0) Market = Profile;
        if (Index == 1) TestTrue(TEXT("Basic and rarity item shops share the general merchant appearance"), Profile == Market);
        ResolvedProfiles.Add(Profile->ProfileId);
    }
    TestEqual(TEXT("Six encounter groups resolve exactly five distinct appearances"), ResolvedProfiles.Num(), 5);
    TestNull(TEXT("Missing classification cannot silently select a merchant appearance"), Catalog->Resolve(FGameplayTagContainer()));

    TSet<FSoftObjectPath> ExpectedAssets;
    TSet<FName> ProfileIds;
    for (const FEncounterStageVisualProfile& Profile : Catalog->Profiles)
    {
        TestFalse(TEXT("Profile IDs are unique"), ProfileIds.Contains(Profile.ProfileId));
        ProfileIds.Add(Profile.ProfileId);
        TestFalse(TEXT("The appearance condition is explicitly tag based"), Profile.StageQuery.IsEmpty());
        TestTrue(TEXT("Each appearance references an original character and idle clip"), !Profile.CharacterMesh.IsNull() && !Profile.IdleAnimation.IsNull());
        TestTrue(TEXT("Each appearance includes service props"), !Profile.Props.IsEmpty());
        TestTrue(TEXT("Character scale is finite and positive"), FMath::IsFinite(Profile.CharacterHeight) && Profile.CharacterHeight > 0.f);
        ExpectedAssets.Add(Profile.CharacterMesh.ToSoftObjectPath());
        ExpectedAssets.Add(Profile.IdleAnimation.ToSoftObjectPath());
        for (const auto& Part : Profile.CharacterParts) ExpectedAssets.Add(Part.ToSoftObjectPath());
        for (const FEncounterStageProp& Prop : Profile.Props)
        {
            TestTrue(TEXT("Props use finite positive fit bounds"), !Prop.MaxSize.ContainsNaN() && Prop.MaxSize.GetMin() > 0.f);
            ExpectedAssets.Add(Prop.Mesh.ToSoftObjectPath());
        }
    }
    TArray<FSoftObjectPath> CookAssets;
    Catalog->GetReferencedAssets(CookAssets);
    TestEqual(TEXT("Cook enumeration includes exactly the selected body, idle, modular parts and prop assets"), CookAssets.Num(), ExpectedAssets.Num());
    TSet<FSoftObjectPath> UniqueAssets;
    for (const FSoftObjectPath& Asset : CookAssets)
    {
        TestTrue(TEXT("Every cooked reference belongs to an actual selected visual profile"), ExpectedAssets.Contains(Asset));
        TestFalse(TEXT("Shared source assets are enumerated only once for cooking"), UniqueAssets.Contains(Asset));
        TestTrue(TEXT("The direct source reference identifies an installed game asset"), Asset.IsValid() && Asset.GetLongPackageName().StartsWith(TEXT("/Game/")) && FPackageName::DoesPackageExist(Asset.GetLongPackageName()));
        UniqueAssets.Add(Asset);
    }
    const FSoftObjectPath ExistingReference(TEXT("/Game/UnrelatedCookFixture.Asset"));
    TArray<FSoftObjectPath> AppendedAssets = {ExistingReference};
    Catalog->GetReferencedAssets(AppendedAssets);
    Catalog->GetReferencedAssets(AppendedAssets);
    TestTrue(TEXT("Repeated cook enumeration preserves earlier callers' references without duplicates"), AppendedAssets.Num() == CookAssets.Num() + 1 && AppendedAssets[0] == ExistingReference);

    TStrongObjectPtr<UEncounterStageVisualCatalog> Synthetic(NewObject<UEncounterStageVisualCatalog>());
    FEncounterStageVisualProfile First;
    First.ProfileId = TEXT("A_Visual");
    First.StageQuery = FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetItemShopTag());
    FEncounterStageVisualProfile Second = First;
    Second.ProfileId = TEXT("B_Visual");
    Second.Priority = 1;
    Synthetic->Profiles = {First, Second};
    const FGameplayTagContainer ItemTags(FRunEncounterOffer::GetBasicItemShopTag());
    const FEncounterStageVisualProfile* Selected = Synthetic->Resolve(ItemTags);
    TestTrue(TEXT("The highest visual priority wins independently of insertion order"), Selected && Selected->ProfileId == Second.ProfileId);
    Synthetic->Profiles[1].Priority = 0;
    Swap(Synthetic->Profiles[0], Synthetic->Profiles[1]);
    Selected = Synthetic->Resolve(ItemTags);
    TestTrue(TEXT("Equal visual priorities resolve deterministically by profile identity"), Selected && Selected->ProfileId == First.ProfileId);
    TestFalse(TEXT("Scenery does not gain replicated gameplay authority"), GetDefault<AEncounterPrototypeStage>()->GetIsReplicated());
    return true;
}

#endif
