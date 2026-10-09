#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Game/Run/RunDungeonPlan.h"
#include "Game/Run/RunEncounterPool.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace RunDungeonPlanTests
{
    bool Initialize(URunSaveGame& Save, bool bTarget, FText& Error)
    {
        Save.Identity.RunId = FGuid(0x12345678, 0x23456789, 0x34567890, 0x45678901);
        Save.Nodes = bTarget ? RunProgressRules::GetTargetRoute().Nodes : RunProgressRules::GetPrototypeRoute().Nodes;
        Save.Phase = ERunPhase::EncounterChoice;
        Save.EncounterProgress.SchemaVersion = bTarget ? 2 : 1;
        Save.EncounterProgress.AfterCompletedNodeCount = bTarget ? 0 : 1;
        Save.EncounterProgress.VisitIndex = 0;
        if (!bTarget) return GetDefault<URunEncounterPoolDataAsset>()->BuildFixedOffers(Save.EncounterProgress.Offers, Error);
        Save.TargetRun.SchemaVersion = 1;
        Save.TargetRun.EncounterSelectionVersion = 1;
        Save.TargetRun.EncounterSeed = 317;
        const FGameplayTag ServiceTags[] = {FRunEncounterOffer::GetRecoveryTag(), FRunEncounterOffer::GetConsumableShopTag(), FRunEncounterOffer::GetRevivalTag()};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ServiceTags); ++Index)
        {
            FRunEncounterOffer& Offer = Save.TargetRun.EncounterPool.AddDefaulted_GetRef();
            Offer.EncounterId = FName(*FString::Printf(TEXT("FrozenService_%d"), Index));
            Offer.DisplayName = FText::FromString(FString::Printf(TEXT("Frozen service %d"), Index));
            Offer.EncounterTag = ServiceTags[Index];
            Offer.SelectionGroupTag = ServiceTags[Index];
            Offer.GroupWeight = static_cast<float>(Index + 1);
            Offer.VariantWeight = 1.f;
        }
        return UTargetRunDefinitionDataAsset::BuildOffers(Save.TargetRun, 0, 0, Save.EncounterProgress.Offers);
    }

    bool SamePlan(const FRunDungeonState& Left, const FRunDungeonState& Right)
    {
        return FRunDungeonState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }

    bool SameOffers(const TArray<FRunEncounterOffer>& Left, const TArray<FRunEncounterOffer>& Right)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index) if (!RunEncounterPool::IsSameOffer(Left[Index], Right[Index])) return false;
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunDungeonPlanFrozenTest, "ProjectA.Run.Dungeon.FrozenBoundariesAndRandomIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunDungeonPlanFrozenTest::RunTest(const FString& Parameters)
{
    FText Error;
    for (bool bTarget : {false, true})
    {
        TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
        if (!TestTrue(TEXT("A pure fixture uses the original target or prototype offer policy."), RunDungeonPlanTests::Initialize(*Save.Get(), bTarget, Error))) return false;
        FRunDungeonState Repeated;
        if (!TestTrue(TEXT("A complete logical dungeon builds without world actors or external data."), RunDungeonPlan::Build(*Save.Get(), Save->DungeonState, Error) && RunDungeonPlan::Build(*Save.Get(), Repeated, Error) && RunDungeonPlan::Validate(*Save.Get(), Error))) return false;
        TestTrue(TEXT("Repeated builds reproduce every seed, variant, boundary and candidate ID."), RunDungeonPlanTests::SamePlan(Save->DungeonState, Repeated));
        TestEqual(TEXT("Targets freeze sixty visits; ten-battle prototypes retain their original nine shops."), Save->DungeonState.Visits.Num(), bTarget ? 60 : 9);
        for (int32 Index = 0; Index < Save->DungeonState.Visits.Num(); ++Index)
        {
            const FRunDungeonVisit& Visit = Save->DungeonState.Visits[Index];
            TestTrue(TEXT("Visit order is the existing progression boundary with one of eight valid layouts."), Visit.AfterCompletedNodeCount == (bTarget ? Index / 3 : Index + 1) && Visit.VisitIndex == (bTarget ? Index % 3 : 0) && Visit.LayoutVariant >= 0 && Visit.LayoutVariant < 8 && Visit.OfferIds.Num() == 3);
            FRunEncounterProgress Progress;
            Progress.AfterCompletedNodeCount = Visit.AfterCompletedNodeCount;
            Progress.VisitIndex = Visit.VisitIndex;
            TestEqual(TEXT("Boundary lookup resolves the exact frozen visit."), RunDungeonPlan::FindVisit(Save->DungeonState, Progress), Index);
            TArray<FRunEncounterOffer> Expected = Save->EncounterProgress.Offers;
            if (bTarget && !TestTrue(TEXT("The original weighted selection remains independently reproducible."), UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, Visit.AfterCompletedNodeCount, Visit.VisitIndex, Expected))) return false;
            TArray<FRunEncounterOffer> Resolved;
            if (!TestTrue(TEXT("Frozen offer consumption resolves complete original offer values in order."), RunDungeonPlan::ResolveOffers(*Save.Get(), Visit.AfterCompletedNodeCount, Visit.VisitIndex, Resolved, Error) && RunDungeonPlanTests::SameOffers(Expected, Resolved))) return false;
            for (int32 Direction = 0; Direction < 3; ++Direction) TestEqual(TEXT("Each frozen direction keeps its original server offer ID."), Visit.OfferIds[Direction], Resolved[Direction].EncounterId);
        }
        FMath::RandInit(198407);
        const int32 ExpectedBefore = FMath::Rand();
        const int32 ExpectedAfter = FMath::Rand();
        FMath::RandInit(198407);
        const int32 ActualBefore = FMath::Rand();
        const bool bBuiltWithoutGlobalRandom = RunDungeonPlan::Build(*Save.Get(), Repeated, Error);
        const int32 ActualAfter = FMath::Rand();
        TestTrue(TEXT("Dungeon construction consumes no global gameplay random draws."), bBuiltWithoutGlobalRandom && ActualBefore == ExpectedBefore && ActualAfter == ExpectedAfter);
        if (bTarget)
        {
            const FRunDungeonState Original = Save->DungeonState;
            Save->TargetRun.EncounterSeed += 1;
            if (!TestTrue(TEXT("Changing the isolated encounter seed can rebuild offers without changing the geometry domain."), RunDungeonPlan::Build(*Save.Get(), Repeated, Error))) return false;
            TestEqual(TEXT("Geometry seed is independent of the encounter candidate seed."), Repeated.Seed, Original.Seed);
            for (int32 Index = 0; Index < Original.Visits.Num(); ++Index) TestEqual(TEXT("Every geometry variant is independent of encounter random draws."), Repeated.Visits[Index].LayoutVariant, Original.Visits[Index].LayoutVariant);
            TArray<FRunEncounterOffer> Frozen;
            if (!TestTrue(TEXT("Runtime consumption follows frozen IDs even if the selection seed is subsequently different."), RunDungeonPlan::ResolveOffers(*Save.Get(), 0, 0, Frozen, Error))) return false;
            for (int32 Index = 0; Index < Frozen.Num(); ++Index) TestEqual(TEXT("Consumption does not silently replace frozen offer order with a fresh roll."), Frozen[Index].EncounterId, Original.Visits[0].OfferIds[Index]);
            Save->Identity.RunId.A ^= 1u;
            if (!TestTrue(TEXT("A distinct Run identity receives its own geometry seed."), RunDungeonPlan::Build(*Save.Get(), Repeated, Error))) return false;
            TestTrue(TEXT("Run identity separates the geometry random domain."), Repeated.Seed != Original.Seed);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunDungeonPlanCorruptionTest, "ProjectA.Run.Dungeon.LegacyCorruptionAndAtomicFailures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunDungeonPlanCorruptionTest::RunTest(const FString& Parameters)
{
    FText Error;
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    if (!RunDungeonPlanTests::Initialize(*Save.Get(), true, Error) || !RunDungeonPlan::Build(*Save.Get(), Save->DungeonState, Error)) return false;
    const FRunDungeonState Valid = Save->DungeonState;
    const auto RejectPlan = [this, &Save, &Error](const TCHAR* Message, const FRunDungeonState& BadPlan)
    {
        Save->DungeonState = BadPlan;
        TestFalse(Message, RunDungeonPlan::Validate(*Save.Get(), Error));
        TestTrue(TEXT("Invalid plan validation explains the failure and never repairs the saved data."), !Error.IsEmpty() && RunDungeonPlanTests::SamePlan(Save->DungeonState, BadPlan));
    };
    FRunDungeonState Bad = Valid;
    Bad.SchemaVersion = 2;
    RejectPlan(TEXT("Unknown layout versions are rejected."), Bad);
    Bad = Valid;
    Bad.Seed ^= 1;
    RejectPlan(TEXT("A tampered geometry seed is rejected."), Bad);
    Bad = Valid;
    Bad.Visits.Pop();
    RejectPlan(TEXT("A truncated future visit plan is rejected."), Bad);
    Bad = Valid;
    Bad.Visits.Add(Valid.Visits.Last());
    RejectPlan(TEXT("An oversized future visit plan is rejected."), Bad);
    Bad = Valid;
    Bad.Visits[0].LayoutVariant = 8;
    RejectPlan(TEXT("An unsupported layout variant is rejected."), Bad);
    Bad = Valid;
    Bad.Visits[0].LayoutVariant = (Bad.Visits[0].LayoutVariant + 1) % 8;
    RejectPlan(TEXT("A valid-range variant still must match the frozen seed."), Bad);
    Bad = Valid;
    Bad.Visits[0].VisitIndex = 1;
    RejectPlan(TEXT("Duplicated or shifted progression boundaries are rejected."), Bad);
    Bad = Valid;
    Bad.Visits[0].OfferIds.Swap(0, 1);
    RejectPlan(TEXT("Candidate direction order cannot change after freezing."), Bad);
    Bad = Valid;
    Bad.Visits[0].OfferIds[0] = Bad.Visits[0].OfferIds[1];
    RejectPlan(TEXT("Duplicate candidate IDs are rejected."), Bad);
    Bad = Valid;
    Bad.Visits[0].OfferIds[0] = TEXT("MissingFrozenOffer");
    RejectPlan(TEXT("A plan cannot reference an unknown pool entry."), Bad);
    TArray<FRunEncounterOffer> Output = Save->EncounterProgress.Offers;
    const TArray<FRunEncounterOffer> OriginalOutput = Output;
    TestFalse(TEXT("Missing frozen offers reject runtime resolution."), RunDungeonPlan::ResolveOffers(*Save.Get(), 0, 0, Output, Error));
    TestTrue(TEXT("Failed resolution preserves the complete previous output and reports its reason."), !Error.IsEmpty() && RunDungeonPlanTests::SameOffers(Output, OriginalOutput));
    Save->DungeonState = Valid;
    TestFalse(TEXT("Missing visit boundaries reject resolution."), RunDungeonPlan::ResolveOffers(*Save.Get(), 20, 0, Output, Error));
    TestTrue(TEXT("A missing visit also preserves the previous candidates."), !Error.IsEmpty() && RunDungeonPlanTests::SameOffers(Output, OriginalOutput));
    Save->EncounterProgress.Offers.Swap(0, 1);
    TestFalse(TEXT("Current visible offers must agree with the frozen direction order."), RunDungeonPlan::Validate(*Save.Get(), Error));
    Save->EncounterProgress.Offers.Swap(0, 1);
    Save->EncounterProgress.VisitIndex = 99;
    TestEqual(TEXT("Out-of-range visits never bind to an unrelated existing layout."), RunDungeonPlan::FindVisit(Valid, Save->EncounterProgress), INDEX_NONE);
    TestFalse(TEXT("A save outside the plan cannot validate."), RunDungeonPlan::Validate(*Save.Get(), Error));
    Save->EncounterProgress.VisitIndex = 0;
    Save->Identity.RunId.Invalidate();
    FRunDungeonState OutputPlan = Valid;
    TestFalse(TEXT("A missing Run identity rejects fresh plan construction."), RunDungeonPlan::Build(*Save.Get(), OutputPlan, Error));
    TestTrue(TEXT("A failed plan build preserves its previous output and reports the failure."), !Error.IsEmpty() && RunDungeonPlanTests::SamePlan(OutputPlan, Valid));
    Save->DungeonState = FRunDungeonState();
    TestTrue(TEXT("Historical empty layout data validates without creating a new plan or inferring a Run ID."), RunDungeonPlan::Validate(*Save.Get(), Error) && Save->DungeonState.Visits.IsEmpty());
    TestEqual(TEXT("Historical empty data has no frozen visit to project."), RunDungeonPlan::FindVisit(Save->DungeonState, Save->EncounterProgress), INDEX_NONE);
    Save->DungeonState.Seed = 1;
    TestFalse(TEXT("Version zero cannot smuggle a geometry seed."), RunDungeonPlan::Validate(*Save.Get(), Error));
    Save->DungeonState.Seed = 0;
    Save->DungeonState.Visits.Add(Valid.Visits[0]);
    TestFalse(TEXT("Version zero cannot smuggle a frozen visit."), RunDungeonPlan::Validate(*Save.Get(), Error));
    Save->DungeonState = FRunDungeonState();
    TestTrue(TEXT("Historical targets keep their original offer selector."), RunDungeonPlan::ResolveOffers(*Save.Get(), 0, 0, Output, Error) && RunDungeonPlanTests::SameOffers(Output, OriginalOutput));
    TestFalse(TEXT("Legacy resolution also rejects invalid boundaries."), RunDungeonPlan::ResolveOffers(*Save.Get(), -1, 0, Output, Error));
    TestTrue(TEXT("Failed legacy resolution preserves its candidates."), RunDungeonPlanTests::SameOffers(Output, OriginalOutput));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunDungeonPlanSerializationTest, "ProjectA.Run.Dungeon.FrozenPlanSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunDungeonPlanSerializationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    FText Error;
    if (!RunDungeonPlanTests::Initialize(*Save.Get(), true, Error) || !RunDungeonPlan::Build(*Save.Get(), Save->DungeonState, Error)) return false;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("The complete seeded plan serializes through the real SaveGame format."), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The real SaveGame type restores the frozen plan."), Restored.Get())) return false;
    TestTrue(TEXT("SaveGame preserves all future visits and validates solely against its frozen data."), RunDungeonPlanTests::SamePlan(Save->DungeonState, Restored->DungeonState) && RunDungeonPlan::Validate(*Restored.Get(), Error));
    Save->DungeonState.Visits[0].OfferIds[0] = TEXT("ChangedOriginalOnly");
    TestTrue(TEXT("Restored arrays remain independent of the original SaveGame."), Restored->DungeonState.Visits[0].OfferIds[0] != Save->DungeonState.Visits[0].OfferIds[0]);
    Restored->EncounterProgress.AfterCompletedNodeCount = 19;
    Restored->EncounterProgress.VisitIndex = 2;
    TestTrue(TEXT("The final frozen visit resolves without rebuilding or loading CSV."), RunDungeonPlan::ResolveOffers(*Restored.Get(), 19, 2, Restored->EncounterProgress.Offers, Error) && RunDungeonPlan::Validate(*Restored.Get(), Error));
    return true;
}

#endif
