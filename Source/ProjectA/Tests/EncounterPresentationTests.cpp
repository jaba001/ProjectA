#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/EncounterStageVisualCatalog.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Controller/GameplayPlayerController.h"
#include "Engine/World.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Encounter/EncounterDungeonRoute.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterDungeonDoorLifecycleTest, "ProjectA.Run.EncounterPresentation.DoorArrivalDepartureAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterDungeonDoorLifecycleTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
    if (!TestTrue(TEXT("A local presentation fixture has a world."), World.IsValid())) return false;
    AEncounterDungeonRoute* Route = World->SpawnActor<AEncounterDungeonRoute>();
    AEncounterPrototypeStage* Template = World->SpawnActor<AEncounterPrototypeStage>();
    AGameplayPlayerController* Controller = World->SpawnActor<AGameplayPlayerController>();
    if (!Route || !Template || !Controller)
    {
        World->DestroyWorld(false);
        return false;
    }
    Template->bUseLibraryVisuals = false;
    int32 Completions = 0;
    Route->OnTravelFinished.AddLambda([&Completions]() { ++Completions; });
    for (int32 Variant = 0; Variant < EncounterDungeonLayout::VariantCount; ++Variant)
    {
        TestTrue(TEXT("Every supported layout prepares its cached scenery."), Route->ConfigureLayout(Variant));
        TestEqual(TEXT("Three doors per room and one matching section entrance have paired original leaves."), Route->LeftDoors->GetInstanceCount(), 10);
        TestEqual(TEXT("The left and right door instances remain paired."), Route->RightDoors->GetInstanceCount(), 10);
        for (int32 Direction = 0; Direction < 3; ++Direction)
        {
            Route->ResetAtJunction();
            AEncounterPrototypeStage* Stage = Route->ConfigureStage(Template, Direction, FRunEncounterOffer());
            TestNotNull(TEXT("A room owns a local NPC stage."), Stage);
            if (!Stage) continue;
            const int32 Before = Completions;
            if (!TestTrue(TEXT("Confirmed selection begins arrival travel."), Route->StartTravel(Direction))) continue;
            TestFalse(TEXT("A second travel request cannot replace an active door arrival."), Route->StartTravel(Direction));
            float ApproachSeconds = 0.f;
            for (const FEncounterDungeonTravelSegment& Segment : Route->TravelSegments)
            {
                if (Segment.DoorIndex != INDEX_NONE) break;
                ApproachSeconds += Segment.Duration;
            }
            Route->Tick(ApproachSeconds + 0.3f);
            TestTrue(TEXT("The camera waits outside while the entrance opens."), Route->Camera->GetRelativeLocation().X < EncounterDungeonLayout::GetDoorCenter(Direction, Variant, false).X && Route->IsTraveling() && Completions == Before);
            Route->Tick(10.f);
            TestTrue(TEXT("A frame hitch completes arrival once at the configured NPC camera."), !Route->IsTraveling() && Route->Camera->GetComponentLocation().Equals(Stage->Camera->GetComponentLocation()) && Completions == Before + 1);
            if (!TestTrue(TEXT("Departure reuses the current room instead of resetting to the junction."), Route->StartExitTravel())) continue;
            Route->Tick(10.f);
            TestTrue(TEXT("Departure finishes beyond the rear door and preserves the NPC until the cache swap."), !Route->IsTraveling() && Route->GetPresentedStage() == Stage && Route->Camera->GetRelativeLocation().X > EncounterDungeonLayout::GetDoorCenter(Direction, Variant, true).X && Completions == Before + 2);
            const FVector LoopDoor = EncounterDungeonLayout::GetLoopDoorCenter(Direction, Variant);
            TestTrue(TEXT("Departure remains on the center line and stops in front of the closed loop door."), Route->Camera->GetRelativeLocation().Y == LoopDoor.Y && FMath::IsNearlyEqual(LoopDoor.X - Route->Camera->GetRelativeLocation().X, EncounterDungeonLayout::DoorCoverDistance));
            Route->Tick(1.f);
            TestEqual(TEXT("An idle frame cannot repeat a completed departure callback."), Completions, Before + 2);
            Route->FinishAtStage();
            TestTrue(TEXT("Continue can restore directly to arrival without replaying entry."), !Route->IsTraveling() && Route->Camera->GetComponentLocation().Equals(Stage->Camera->GetComponentLocation()));
            TestTrue(TEXT("The restored room can depart through its rear doorway."), Route->StartExitTravel());
            Controller->DungeonRoute = Route;
            Controller->bEncounterExitTransition = true;
            Controller->bEncounterPresentationTransition = true;
            const uint32 OldGeneration = Controller->PresentationGeneration;
            Controller->ResetEncounterPresentation();
            Controller->FadeEncounterExit(OldGeneration);
            Controller->FinishEncounterExit(OldGeneration);
            TestTrue(TEXT("Cancellation invalidates old exit callbacks and stops the cached route."), Controller->PresentationGeneration != OldGeneration && !Controller->bEncounterExitTransition && !Controller->bEncounterPresentationTransition && !Route->IsTraveling() && !Route->GetPresentedStage());
            TestTrue(TEXT("The next section opens its matching doorway without an NPC stage."), Route->StartLoopEntrance());
            const FVector EntranceDoor = Route->DoorCenters[EncounterDungeonLayout::LoopEntranceDoorIndex];
            TestTrue(TEXT("The incoming door begins at the same distance and facing as the outgoing closed door."), FMath::IsNearlyEqual(EntranceDoor.X - Route->Camera->GetRelativeLocation().X, EncounterDungeonLayout::DoorCoverDistance) && Route->Camera->GetRelativeRotation().IsNearlyZero());
            Route->Tick(0.3f);
            TestTrue(TEXT("Door opening finishes before the camera crosses into the new junction."), Route->IsTraveling() && Route->Camera->GetRelativeLocation().X < EntranceDoor.X);
            Route->Tick(10.f);
            TestTrue(TEXT("A frame hitch completes the incoming doorway once at the ordinary junction."), !Route->IsTraveling() && Route->Camera->GetRelativeLocation().Equals(FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight)) && Completions == Before + 3);
            TestTrue(TEXT("An incoming doorway can be cancelled and cannot continue from stale travel segments."), Route->StartLoopEntrance());
            Route->ResetAtJunction();
            Route->Tick(10.f);
            TestEqual(TEXT("Cancellation cannot replay an incoming door completion."), Completions, Before + 3);
            Route->SetPresentationVisible(false);
            TestFalse(TEXT("Prepared or hidden rooms hide their doors and seam covers too."), Route->LeftDoors->IsVisible() || Route->RightDoors->IsVisible() || Route->DoorSeals->IsVisible());
            Route->SetPresentationVisible(true);
            TestTrue(TEXT("PvE enters the chosen corridor without an NPC stage."), Route->StartCombatTravel(Direction));
            TestFalse(TEXT("PvE travel cannot be replaced while its doors are opening."), Route->StartCombatTravel((Direction + 1) % 3));
            Route->Tick(10.f);
            TestTrue(TEXT("PvE stops before the terminal door and completes exactly once."), !Route->IsTraveling() && !Route->GetPresentedStage() && Route->Camera->GetRelativeLocation().Y == LoopDoor.Y && FMath::IsNearlyEqual(LoopDoor.X - Route->Camera->GetRelativeLocation().X, EncounterDungeonLayout::DoorCoverDistance) && Completions == Before + 4);
            Controller->bCombatEntryTransition = true;
            Controller->bCombatTravelFinished = true;
            Controller->bEncounterPresentationTransition = true;
            const uint32 CombatGeneration = Controller->PresentationGeneration;
            Controller->ResetEncounterPresentation();
            Controller->TryRevealCombat(CombatGeneration);
            Controller->RevealCombat(CombatGeneration);
            TestTrue(TEXT("A cancelled PvE entry cannot reveal a stale arena or keep input blocked."), !Controller->bCombatEntryTransition && !Controller->bEncounterPresentationTransition && !Controller->bCombatTravelFinished);
        }
    }
    Route->OnTravelFinished.Clear();
    World->DestroyWorld(false);
    return true;
}

#endif
