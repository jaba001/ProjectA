#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Game/Run/RunEncounterPool.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    FString MakeEncounterPoolCsv(int32 TagVariantCount = 3)
    {
        FString Csv = TEXT("인카운터 ID,게임 내 이름,상점 종류,속성,분류 태그,판매 대상,활성 여부,그룹 태그,그룹 가중치,변형 가중치,상품 필수 태그,상품 제외 태그,진열 정책,구현 상태,확인 사항\n");
        Csv += TEXT("Basic,기본 상점,기본,,Encounter.Shop.Item.Basic,전체 아이템,1,Encounter.Shop.Item.Basic,40,1,Item.Weapon,,기본5,시험,기본 그룹\n");
        Csv += TEXT("White,흰색 상점,등급별,,Encounter.Shop.Item.Rarity,흰색,1,Encounter.Shop.Item.Rarity,20,50,Item.Weapon|Item.Rarity.White,,최대5,시험,등급 그룹\n");
        Csv += TEXT("Blue,파란색 상점,등급별,,Encounter.Shop.Item.Rarity,파란색,1,Encounter.Shop.Item.Rarity,20,15,Item.Weapon|Item.Rarity.Blue,,최대5,시험,등급 그룹\n");
        for (int32 Index = 0; Index < TagVariantCount; ++Index) Csv += FString::Printf(TEXT("Tag_%d,태그 상점 %d,태그별,,Encounter.Shop.Item.Tag,검,1,Encounter.Shop.Item.Tag,20,1,Item.Weapon|Item.Weapon.Sword,Item.Rarity.Orange,최대5,시험,태그 그룹\n"), Index, Index);
        Csv += TEXT("Recovery,회복소,회복,,Encounter.Service.Recovery,체력 회복,1,Encounter.Service.Recovery,10,1,,,해당없음,시험,회복 그룹\n");
        Csv += TEXT("Consumable,소모품상점,소모품,,Encounter.Shop.Consumable,회복 소모품,1,Encounter.Shop.Consumable,7,1,,,해당없음,시험,소모품 그룹\n");
        Csv += TEXT("Revival,부활소,부활,,Encounter.Service.Revival,사망 아군,1,Encounter.Service.Revival,3,1,,,해당없음,시험,부활 그룹\n");
        Csv += TEXT("Retired,이전 상점,스킬,,Encounter.Shop.Skill,과거 상품,0,,,,,,보존,과거 기록,활성 대상 아님\n");
        return Csv;
    }

    FRunTargetState MakeUnconfiguredTarget()
    {
        FRunTargetState State;
        State.SchemaVersion = 1;
        State.EncounterPool = GetDefault<UTargetRunDefinitionDataAsset>()->EncounterPool;
        State.EncounterQuery = GetDefault<UTargetRunDefinitionDataAsset>()->EncounterQuery;
        State.Groups = GetDefault<UTargetRunDefinitionDataAsset>()->Groups;
        return State;
    }

    bool SameEncounterOffers(const TArray<FRunEncounterOffer>& Left, const TArray<FRunEncounterOffer>& Right)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index) if (!RunEncounterPool::IsSameOffer(Left[Index], Right[Index])) return false;
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEncounterPoolCsvTest, "ProjectA.Run.EncounterPool.StrictCsvAndFrozenInitialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEncounterPoolCsvTest::RunTest(const FString& Parameters)
{
    const FString Csv = MakeEncounterPoolCsv();
    FRunTargetState State = MakeUnconfiguredTarget();
    const FRunTargetState Original = State;
    FText Error;
    if (!TestTrue(TEXT("The authored group variant and item filter CSV freezes into a new Run"), RunEncounterPool::LoadFromString(Csv, 731, State, Error))) return false;
    TestTrue(TEXT("Only active rows enter the pool and the original Run seed is frozen"), State.EncounterSelectionVersion == 1 && State.EncounterSeed == 731 && State.EncounterPool.Num() == 9 && !State.EncounterPool.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.EncounterId == TEXT("Retired"); }));
    TestTrue(TEXT("CSV loading preserves existing battle group and snapshot values"), State.Groups.Num() == Original.Groups.Num() && FTargetRunGroup::StaticStruct()->CompareScriptStruct(&State.Groups[0], &Original.Groups[0], 0));
    const FRunEncounterOffer* TagShop = State.EncounterPool.FindByPredicate([](const FRunEncounterOffer& Offer) { return Offer.EncounterId == TEXT("Tag_0"); });
    if (!TestNotNull(TEXT("The tagged shop is present"), TagShop)) return false;
    FGameplayTagContainer SwordTags;
    SwordTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Sword")));
    SwordTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.White")));
    TestTrue(TEXT("CSV required tags execute as a gameplay tag query"), TagShop->ItemQuery.Matches(SwordTags));
    SwordTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Orange")));
    TestFalse(TEXT("CSV excluded tags execute in the same item query"), TagShop->ItemQuery.Matches(SwordTags));
    const FRunTargetState Frozen = State;
    TestFalse(TEXT("An already frozen Run cannot reload current CSV or change its seed"), RunEncounterPool::LoadFromString(Csv, 999, State, Error));
    TestTrue(TEXT("Rejected reinitialization preserves the complete original Run"), FRunTargetState::StaticStruct()->CompareScriptStruct(&State, &Frozen, 0));

    const TArray<FString> InvalidCsvs =
    {
        Csv.Replace(TEXT("활성 여부"), TEXT("다른 열")),
        Csv.Replace(TEXT("전체 아이템,1"), TEXT("전체 아이템,true")),
        Csv.Replace(TEXT("Item.Weapon.Sword"), TEXT("Item.UnregisteredWeapon")),
        Csv.Replace(TEXT("Item.Weapon|Item.Weapon.Sword"), TEXT("Item.Weapon||Item.Weapon.Sword")),
        Csv.Replace(TEXT("Item.Weapon|Item.Rarity.White"), TEXT("")),
        Csv.Replace(TEXT("Encounter.Shop.Item.Basic,40,1"), TEXT("Encounter.Shop.Item.Basic,-40,1")),
        Csv.Replace(TEXT("Encounter.Shop.Item.Basic,40,1"), TEXT("Encounter.Shop.Item.Basic,NaN,1")),
        Csv.Replace(TEXT("Encounter.Shop.Item.Basic,40,1"), TEXT("Encounter.Shop.Item.Basic,4e1,1")),
        Csv.Replace(TEXT("Encounter.Shop.Item.Rarity,20,15"), TEXT("Encounter.Shop.Item.Rarity,21,15")),
        Csv.Replace(TEXT("기본5,시험"), TEXT("최대5,시험")),
        Csv.Replace(TEXT("White,흰색"), TEXT("Basic,흰색")),
        Csv.Replace(TEXT("Retired,이전 상점"), TEXT("Retired/Path,이전 상점")),
        Csv.Replace(TEXT("과거 기록,활성 대상 아님"), TEXT("과거 기록,\"두 줄\n기록\""))
    };
    for (const FString& InvalidCsv : InvalidCsvs)
    {
        FRunTargetState Rejected = Original;
        TestFalse(TEXT("Malformed active or archived CSV rows reject the complete initialization"), RunEncounterPool::LoadFromString(InvalidCsv, 52, Rejected, Error));
        TestTrue(TEXT("Rejected CSV preserves the previous pool seed snapshots and progression"), FRunTargetState::StaticStruct()->CompareScriptStruct(&Rejected, &Original, 0));
        TestFalse(TEXT("Rejected CSV explains its failure"), Error.IsEmpty());
    }
    FRunTargetState Current = MakeUnconfiguredTarget();
    TestTrue(TEXT("The repository CSV exposes 13 active grouped encounters with supported stock"), RunEncounterPool::Load(Current, Error) && Current.EncounterPool.Num() == 13 && Current.EncounterSelectionVersion == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEncounterPoolSelectionTest, "ProjectA.Run.EncounterPool.HierarchicalWeightsAndLegacyRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEncounterPoolSelectionTest::RunTest(const FString& Parameters)
{
    FRunTargetState Small = MakeUnconfiguredTarget();
    FRunTargetState Large = MakeUnconfiguredTarget();
    FText Error;
    if (!TestTrue(TEXT("Small and large tag groups both freeze valid policies"), RunEncounterPool::LoadFromString(MakeEncounterPoolCsv(3), 731, Small, Error) && RunEncounterPool::LoadFromString(MakeEncounterPoolCsv(19), 731, Large, Error))) return false;
    int32 BasicFirst = 0;
    int32 TagFirst = 0;
    for (int32 Seed = 0; Seed < 4096; ++Seed)
    {
        Small.EncounterSeed = Seed;
        Large.EncounterSeed = Seed;
        TArray<FRunEncounterOffer> SmallOffers;
        TArray<FRunEncounterOffer> LargeOffers;
        if (!RunEncounterPool::Select(Small, 0, 0, SmallOffers, Error) || !RunEncounterPool::Select(Large, 0, 0, LargeOffers, Error)) return TestTrue(TEXT("Every seeded draw has three eligible distinct offers"), false);
        TestEqual(TEXT("More variants do not multiply a group's first-slot probability"), SmallOffers[0].SelectionGroupTag, LargeOffers[0].SelectionGroupTag);
        BasicFirst += SmallOffers[0].SelectionGroupTag == FRunEncounterOffer::GetBasicItemShopTag() ? 1 : 0;
        TagFirst += SmallOffers[0].SelectionGroupTag == FRunEncounterOffer::GetTagItemShopTag() ? 1 : 0;
        TSet<FName> Ids;
        for (const FRunEncounterOffer& Offer : LargeOffers) Ids.Add(Offer.EncounterId);
        if (!TestEqual(TEXT("Each visible set excludes duplicate encounter IDs"), Ids.Num(), 3)) return false;
    }
    TestTrue(TEXT("The first slot follows the 40-percent basic group weight within sampling tolerance"), FMath::Abs(BasicFirst - 1638) < 150);
    TestTrue(TEXT("The first slot follows the 20-percent tag group weight despite 19 variants"), FMath::Abs(TagFirst - 819) < 130);

    FRunTargetState Filtered = Small;
    Filtered.EncounterQuery = FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetTagItemShopTag());
    TArray<FRunEncounterOffer> Offers;
    if (!TestTrue(TEXT("The encounter tag query filters the actual grouped draw"), RunEncounterPool::Select(Filtered, 2, 1, Offers, Error))) return false;
    for (const FRunEncounterOffer& Offer : Offers) TestEqual(TEXT("Only query-compatible groups can be selected"), Offer.SelectionGroupTag, FRunEncounterOffer::GetTagItemShopTag());
    const TArray<FRunEncounterOffer> Previous = Offers;
    Filtered.EncounterPool[0].VariantWeight = -1.0f;
    TestFalse(TEXT("Invalid saved weights reject selection before publishing partial offers"), RunEncounterPool::Select(Filtered, 2, 1, Offers, Error));
    TestTrue(TEXT("Rejected selection retains previous displayed offers"), SameEncounterOffers(Offers, Previous));

    FRunTargetState Exhausted = Small;
    for (FRunEncounterOffer& Offer : Exhausted.EncounterPool) if (Offer.EncounterId != TEXT("Basic") && Offer.EncounterId != TEXT("White") && Offer.EncounterId != TEXT("Blue")) Offer.VariantWeight = 0.0f;
    if (!TestTrue(TEXT("Exhausted groups are removed while remaining positive variants fill the set"), RunEncounterPool::Select(Exhausted, 3, 0, Offers, Error))) return false;
    TSet<FName> ExhaustedIds;
    for (const FRunEncounterOffer& Offer : Offers) ExhaustedIds.Add(Offer.EncounterId);
    TestTrue(TEXT("Zero-weight variants are never fallback candidates"), ExhaustedIds.Num() == 3 && ExhaustedIds.Contains(TEXT("Basic")) && ExhaustedIds.Contains(TEXT("White")) && ExhaustedIds.Contains(TEXT("Blue")));

    FRunTargetState Legacy = MakeUnconfiguredTarget();
    for (int32 Combat = 0; Combat < 20; ++Combat)
    {
        for (int32 Visit = 0; Visit < 3; ++Visit)
        {
            if (!TestTrue(TEXT("Missing new policy metadata keeps the original rotation available"), UTargetRunDefinitionDataAsset::BuildOffers(Legacy, Combat, Visit, Offers))) return false;
            for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Every old boundary retains its original candidate order"), Offers[Index].EncounterId, Legacy.EncounterPool[(Combat * 3 + Visit + Index) % Legacy.EncounterPool.Num()].EncounterId);
        }
    }
    Legacy.EncounterSeed = 7;
    TestFalse(TEXT("Weighted metadata cannot silently change a version-zero saved policy"), UTargetRunDefinitionDataAsset::BuildOffers(Legacy, 0, 0, Offers));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEncounterPoolSerializationTest, "ProjectA.Run.EncounterPool.FrozenSeedQueriesAndDisplayText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEncounterPoolSerializationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->TargetRun = MakeUnconfiguredTarget();
    FText Error;
    if (!TestTrue(TEXT("The CSV encounter policy initializes for saved progression"), RunEncounterPool::LoadFromString(MakeEncounterPoolCsv(), 962, Save->TargetRun, Error))) return false;
    if (!TestTrue(TEXT("A visit records its original selected offer set"), UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, 4, 2, Save->EncounterProgress.Offers))) return false;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The Run seed group weights item queries and displayed copies serialize"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The frozen encounter save deserializes"), Restored.Get())) return false;
    if (!TestTrue(TEXT("The restored grouped pool validates without consulting the current CSV"), RunEncounterPool::Validate(Restored->TargetRun, Error))) return false;
    TArray<FRunEncounterOffer> Replayed;
    if (!TestTrue(TEXT("The same saved visit replays the same offer set"), UTargetRunDefinitionDataAsset::BuildOffers(Restored->TargetRun, 4, 2, Replayed))) return false;
    TestTrue(TEXT("CSV text keys cannot invalidate an otherwise identical restored offer"), SameEncounterOffers(Replayed, Restored->EncounterProgress.Offers) && SameEncounterOffers(Replayed, Save->EncounterProgress.Offers));
    TestEqual(TEXT("The initial Run seed survives serialization"), Restored->TargetRun.EncounterSeed, 962);
    FRunEncounterOffer Changed = Replayed[0];
    Changed.DisplayName = FText::FromString(TEXT("변조된 상점 이름"));
    TestFalse(TEXT("Lexical text comparison still rejects changed display names"), RunEncounterPool::IsSameOffer(Changed, Replayed[0]));
    Changed = Replayed[0];
    Changed.ItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Bow")));
    TestFalse(TEXT("Frozen offer comparison still rejects changed item conditions"), RunEncounterPool::IsSameOffer(Changed, Replayed[0]));
    return true;
}

#endif
