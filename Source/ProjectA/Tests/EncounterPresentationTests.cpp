#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Encounter/EncounterPrototypeStage.h"

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

#endif
