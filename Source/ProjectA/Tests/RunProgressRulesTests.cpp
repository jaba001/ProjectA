#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRouteRulesTest, "ProjectA.Run.Progress.RouteDefinitionBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRouteRulesTest::RunTest(const FString& Parameters)
{
    const FRunRouteDefinition& Prototype = RunProgressRules::GetPrototypeRoute();
    if (!TestEqual(TEXT("The production prototype retains its two combat nodes"), Prototype.Nodes.Num(), 2)) return false;
    TestTrue(TEXT("The prototype keeps the existing node and encounter identifiers"), Prototype.Nodes[0].NodeId == TEXT("Combat_01") && Prototype.Nodes[1].NodeId == TEXT("Combat_02") && Prototype.Nodes[0].EncounterId == TEXT("DefaultEncounter") && Prototype.Nodes[1].EncounterId == TEXT("DefaultEncounter"));
    FRunRouteDefinition Alternate = Prototype;
    FRunNodeDefinition Extra;
    Extra.NodeId = TEXT("Fixture_Final");
    Extra.EncounterId = TEXT("Fixture_FinalEncounter");
    Alternate.Nodes.Add(Extra);
    Alternate.Nodes[1].EncounterId = TEXT("Fixture_MiddleEncounter");
    Alternate.EncounterAfterCompletedNodes = 2;
    TArray<FRunNodeDefinition> Nodes = Alternate.Nodes;
    TArray<FName> Completed;
    FRunProgressView Progress{Nodes, Completed, NAME_None, NAME_None, ERunPhase::Map, ECombatResult::None};
    TestTrue(TEXT("A different definition controls node count and encounter identity without changing production content"), RunProgressRules::ValidateNodes(Alternate, Progress));
    TestFalse(TEXT("Existing prototype saves cannot silently acquire the alternate route"), RunProgressRules::ValidateNodes(Prototype, Progress));
    TestTrue(TEXT("The alternate route starts from its empty map boundary"), RunProgressRules::ValidatePhase(Progress, true, true));
    FRunEncounterProgress Encounter;
    Encounter.SchemaVersion = 1;
    FText Error;
    if (!GetDefault<URunEncounterPoolDataAsset>()->BuildFixedOffers(Encounter.Offers, Error)) return false;
    Completed.Add(Nodes[0].NodeId);
    Progress.CurrentNode = Nodes[0].NodeId;
    Progress.CurrentEncounter = Nodes[0].EncounterId;
    Progress.Phase = ERunPhase::Result;
    Progress.Result = ECombatResult::Victory;
    TestTrue(TEXT("A result before the configured encounter boundary remains valid"), RunProgressRules::ValidatePhase(Progress, true, true) && RunProgressRules::ValidateEncounterProgress(Alternate, Progress, Encounter));
    TestTrue(TEXT("The first victory does not invent an early shop on the alternate route"), RunProgressRules::GetContinuationPhase(Alternate, Completed.Num(), Encounter) == ERunPhase::Map);
    Completed.Add(Nodes[1].NodeId);
    Progress.CurrentNode = Nodes[1].NodeId;
    Progress.CurrentEncounter = Nodes[1].EncounterId;
    TestTrue(TEXT("Result validation uses the completed node's actual encounter identifier"), RunProgressRules::ValidatePhase(Progress, true, true));
    Progress.CurrentEncounter = TEXT("DefaultEncounter");
    TestFalse(TEXT("A mismatched encounter cannot masquerade as the current result"), RunProgressRules::ValidatePhase(Progress, true, true));
    Progress.CurrentEncounter = NAME_None;
    Progress.Phase = RunProgressRules::GetContinuationPhase(Alternate, Completed.Num(), Encounter);
    TestTrue(TEXT("The configured second victory opens encounter choices"), Progress.Phase == ERunPhase::EncounterChoice && RunProgressRules::ValidateEncounterProgress(Alternate, Progress, Encounter));
    Encounter.SelectedEncounterId = Encounter.Offers[0].EncounterId;
    Progress.Phase = ERunPhase::Shop;
    TestTrue(TEXT("The chosen encounter is valid at its configured boundary"), RunProgressRules::ValidateEncounterProgress(Alternate, Progress, Encounter));
    Encounter.bCompleted = true;
    Progress.Phase = ERunPhase::Map;
    TestTrue(TEXT("Completed encounters permit the following map"), RunProgressRules::ValidatePhase(Progress, true, true) && RunProgressRules::ValidateEncounterProgress(Alternate, Progress, Encounter));
    Completed.Add(Nodes.Last().NodeId);
    Progress.CurrentNode = Nodes.Last().NodeId;
    Progress.Phase = RunProgressRules::GetContinuationPhase(Alternate, Completed.Num(), Encounter);
    TestTrue(TEXT("Completion follows the supplied route length"), Progress.Phase == ERunPhase::Complete && RunProgressRules::ValidatePhase(Progress, true, true));
    Completed[1] = TEXT("SkippedNode");
    TestFalse(TEXT("Progress must still be the exact prefix of the supplied route"), RunProgressRules::ValidateNodes(Alternate, Progress));
    TestEqual(TEXT("The alternate fixture never changes production route content"), RunProgressRules::GetPrototypeRoute().Nodes.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCandidatePublicationTest, "ProjectA.Run.Persistence.CandidatePublicationPreservesAllFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunCandidatePublicationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<URunStateSubsystem> Run(NewObject<URunStateSubsystem>(Instance.Get()));
    const FString Slot = TEXT("RunCandidate_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT { Run->OnRunStateChanged.Clear(); UGameplayStatics::DeleteGameInSlot(Slot, 0); };
    Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestNotNull(TEXT("Transaction coverage uses the real party catalog"), Run->PartyDefinition.Get())) return false;
    Run->EnableCheckpointSaving(Slot);
    FRunPartyMember Member;
    Member.SlotIndex = 0;
    Member.ClassId = TEXT("Archer");
    Member.CharacterName = FText::FromString(TEXT("Candidate Archer"));
    Member.bCreated = Member.bPlayerControlled = true;
    FText Error;
    if (!Run->InitializeRun({Member}, Error) || !Run->GetSaveError().IsEmpty()) return false;
    const auto CaptureRuntime = [&Run]()
    {
        TStrongObjectPtr<URunSaveGame> Save(Run->CreateSaveData());
        TArray<uint8> Bytes;
        UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes);
        return Bytes;
    };
    const auto ReadDisk = [&Slot]()
    {
        TArray<uint8> Bytes;
        UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
        return Bytes;
    };
    int32 Events = 0;
    bool bObservedDurableState = false;
    Run->OnRunStateChanged.AddLambda([&]()
    {
        ++Events;
        bObservedDurableState = CaptureRuntime() == ReadDisk();
    });
    const auto VerifyTransaction = [&](const TCHAR* Label, TFunctionRef<bool()> Action)
    {
        const TArray<uint8> BeforeRuntime = CaptureRuntime();
        const TArray<uint8> BeforeDisk = ReadDisk();
        const int32 BeforeEvents = Events;
        FRunCheckpointStorage::FailNextWriteForTesting();
        if (!TestFalse(FString(Label) + TEXT(" rejects failed persistence"), Action())) return false;
        TestTrue(FString(Label) + TEXT(" preserves every serialized runtime value and all durable bytes"), CaptureRuntime() == BeforeRuntime && ReadDisk() == BeforeDisk);
        TestTrue(FString(Label) + TEXT(" keeps saving enabled and publishes no failed state"), Run->IsCheckpointSavingEnabled() && Events == BeforeEvents && !Run->GetSaveError().IsEmpty());
        if (!TestTrue(FString(Label) + TEXT(" retries successfully after storage recovers"), Action())) return false;
        TestTrue(FString(Label) + TEXT(" publishes exactly once after disk and memory agree"), Events == BeforeEvents + 1 && bObservedDurableState && Run->GetSaveError().IsEmpty());
        return true;
    };
    if (!Run->BeginEncounter(TEXT("Combat_01"))) return false;
    if (!VerifyTransaction(TEXT("Preparation cancellation"), [&]() { return Run->AbortEncounter(); })) return false;
    if (!Run->BeginEncounter(TEXT("Combat_01")) || !Run->MarkCombatStarted()) return false;
    Run->UpdatePartyMemberHP(0, 1.0f);
    if (!VerifyTransaction(TEXT("Victory publication"), [&]() { return Run->CompleteEncounter(ECombatResult::Victory); })) return false;
    const FRunAccountId Account = Run->GetPartyMembers()[0].OwnerAccountId;
    const FGuid CharacterId = Run->GetPartyMembers()[0].CharacterId;
    if (!VerifyTransaction(TEXT("Reward claim"), [&]() { return Run->SelectGoldReward(Account, CharacterId, TEXT("Combat_01"), 0, Error); })) return false;
    if (!VerifyTransaction(TEXT("Continue"), [&]() { return Run->ContinueRun(); })) return false;
    if (!VerifyTransaction(TEXT("Shop selection"), [&]() { return Run->SelectRunEncounter(TEXT("Shop_02")); })) return false;
    const FName SkillOffer = Run->GetSkillShopState().Offers[0].OfferId;
    if (!VerifyTransaction(TEXT("Skill purchase"), [&]() { return Run->PurchaseShopOffer(Account, CharacterId, SkillOffer, Error); })) return false;
    if (!VerifyTransaction(TEXT("Recovery purchase"), [&]() { return Run->PurchaseShopOffer(Account, CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Error); })) return false;
    if (!VerifyTransaction(TEXT("Shop departure"), [&]() { return Run->LeaveRunEncounter(); })) return false;
    if (!Run->BeginEncounter(TEXT("Combat_02")) || !Run->MarkCombatStarted()) return false;
    Run->UpdatePartyMemberHP(0, 0.0f);
    if (!VerifyTransaction(TEXT("Defeat publication"), [&]() { return Run->CompleteEncounter(ECombatResult::Defeat); })) return false;
    return true;
}

#endif
