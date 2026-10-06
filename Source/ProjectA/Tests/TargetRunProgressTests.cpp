#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunProgressTest, "ProjectA.Run.Target.SixtyChoicesTwentyCombats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunProgressTest::RunTest(const FString& Parameters)
{
    const FRunRouteDefinition& Route = RunProgressRules::GetTargetRoute();
    TestEqual(TEXT("Target route has twenty combats"), Route.Nodes.Num(), 20);
    TestEqual(TEXT("Old prototype remains ten combats"), RunProgressRules::GetPrototypeRoute().Nodes.Num(), 10);
    TestEqual(TEXT("Old legacy remains two combats"), RunProgressRules::GetLegacyPrototypeRoute().Nodes.Num(), 2);
    FRunTargetState Target;
    Target.SchemaVersion = 1;
    Target.Groups = GetDefault<UTargetRunDefinitionDataAsset>()->Groups;
    Target.EncounterPool = GetDefault<UTargetRunDefinitionDataAsset>()->EncounterPool;
    Target.EncounterQuery = GetDefault<UTargetRunDefinitionDataAsset>()->EncounterQuery;
    TArray<FName> Completed;
    FRunProgressView Progress{Route.Nodes, Completed, NAME_None, NAME_None, ERunPhase::EncounterChoice, ECombatResult::None};
    FRunEncounterProgress Encounter;
    Encounter.SchemaVersion = 2;
    int32 Pve = 0;
    int32 Snapshot = 0;
    for (int32 CombatIndex = 0; CombatIndex < 20; ++CombatIndex)
    {
        Encounter.AfterCompletedNodeCount = CombatIndex;
        for (int32 VisitIndex = 0; VisitIndex < 3; ++VisitIndex)
        {
            Encounter.VisitIndex = VisitIndex;
            Encounter.SelectedEncounterId = NAME_None;
            Encounter.bCompleted = false;
            Progress.Phase = ERunPhase::EncounterChoice;
            if (!TestTrue(TEXT("Each visit has three eligible candidates"), UTargetRunDefinitionDataAsset::BuildOffers(Target, CombatIndex, VisitIndex, Encounter.Offers) && Encounter.Offers.Num() == 3)) return false;
            if (!TestTrue(TEXT("Initial and later choice boundaries validate"), RunProgressRules::ValidateEncounterProgress(Route, Progress, Encounter) && RunProgressRules::ValidatePhase(Progress, true, true))) return false;
            TestFalse(TEXT("Target choices cannot be loaded as a prototype visit"), RunProgressRules::ValidateEncounterProgress(RunProgressRules::GetPrototypeRoute(), Progress, Encounter));
            Encounter.SelectedEncounterId = Encounter.Offers[VisitIndex].EncounterId;
            Progress.Phase = ERunPhase::Shop;
            TestTrue(TEXT("Selected service or shop validates"), RunProgressRules::ValidateEncounterProgress(Route, Progress, Encounter));
            Target.CompletedEncounterChoices.Add(Encounter.SelectedEncounterId);
        }
        Encounter.bCompleted = true;
        Progress.Phase = ERunPhase::Map;
        TestTrue(TEXT("Only the third completed visit opens combat"), RunProgressRules::ValidateEncounterProgress(Route, Progress, Encounter));
        FRunEncounterProgress TooEarly = Encounter;
        TooEarly.VisitIndex = 1;
        TestFalse(TEXT("Skipping the third encounter is rejected"), RunProgressRules::ValidateEncounterProgress(Route, Progress, TooEarly));
        Progress.CurrentNode = Route.Nodes[CombatIndex].NodeId;
        Progress.CurrentEncounter = Route.Nodes[CombatIndex].EncounterId;
        Progress.Phase = ERunPhase::Combat;
        Progress.Result = ECombatResult::None;
        TestTrue(TEXT("Next combat identity matches the route"), RunProgressRules::ValidatePhase(Progress, true, true));
        if (Progress.CurrentEncounter == TEXT("TargetPvE")) ++Pve;
        else if (Progress.CurrentEncounter == TEXT("TargetSnapshot")) ++Snapshot;
        Completed.Add(Progress.CurrentNode);
        Progress.Phase = ERunPhase::Result;
        Progress.Result = ECombatResult::Victory;
        TestTrue(TEXT("Combat result retains its preceding three choices"), RunProgressRules::ValidateEncounterProgress(Route, Progress, Encounter));
        Progress.CurrentEncounter = NAME_None;
        if (Completed.Num() == 20)
        {
            Progress.Phase = RunProgressRules::GetContinuationPhase(Route, Completed.Num(), Encounter);
            TestTrue(TEXT("Final snapshot ends after exactly sixty choices"), Progress.Phase == ERunPhase::Complete && RunProgressRules::ValidatePhase(Progress, true, true) && RunProgressRules::ValidateEncounterProgress(Route, Progress, Encounter));
        }
    }
    TestEqual(TEXT("Exactly sixty selections"), Target.CompletedEncounterChoices.Num(), 60);
    TestEqual(TEXT("Exactly ten PvE combats"), Pve, 10);
    TestEqual(TEXT("Exactly ten local Snapshot combats"), Snapshot, 10);
    FProfessionDefinition AfterPve;
    FProfessionDefinition AfterSnapshot;
    UTargetRunDefinitionDataAsset::ApplyGrowth(Target, 1, AfterPve);
    UTargetRunDefinitionDataAsset::ApplyGrowth(Target, 2, AfterSnapshot);
    TestEqual(TEXT("PvE grants the trial HP growth"), AfterPve.MaxHP, 105.0f);
    TestEqual(TEXT("PvE grants direct speed growth"), AfterPve.Speed, 11.0f);
    TestEqual(TEXT("Snapshot grants no additional HP growth"), AfterSnapshot.MaxHP, AfterPve.MaxHP);
    TestEqual(TEXT("Snapshot grants no additional speed growth"), AfterSnapshot.Speed, AfterPve.Speed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunSaveValuesTest, "ProjectA.Run.Target.FrozenValuesSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunSaveValuesTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->TargetRun.SchemaVersion = 1;
    Save->TargetRun.Groups = GetDefault<UTargetRunDefinitionDataAsset>()->Groups;
    Save->TargetRun.Groups[0].SpeedGrowth = 1.25f;
    Save->TargetRun.EncounterPool = GetDefault<UTargetRunDefinitionDataAsset>()->EncounterPool;
    Save->TargetRun.EncounterQuery = GetDefault<UTargetRunDefinitionDataAsset>()->EncounterQuery;
    Save->TargetRun.CompletedEncounterChoices = {TEXT("TargetOffer_01"), TEXT("TargetOffer_02"), TEXT("TargetOffer_03")};
    Save->Nodes = RunProgressRules::GetTargetRoute().Nodes;
    Save->EncounterProgress.SchemaVersion = 2;
    Save->EncounterProgress.VisitIndex = 2;
    Save->EncounterProgress.AfterCompletedNodeCount = 0;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Serialize frozen target data"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Deserialize the same SaveGame class"), Restored.Get())) return false;
    TestTrue(TEXT("All fixed lineups, snapshots, rewards, growth and history survive serialization"), FRunTargetState::StaticStruct()->CompareScriptStruct(&Save->TargetRun, &Restored->TargetRun, 0));
    TestEqual(TEXT("Frozen fractional speed growth survives serialization"), Restored->TargetRun.Groups[0].SpeedGrowth, 1.25f);
    TestEqual(TEXT("Visit index survives serialization"), Restored->EncounterProgress.VisitIndex, 2);
    Save->TargetRun.Groups[0].Opponent.Members[0].Stats.MaxHP += 99.0f;
    TestEqual(TEXT("Restored snapshot owns its frozen value independently"), Restored->TargetRun.Groups[0].Opponent.Members[0].Stats.MaxHP, 100.0f);
    const FRunTargetState Empty;
    TStrongObjectPtr<URunSaveGame> Legacy(NewObject<URunSaveGame>());
    TestTrue(TEXT("Default additional target fields preserve historical saves"), FRunTargetState::StaticStruct()->CompareScriptStruct(&Legacy->TargetRun, &Empty, 0));
    return true;
}

namespace
{
    bool SameTargetTransactionParty(const TArray<FRunPartyMember>& Left, const TArray<FRunPartyMember>& Right)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], 0)) return false;
        }
        return true;
    }

    bool SameTargetTransactionState(const URunSaveGame& Saved, const URunStateSubsystem& Run)
    {
        return Saved.Phase == Run.GetPhase() && Saved.Result == Run.GetLastResult() && Saved.CurrentNode == Run.GetCurrentNodeId() && Saved.CurrentEncounter == Run.GetCurrentEncounterId() && Saved.CompletedNodes == Run.GetCompletedNodes() && SameTargetTransactionParty(Saved.Party, Run.GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Saved.Identity, &Run.GetRunIdentity(), 0) && FRunTargetState::StaticStruct()->CompareScriptStruct(&Saved.TargetRun, &Run.GetTargetRunState(), 0) && FRunEncounterProgress::StaticStruct()->CompareScriptStruct(&Saved.EncounterProgress, &Run.GetEncounterProgress(), 0) && FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Saved.GoldRewardState, &Run.GetGoldRewardState(), 0) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Saved.SkillShopState, &Run.GetSkillShopState(), 0) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&Saved.ItemShopState, &Run.GetItemShopState(), 0);
    }

    struct FTargetRunTransactionFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("TargetTxn_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FText Error;
        int32 ReloadedBoundaries = 0;

        FTargetRunTransactionFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
        }

        ~FTargetRunTransactionFixture()
        {
            Run->OnRunStateChanged.Clear();
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
            return Bytes;
        }

        bool ReloadBoundary(FAutomationTestBase& Test)
        {
            TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Test.TestTrue(TEXT("The durable target boundary equals the published transaction"), Saved.IsValid() && SameTargetTransactionState(*Saved.Get(), *Run.Get()))) return false;
            const TArray<uint8> Before = ReadBytes();
            TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Instance.Get()));
            Restored->PartyDefinition = Run->PartyDefinition;
            Restored->EnableCheckpointSaving(Slot);
            if (!Test.TestTrue(TEXT("A fresh subsystem validates and reloads the real target file: ") + Error.ToString(), Restored->LoadStandaloneCheckpoint(Error))) return false;
            if (!Test.TestTrue(TEXT("Reload preserves every published target, ownership, party, shop and reward value without rewriting the file"), SameTargetTransactionState(*Saved.Get(), *Restored.Get()) && Before == ReadBytes())) return false;
            Run.Reset(Restored.Get());
            ++ReloadedBoundaries;
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunDurableSyntheticResultsTest, "ProjectA.Run.Target.SyntheticResultsAllEightyStagesDurableTransactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunDurableSyntheticResultsTest::RunTest(const FString& Parameters)
{
    // Supply synthetic victory/HP inputs only to verify durable progression; no actor combat, Ready replay or normal-difficulty completion is claimed.
    // 영속 진행 검증에만 합성 승리/HP를 입력하며 액터 전투·Ready 재실행·정상 난이도 완주를 검증한 것으로 취급하지 않습니다.
    FTargetRunTransactionFixture Fixture;
    if (!TestNotNull(TEXT("The real project party catalog is available"), Fixture.Run->PartyDefinition.Get())) return false;
    TArray<FRunPartyMember> Party;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FRunPartyMember& Member = Party.AddDefaulted_GetRef();
        Member.SlotIndex = Index;
        Member.ClassId = TEXT("Warrior");
        Member.CharacterName = FText::FromString(FString::Printf(TEXT("Synthetic transaction %d"), Index));
        Member.bCreated = true;
        Member.bPlayerControlled = Index == 0;
    }
    if (!TestTrue(TEXT("Public initialization commits a fresh target Run and its real consumable asset"), Fixture.Run->InitializeTargetRun(Party, Fixture.Error) && Fixture.ReloadBoundary(*this))) return false;
    const FRunIdentityData OriginalIdentity = Fixture.Run->GetRunIdentity();
    const FRunTargetState FrozenTarget = Fixture.Run->GetTargetRunState();
    const FGuid BuyerId = Fixture.Run->GetPartyMembers()[0].CharacterId;
    const FRunAccountId BuyerAccount = Fixture.Run->GetPartyMembers()[0].OwnerAccountId;
    int32 ExpectedGold = Fixture.Run->GetPartyMembers()[0].Gold;
    int32 PveResults = 0;
    int32 SnapshotResults = 0;
    for (int32 CombatIndex = 0; CombatIndex < 20; ++CombatIndex)
    {
        const FName Node = Fixture.Run->GetNodes()[CombatIndex].NodeId;
        for (int32 VisitIndex = 0; VisitIndex < 3; ++VisitIndex)
        {
            if (!TestTrue(TEXT("Every combat requires its next persisted three-candidate encounter"), Fixture.Run->GetPhase() == ERunPhase::EncounterChoice && Fixture.Run->GetEncounterProgress().Offers.Num() == 3 && Fixture.Run->GetTargetRunState().CompletedEncounterChoices.Num() == CombatIndex * 3 + VisitIndex && !Fixture.Run->CanStartNode(Node))) return false;
            const FName Offer = Fixture.Run->GetEncounterProgress().Offers[(CombatIndex + VisitIndex) % 3].EncounterId;
            if (!TestTrue(TEXT("The public encounter selection is durable"), Fixture.Run->SelectRunEncounter(Offer) && Fixture.Run->GetPhase() == ERunPhase::Shop && Fixture.ReloadBoundary(*this))) return false;
            if (!TestTrue(TEXT("The public encounter exit is durable"), Fixture.Run->LeaveRunEncounter() && Fixture.ReloadBoundary(*this))) return false;
            if (!TestEqual(TEXT("Exactly one chosen encounter is appended per exit"), Fixture.Run->GetTargetRunState().CompletedEncounterChoices.Num(), CombatIndex * 3 + VisitIndex + 1)) return false;
        }
        if (!TestTrue(TEXT("The third completed choice opens only the expected node"), Fixture.Run->GetPhase() == ERunPhase::Map && Fixture.Run->CanStartNode(Node))) return false;
        FProfessionDefinition BeforeGrowth;
        if (!Fixture.Run->ResolveMemberProfession(Fixture.Run->GetPartyMembers()[0], BeforeGrowth, Fixture.Error)) return false;
        TMap<int32, float> FinalHP;
        TMap<int32, TArray<FRunConsumableStack>> FinalStock;
        for (const FRunPartyMember& Member : Fixture.Run->GetPartyMembers())
        {
            FinalHP.Add(Member.SlotIndex, Member.CurrentHP - 1.f);
            FinalStock.Add(Member.SlotIndex, Member.Consumables);
        }
        if (!TestTrue(TEXT("The public API enters synthetic combat without creating actor or Ready evidence"), Fixture.Run->BeginEncounter(Node) && Fixture.Run->MarkCombatStarted())) return false;
        if (CombatIndex == 19)
        {
            const TArray<uint8> BeforeWrite = Fixture.ReadBytes();
            const TArray<FRunPartyMember> BeforeParty = Fixture.Run->GetPartyMembers();
            int32 Publications = 0;
            Fixture.Run->OnRunStateChanged.AddLambda([&Publications]() { ++Publications; });
            FRunCheckpointStorage::FailNextWriteForTesting();
            TestFalse(TEXT("The final synthetic Snapshot result rejects an injected durable-write failure"), Fixture.Run->CompleteEncounter(ECombatResult::Victory, FinalHP, FinalStock));
            TestTrue(TEXT("A failed final write retains the previous bytes, combat, nineteen wins, sixty choices, party and publication count"), BeforeWrite == Fixture.ReadBytes() && Fixture.Run->GetPhase() == ERunPhase::Combat && Fixture.Run->GetCompletedNodes().Num() == 19 && Fixture.Run->GetTargetRunState().CompletedEncounterChoices.Num() == 60 && SameTargetTransactionParty(BeforeParty, Fixture.Run->GetPartyMembers()) && Publications == 0);
            const bool bRetried = Fixture.Run->CompleteEncounter(ECombatResult::Victory, FinalHP, FinalStock);
            Fixture.Run->OnRunStateChanged.Clear();
            if (!TestTrue(TEXT("An identical result retry commits once after the failed write"), bRetried && Publications == 1)) return false;
        }
        else if (!TestTrue(TEXT("The explicit synthetic victory passes the production result/save validators"), Fixture.Run->CompleteEncounter(ECombatResult::Victory, FinalHP, FinalStock))) return false;
        if (!Fixture.ReloadBoundary(*this)) return false;
        TestEqual(TEXT("Each result persists exactly one new combat completion"), Fixture.Run->GetCompletedNodes().Num(), CombatIndex + 1);
        TestEqual(TEXT("Result publication preserves the supplied HP instead of healing during growth"), Fixture.Run->GetPartyMembers()[0].CurrentHP, FinalHP[0]);
        FProfessionDefinition AfterGrowth;
        if (!Fixture.Run->ResolveMemberProfession(Fixture.Run->GetPartyMembers()[0], AfterGrowth, Fixture.Error)) return false;
        const bool bPve = CombatIndex % 2 == 0;
        const FTargetRunGroup& Group = FrozenTarget.Groups[CombatIndex / 2];
        TestEqual(TEXT("Only PvE results add the frozen maximum-HP growth"), AfterGrowth.MaxHP, BeforeGrowth.MaxHP + (bPve ? Group.MaxHPGrowth : 0.f));
        TestEqual(TEXT("Only PvE results add the frozen speed growth"), AfterGrowth.Speed, BeforeGrowth.Speed + (bPve ? Group.SpeedGrowth : 0.f));
        if (bPve)
        {
            ++PveResults;
            if (!TestTrue(TEXT("PvE requires exactly the frozen three-way reward before Continue"), Fixture.Run->GetGoldRewardState().SchemaVersion == 1 && Fixture.Run->GetGoldRewardState().GoldChoices == Group.GoldChoices && !Fixture.Run->CanContinueAfterRewards() && !Fixture.Run->ContinueRun())) return false;
            const int32 Choice = (CombatIndex / 2) % 3;
            if (!TestTrue(TEXT("The original human claims the public PvE reward"), Fixture.Run->SelectGoldReward(BuyerAccount, BuyerId, Node, Choice, Fixture.Error) && Fixture.ReloadBoundary(*this))) return false;
            ExpectedGold += Group.GoldChoices[Choice];
            TestFalse(TEXT("An accepted reward cannot be claimed twice after reloading"), Fixture.Run->SelectGoldReward(BuyerAccount, BuyerId, Node, Choice, Fixture.Error));
        }
        else
        {
            ++SnapshotResults;
            const FRunGoldRewardState Empty;
            TestTrue(TEXT("Snapshot results have no reward state or recipients and may Continue immediately"), FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Empty, &Fixture.Run->GetGoldRewardState(), 0) && Fixture.Run->GetGoldRewardRecipientIds().IsEmpty() && Fixture.Run->CanContinueAfterRewards());
            TestFalse(TEXT("Snapshot results reject a fabricated gold reward request"), Fixture.Run->SelectGoldReward(BuyerAccount, BuyerId, Node, 0, Fixture.Error));
        }
        TestEqual(TEXT("Only one selected PvE reward changes the human balance"), Fixture.Run->GetPartyMembers()[0].Gold, ExpectedGold);
        if (!TestTrue(TEXT("Public Continue commits the next target boundary"), Fixture.Run->CanContinueAfterRewards() && Fixture.Run->ContinueRun())) return false;
        if (CombatIndex < 19 && !Fixture.ReloadBoundary(*this)) return false;
    }
    TestEqual(TEXT("The durable route includes ten synthetic PvE results"), PveResults, 10);
    TestEqual(TEXT("The durable route includes ten synthetic local Snapshot results"), SnapshotResults, 10);
    TestEqual(TEXT("Every resumable stable boundary was reloaded in a fresh subsystem"), Fixture.ReloadedBoundaries, 170);
    TestTrue(TEXT("Completion requires all sixty choices and twenty results with original identity intact"), Fixture.Run->GetPhase() == ERunPhase::Complete && Fixture.Run->GetCompletedNodes().Num() == 20 && Fixture.Run->GetTargetRunState().CompletedEncounterChoices.Num() == 60 && FRunIdentityData::StaticStruct()->CompareScriptStruct(&OriginalIdentity, &Fixture.Run->GetRunIdentity(), 0));
    TStrongObjectPtr<URunSaveGame> Completed(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    TestTrue(TEXT("The terminal SaveGame deserializes to the complete published state"), Completed.IsValid() && SameTargetTransactionState(*Completed.Get(), *Fixture.Run.Get()));
    const TArray<uint8> CompletedBytes = Fixture.ReadBytes();
    TestFalse(TEXT("The finished Run cannot execute another Continue"), Fixture.Run->ContinueRun());
    TestFalse(TEXT("Menu Continue rejects the completed Run"), Fixture.Run->CanContinueStandaloneSavedRun(Fixture.Error));
    TestFalse(TEXT("Direct standalone loading also rejects completed progress"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error));
    TestTrue(TEXT("Rejected completion requests preserve the terminal bytes and in-memory completion"), CompletedBytes == Fixture.ReadBytes() && Fixture.Run->GetPhase() == ERunPhase::Complete);
    return true;
}

#endif
