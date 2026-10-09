#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/EncounterStageVisualCatalog.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Game/Run/RunEncounterPool.h"
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
    Offer = FRunEncounterOffer();
    Offer.EncounterId = TEXT("HistoricalRecoveryIdentity");
    Offer.EncounterTag = FRunEncounterOffer::GetRecoveryTag();
    Offer.DisplayName = FText::FromString(TEXT("회복소"));
    const FRunEncounterOffer SavedOffer = Offer;
    TestEqual(TEXT("Historical recovery labels resolve to the canonical spring name independent of their saved identifier"), Offer.GetDisplayName().BuildSourceString(), FString(TEXT("회복의 샘물")));
    TestTrue(TEXT("Display compatibility never rewrites the serialized offer"), FRunEncounterOffer::StaticStruct()->CompareScriptStruct(&Offer, &SavedOffer, 0));
    Offer.DisplayName = FText::FromString(TEXT("사용자 회복 정원"));
    TestEqual(TEXT("Custom recovery names are preserved exactly"), Offer.GetDisplayName().BuildSourceString(), Offer.DisplayName.BuildSourceString());
    Offer.DisplayName = FText::FromString(TEXT("회복소"));
    Offer.EncounterTag = FRunEncounterOffer::GetBasicItemShopTag();
    TestEqual(TEXT("The old label alone cannot rename a non-recovery encounter"), Offer.GetDisplayName().BuildSourceString(), FString(TEXT("회복소")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterLibraryProfilesTest, "ProjectA.Run.EncounterPresentation.LibraryProfilesAndCookReferences", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterLibraryProfilesTest::RunTest(const FString& Parameters)
{
    const UEncounterStageVisualCatalog* Catalog = GetDefault<UEncounterStageVisualCatalog>();
    TestFalse(TEXT("The native catalog contains authored encounter appearances"), Catalog->Profiles.IsEmpty());
    const FGameplayTag StageTags[] = {FRunEncounterOffer::GetBasicItemShopTag(), FRunEncounterOffer::GetRarityItemShopTag(), FRunEncounterOffer::GetTagItemShopTag(), FRunEncounterOffer::GetRecoveryTag(), FRunEncounterOffer::GetConsumableShopTag(), FRunEncounterOffer::GetRevivalTag()};
    TSet<FName> ResolvedProfiles;
    const FEncounterStageVisualProfile* Market = nullptr;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(StageTags); ++Index)
    {
        const FEncounterStageVisualProfile* Profile = Catalog->Resolve(FGameplayTagContainer(StageTags[Index]));
        if (!TestNotNull(TEXT("Every authored encounter group resolves a library appearance through tags"), Profile)) return false;
        TestFalse(TEXT("A resolved visual profile has an explicit identity"), Profile->ProfileId.IsNone());
        if (Index == 0) Market = Profile;
        if (Index == 1) TestTrue(TEXT("A rarity shop without a merchandise query keeps the generic merchant fallback"), Profile == Market);
        ResolvedProfiles.Add(Profile->ProfileId);
    }
    TestTrue(TEXT("Services and specialized shops retain distinct generic appearances"), ResolvedProfiles.Num() > 1);
    TestNull(TEXT("Missing classification cannot silently select a merchant appearance"), Catalog->Resolve(FGameplayTagContainer()));

    TSet<FSoftObjectPath> ExpectedAssets;
    TSet<FName> ProfileIds;
    for (const FEncounterStageVisualProfile& Profile : Catalog->Profiles)
    {
        TestFalse(TEXT("Profile IDs are unique"), ProfileIds.Contains(Profile.ProfileId));
        ProfileIds.Add(Profile.ProfileId);
        TestFalse(TEXT("The appearance condition is explicitly tag based"), Profile.StageQuery.IsEmpty());
        TestTrue(TEXT("Character appearances reference a body and idle while environment-only stages omit both"), Profile.bEnvironmentOnly ? Profile.CharacterMesh.IsNull() && Profile.IdleAnimation.IsNull() && Profile.CharacterParts.IsEmpty() : !Profile.CharacterMesh.IsNull() && !Profile.IdleAnimation.IsNull());
        TestTrue(TEXT("Each appearance includes service props"), !Profile.Props.IsEmpty());
        TestTrue(TEXT("Character scale is finite and positive"), FMath::IsFinite(Profile.CharacterHeight) && Profile.CharacterHeight > 0.f);
        TestTrue(TEXT("Every stage has a finite positive presentation focus"), !Profile.PresentationFocus.ContainsNaN() && FMath::IsFinite(Profile.PresentationFocusRadius) && Profile.PresentationFocusRadius > 0.f);
        if (!Profile.CharacterMesh.IsNull()) ExpectedAssets.Add(Profile.CharacterMesh.ToSoftObjectPath());
        if (!Profile.IdleAnimation.IsNull()) ExpectedAssets.Add(Profile.IdleAnimation.ToSoftObjectPath());
        for (const auto& Part : Profile.CharacterParts) ExpectedAssets.Add(Part.ToSoftObjectPath());
        for (const FEncounterStageProp& Prop : Profile.Props)
        {
            TestTrue(TEXT("Props use finite positive fit bounds"), !Prop.MaxSize.ContainsNaN() && Prop.MaxSize.GetMin() > 0.f);
            TestTrue(TEXT("Environment props cannot reference a missing NPC attachment bone"), !Profile.bEnvironmentOnly || Prop.AttachBone.IsNone());
            ExpectedAssets.Add(Prop.Mesh.ToSoftObjectPath());
            if (!Prop.MaterialOverride.IsNull()) ExpectedAssets.Add(Prop.MaterialOverride.ToSoftObjectPath());
        }
    }
    TArray<FSoftObjectPath> CookAssets;
    Catalog->GetReferencedAssets(CookAssets);
    TestEqual(TEXT("Cook enumeration includes exactly the selected body, idle, modular parts, props and material overrides"), CookAssets.Num(), ExpectedAssets.Num());
    TSet<FSoftObjectPath> UniqueAssets;
    for (const FSoftObjectPath& Asset : CookAssets)
    {
        TestTrue(TEXT("Every cooked reference belongs to an actual selected visual profile"), ExpectedAssets.Contains(Asset));
        TestFalse(TEXT("Shared source assets are enumerated only once for cooking"), UniqueAssets.Contains(Asset));
        TestTrue(TEXT("The direct source reference identifies an installed game or engine asset"), Asset.IsValid() && (Asset.GetLongPackageName().StartsWith(TEXT("/Game/")) || Asset.GetLongPackageName().StartsWith(TEXT("/Engine/"))) && FPackageName::DoesPackageExist(Asset.GetLongPackageName()));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterMerchandiseProfilesTest, "ProjectA.Run.EncounterPresentation.MerchandiseRolesAndHealingSpring", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterMerchandiseProfilesTest::RunTest(const FString& Parameters)
{
    const UEncounterStageVisualCatalog* Catalog = GetDefault<UEncounterStageVisualCatalog>();
    FRunTargetState State;
    State.SchemaVersion = 1;
    FText Error;
    if (!TestTrue(TEXT("The active CSV encounter pool loads with its saved merchandise queries"), RunEncounterPool::Load(State, Error))) return false;
    const TMap<FName, FName> ExpectedProfiles = {
        {TEXT("Shop_Item_Basic"), TEXT("Market")},
        {TEXT("Shop_Item_Rarity_White"), TEXT("RarityWhite")},
        {TEXT("Shop_Item_Rarity_Green"), TEXT("RarityGreen")},
        {TEXT("Shop_Item_Rarity_Blue"), TEXT("RarityBlue")},
        {TEXT("Shop_Item_Rarity_Purple"), TEXT("RarityPurple")},
        {TEXT("Shop_Item_Tag_Sword"), TEXT("Swordsmith")},
        {TEXT("Shop_Item_Tag_Dagger"), TEXT("Rogue")},
        {TEXT("Shop_Item_Tag_Bow"), TEXT("Ranger")},
        {TEXT("Shop_Item_Tag_StaffWand"), TEXT("Arcanist")},
        {TEXT("Shop_Item_Tag_Shield"), TEXT("Warden")},
        {TEXT("TargetOffer_03"), TEXT("HealingSpring")},
        {TEXT("TargetOffer_04"), TEXT("Alchemy")},
        {TEXT("TargetOffer_05"), TEXT("Shrine")}
    };
    for (const FRunEncounterOffer& Offer : State.EncounterPool)
    {
        const FName* Expected = ExpectedProfiles.Find(Offer.EncounterId);
        const FEncounterStageVisualProfile* Profile = Catalog->Resolve(Offer);
        if (!TestTrue(TEXT("Every active authored offer resolves the appearance of its actual merchandise or service"), Expected && Profile && Profile->ProfileId == *Expected)) return false;
        TestTrue(TEXT("A specialized appearance matches at least one full merchandise sample including rarity"), Profile->MerchandiseSamples.IsEmpty() || Profile->MerchandiseSamples.ContainsByPredicate([&Offer](const FGameplayTagContainer& Sample) { return Offer.ItemQuery.Matches(Sample); }));
        const FRunEncounterOffer Before = Offer;
        FRunEncounterOffer Renamed = Offer;
        Renamed.EncounterId = TEXT("MisleadingSwordRecoveryIdentity");
        Renamed.DisplayName = FText::FromString(TEXT("방패 회복소 전설 상점"));
        TestTrue(TEXT("Saved names and IDs cannot override tag and merchandise-based presentation"), Catalog->Resolve(Renamed) == Profile && RunEncounterPool::IsSameOffer(Offer, Before));
        if (Offer.GetResolvedTag() == FRunEncounterOffer::GetRecoveryTag())
        {
            TestTrue(TEXT("Recovery uses the canonical spring name with an environment-only scene"), Offer.GetDisplayName().BuildSourceString() == TEXT("회복의 샘물") && Profile->bEnvironmentOnly && Profile->CharacterMesh.IsNull() && Profile->IdleAnimation.IsNull());
            TestTrue(TEXT("The spring includes a flat water surface whose fit box remains positive"), Profile->Props.ContainsByPredicate([](const FEncounterStageProp& Prop) { return !Prop.Mesh.IsNull() && Prop.MaxSize.Z <= 1.f && Prop.MaxSize.GetMin() > 0.f; }));
        }
    }
    TestEqual(TEXT("The expected appearance fixtures cover every active CSV offer"), State.EncounterPool.Num(), ExpectedProfiles.Num());
    FRunEncounterOffer Filtered;
    Filtered.EncounterTag = FRunEncounterOffer::GetTagItemShopTag();
    const FGameplayTag Bow = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Bow"));
    const FGameplayTag Dagger = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger"));
    FGameplayTagQueryExpression Root;
    Root.AllExprMatch();
    FGameplayTagQueryExpression Either;
    Either.AnyTagsMatch().AddTag(Bow).AddTag(Dagger);
    FGameplayTagQueryExpression Excluded;
    Excluded.NoTagsMatch().AddTag(Bow);
    Root.AddExpr(Either).AddExpr(Excluded);
    Filtered.ItemQuery = FGameplayTagQuery::BuildQuery(Root);
    const FEncounterStageVisualProfile* Selected = Catalog->Resolve(Filtered);
    TestTrue(TEXT("An explicit bow exclusion prevents the ranger from winning a bow-or-dagger query"), Selected && Selected->ProfileId == FName(TEXT("Rogue")));
    Filtered.ItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Crossbow")));
    Selected = Catalog->Resolve(Filtered);
    TestTrue(TEXT("An unrepresented merchandise type retains the generic forge fallback"), Selected && Selected->ProfileId == FName(TEXT("Forge")));
    Filtered.ItemQuery = FGameplayTagQuery();
    Selected = Catalog->Resolve(Filtered);
    TestTrue(TEXT("A historical shop without a merchandise query keeps its generic appearance"), Selected && Selected->ProfileId == FName(TEXT("Forge")));
    Filtered.EncounterTag = FRunEncounterOffer::GetRarityItemShopTag();
    Filtered.ItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Orange")));
    Selected = Catalog->Resolve(Filtered);
    TestTrue(TEXT("The orange rarity appearance is ready without enabling an inactive encounter in the CSV"), Selected && Selected->ProfileId == FName(TEXT("RarityOrange")));
    return true;
}

#endif
