#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/RunRewardTestHelpers.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FGoldRewardFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("GoldReward_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FText Error;

        FGoldRewardFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
        }

        ~FGoldRewardFixture()
        {
            Run->OnRunStateChanged.Clear();
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        bool Initialize()
        {
            if (!Run->PartyDefinition) return false;
            Run->EnableCheckpointSaving(Slot);
            TArray<FRunPartyMember> Members;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunPartyMember& Member = Members.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.ClassId = TEXT("Archer");
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Reward Archer %d"), Index));
                Member.bCreated = true;
                Member.bPlayerControlled = Index == 1;
            }
            return Run->InitializeRun(Members, Error) && Run->GetSaveError().IsEmpty();
        }

        bool FinishBattle(ECombatResult Result = ECombatResult::Victory, bool bPlayerDead = false)
        {
            const FName Node = Run->GetCompletedNodes().IsEmpty() ? TEXT("Combat_01") : TEXT("Combat_02");
            if (!Run->BeginEncounter(Node) || !Run->MarkCombatStarted()) return false;
            Run->UpdatePartyMemberHP(0, Result == ECombatResult::Defeat ? 0.0f : 70.0f);
            Run->UpdatePartyMemberHP(1, Result == ECombatResult::Defeat || bPlayerDead ? 0.0f : 70.0f);
            return Run->CompleteEncounter(Result);
        }

        bool Select(int32 ChoiceIndex, FName Node = TEXT("Combat_01"))
        {
            const FRunPartyMember Player = Run->GetPartyMembers()[1];
            return Run->SelectGoldReward(Player.OwnerAccountId, Player.CharacterId, Node, ChoiceIndex, Error);
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
            return Bytes;
        }
    };

    bool SameRewards(const FRunGoldRewardState& Left, const FRunGoldRewardState& Right)
    {
        return FRunGoldRewardState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGoldRewardSelectionTest, "ProjectA.Run.Reward.ThreeChoicesAndSingleOwnedClaim", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunGoldRewardSelectionTest::RunTest(const FString& Parameters)
{
    FGoldRewardFixture Fixture;
    if (!TestTrue(TEXT("The reward fixture initializes a saved Run"), Fixture.Initialize())) return false;
    TestTrue(TEXT("A new Run has no premature gold choices"), Fixture.Run->GetGoldRewardState().GoldChoices.IsEmpty());
    TestFalse(TEXT("Gold cannot be claimed from the Map phase"), Fixture.Select(0));
    if (!TestTrue(TEXT("Victory persists a reward result"), Fixture.FinishBattle())) return false;
    const FRunGoldRewardState Offered = Fixture.Run->GetGoldRewardState();
    const FRunPartyMember Player = Fixture.Run->GetPartyMembers()[1];
    const FRunPartyMember Companion = Fixture.Run->GetPartyMembers()[0];
    if (!TestEqual(TEXT("Victory offers exactly three amounts"), Offered.GoldChoices.Num(), 3)) return false;
    for (int32 Gold : Offered.GoldChoices) TestTrue(TEXT("Each independently generated choice is within inclusive 5 to 15G"), Gold >= 5 && Gold <= 15);
    TestTrue(TEXT("The pending reward belongs to the completed node"), Offered.NodeId == TEXT("Combat_01") && Offered.Claims.IsEmpty());
    TestTrue(TEXT("Only the selected human character is eligible"), Fixture.Run->GetGoldRewardRecipientIds() == TArray<FGuid>{Player.CharacterId});
    TestEqual(TEXT("Displaying a reward does not grant gold"), Player.Gold, 10);
    TestFalse(TEXT("Continue is disabled until a choice is collected"), Fixture.Run->CanContinueAfterRewards());
    TestFalse(TEXT("The authority rejects Continue before collection"), Fixture.Run->ContinueRun());
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    FRunAccountId Outsider = Player.OwnerAccountId;
    Outsider.Subject += TEXT("_Other");
    TestFalse(TEXT("Negative choice indices are rejected"), Fixture.Select(-1));
    TestFalse(TEXT("An index after the third choice is rejected"), Fixture.Select(3));
    TestFalse(TEXT("A request for a different node is rejected"), Fixture.Select(0, TEXT("Combat_02")));
    TestFalse(TEXT("A foreign account cannot claim another character's reward"), Fixture.Run->SelectGoldReward(Outsider, Player.CharacterId, Offered.NodeId, 0, Fixture.Error));
    TestFalse(TEXT("Unknown characters cannot receive a reward"), Fixture.Run->SelectGoldReward(Player.OwnerAccountId, FGuid::NewGuid(), Offered.NodeId, 0, Fixture.Error));
    TestFalse(TEXT("A same-owner AI companion cannot receive the selected player's reward"), Fixture.Run->SelectGoldReward(Player.OwnerAccountId, Companion.CharacterId, Offered.NodeId, 0, Fixture.Error));
    TestTrue(TEXT("Rejected requests preserve choices gold and durable bytes"), SameRewards(Offered, Fixture.Run->GetGoldRewardState()) && Fixture.Run->GetPartyMembers()[1].Gold == Player.Gold && BeforeBytes == Fixture.ReadBytes());
    if (!TestTrue(TEXT("The owner can choose the third offer"), Fixture.Select(2))) return false;
    TestEqual(TEXT("The selected amount is awarded exactly once"), Fixture.Run->GetPartyMembers()[1].Gold, Player.Gold + Offered.GoldChoices[2]);
    TestTrue(TEXT("The claim stores both character and selected index"), Fixture.Run->GetGoldRewardState().Claims.Num() == 1 && Fixture.Run->GetGoldRewardState().Claims[0].CharacterId == Player.CharacterId && Fixture.Run->GetGoldRewardState().Claims[0].ChoiceIndex == 2);
    TestTrue(TEXT("Reward collection never changes companion state"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Companion, &Fixture.Run->GetPartyMembers()[0], 0));
    const TArray<uint8> ClaimedBytes = Fixture.ReadBytes();
    TestFalse(TEXT("Retrying the same claim cannot award gold twice"), Fixture.Select(2));
    TestFalse(TEXT("Choosing a different card after collection is rejected"), Fixture.Select(0));
    TestTrue(TEXT("Duplicate requests do not write the save again"), ClaimedBytes == Fixture.ReadBytes());
    TestTrue(TEXT("Collection unlocks the normal encounter flow"), Fixture.Run->CanContinueAfterRewards() && Fixture.Run->ContinueRun() && Fixture.Run->GetPhase() == ERunPhase::EncounterChoice);
    TestFalse(TEXT("A result reward cannot be replayed after Continue"), Fixture.Select(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGoldRewardPersistenceTest, "ProjectA.Run.Reward.AtomicClaimAndStableReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunGoldRewardPersistenceTest::RunTest(const FString& Parameters)
{
    FGoldRewardFixture Fixture;
    if (!Fixture.Initialize() || !Fixture.FinishBattle()) return false;
    const FRunGoldRewardState Offered = Fixture.Run->GetGoldRewardState();
    const FRunPartyMember Player = Fixture.Run->GetPartyMembers()[1];
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("A pending result is resumable before reward collection"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Reload retains all offers without rerolling or collecting them"), SameRewards(Offered, Restored->GetGoldRewardState()) && Restored->GetPartyMembers()[1].Gold == Player.Gold && !Restored->CanContinueAfterRewards());
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    int32 Events = 0;
    bool bObservedDurableClaim = false;
    Restored->OnRunStateChanged.AddLambda([&]()
    {
        ++Events;
        TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        bObservedDurableClaim = Durable && Durable->Party[1].Gold == Player.Gold + Offered.GoldChoices[1] && Durable->GoldRewardState.Claims.Num() == 1 && SameRewards(Durable->GoldRewardState, Restored->GetGoldRewardState());
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed claim write reports failure"), Restored->SelectGoldReward(Player.OwnerAccountId, Player.CharacterId, Offered.NodeId, 1, Fixture.Error));
    TestFalse(TEXT("Failed storage supplies a retryable explanation"), Fixture.Error.IsEmpty());
    TestTrue(TEXT("Failed persistence changes neither gold claims offers nor bytes"), Restored->GetPartyMembers()[1].Gold == Player.Gold && SameRewards(Offered, Restored->GetGoldRewardState()) && BeforeBytes == Fixture.ReadBytes() && Events == 0);
    TestFalse(TEXT("A failed award cannot unlock Continue"), Restored->CanContinueAfterRewards());
    TestTrue(TEXT("The same choice succeeds once storage recovers"), Restored->SelectGoldReward(Player.OwnerAccountId, Player.CharacterId, Offered.NodeId, 1, Fixture.Error));
    TestTrue(TEXT("Exactly one notification observes gold and claim committed together"), Events == 1 && bObservedDurableClaim);
    Restored->OnRunStateChanged.Clear();
    if (!TestTrue(TEXT("The paid reward result remains resumable"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Reload preserves paid gold and the selected card"), Fixture.Run->GetPartyMembers()[1].Gold == Player.Gold + Offered.GoldChoices[1] && SameRewards(Restored->GetGoldRewardState(), Fixture.Run->GetGoldRewardState()) && Fixture.Run->CanContinueAfterRewards());
    TestFalse(TEXT("Reload cannot award a previously collected choice again"), Fixture.Select(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGoldRewardLifecycleTest, "ProjectA.Run.Reward.DefeatDeadOwnerAndFinalBattle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunGoldRewardLifecycleTest::RunTest(const FString& Parameters)
{
    FGoldRewardFixture Defeat;
    if (!Defeat.Initialize() || !Defeat.FinishBattle(ECombatResult::Defeat)) return false;
    TestTrue(TEXT("Defeat has no selectable rewards or claims"), Defeat.Run->GetGoldRewardState().GoldChoices.IsEmpty() && Defeat.Run->GetGoldRewardState().Claims.IsEmpty());
    TestFalse(TEXT("Defeat cannot grant gold"), Defeat.Select(0));
    TestEqual(TEXT("Defeat preserves the previous gold balance"), Defeat.Run->GetPartyMembers()[1].Gold, 10);
    FGoldRewardFixture Victory;
    if (!Victory.Initialize() || !Victory.FinishBattle(ECombatResult::Victory, true)) return false;
    TestTrue(TEXT("A dead selected owner can collect the victory won by a companion"), Victory.Select(0));
    TestEqual(TEXT("Gold collection does not revive a dead character"), Victory.Run->GetPartyMembers()[1].CurrentHP, 0.0f);
    const int32 FirstGold = Victory.Run->GetPartyMembers()[1].Gold;
    if (!Victory.Run->ContinueRun() || !Victory.Run->SelectRunEncounter(TEXT("Shop_01")) || !Victory.Run->LeaveRunEncounter() || !Victory.FinishBattle(ECombatResult::Victory, true)) return false;
    TestTrue(TEXT("The final battle replaces the prior node and clears its claims"), Victory.Run->GetGoldRewardState().NodeId == TEXT("Combat_02") && Victory.Run->GetGoldRewardState().GoldChoices.Num() == 3 && Victory.Run->GetGoldRewardState().Claims.IsEmpty());
    TestFalse(TEXT("The previous node's delayed claim cannot spend the final reward"), Victory.Select(0));
    TestFalse(TEXT("The final victory waits on its own reward"), Victory.Run->ContinueRun());
    const int32 FinalReward = Victory.Run->GetGoldRewardState().GoldChoices[2];
    TestTrue(TEXT("The final reward can be selected before Run completion"), Victory.Select(2, TEXT("Combat_02")));
    TestEqual(TEXT("Both independent victories contribute exactly their chosen amounts"), Victory.Run->GetPartyMembers()[1].Gold, FirstGold + FinalReward);
    TestTrue(TEXT("Collecting the last reward unlocks Complete without an extra shop"), Victory.Run->ContinueRun() && Victory.Run->GetPhase() == ERunPhase::Complete);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGoldRewardLegacyTest, "ProjectA.Run.Reward.LegacyResultAndNextVictoryMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunGoldRewardLegacyTest::RunTest(const FString& Parameters)
{
    for (bool bHasShop : {false, true})
    {
        FGoldRewardFixture Fixture;
        if (!Fixture.Initialize() || !Fixture.FinishBattle()) return false;
        TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        if (!Legacy) return false;
        Legacy->GoldRewardState = FRunGoldRewardState();
        Legacy->ItemShopState = FRunItemShopState();
        if (!bHasShop)
        {
            Legacy->SkillShopState = FRunSkillShopState();
            for (FRunPartyMember& Member : Legacy->Party)
            {
                Member.Gold = 0;
                Member.bHasSkillLoadout = false;
                Member.Skills.Reset();
            }
        }
        if (!FRunCheckpointStorage::Save(Legacy.Get(), Fixture.Slot, Fixture.Error)) return false;
        const TArray<uint8> LegacyBytes = Fixture.ReadBytes();
        if (!TestTrue(TEXT("A result saved before rewards existed remains loadable"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error))) return false;
        TestTrue(TEXT("Loading never rerolls rewards for a historical result or rewrites it"), Fixture.Run->GetGoldRewardState().SchemaVersion == 0 && Fixture.Run->GetGoldRewardState().GoldChoices.IsEmpty() && LegacyBytes == Fixture.ReadBytes());
        TestFalse(TEXT("A historical result cannot claim retroactive gold"), Fixture.Select(0));
        if (!TestTrue(TEXT("Historical results can Continue without a reward"), Fixture.Run->CanContinueAfterRewards() && Fixture.Run->ContinueRun())) return false;
        if (!Fixture.Run->SelectRunEncounter(TEXT("Shop_01")) || !Fixture.Run->LeaveRunEncounter() || !Fixture.FinishBattle()) return false;
        if (bHasShop)
        {
            TestTrue(TEXT("An existing modern Run gains rewards on its next new victory"), Fixture.Run->GetGoldRewardState().SchemaVersion == 1 && Fixture.Run->GetGoldRewardState().GoldChoices.Num() == 3 && !Fixture.Run->CanContinueAfterRewards());
            TestTrue(TEXT("Migrated rewards follow the ordinary collection flow"), RunRewardTests::CollectPendingGoldRewards(Fixture.Run.Get()));
        }
        else
        {
            TestTrue(TEXT("Pre-shop legacy Runs preserve their original zero-gold progression"), Fixture.Run->GetGoldRewardState().SchemaVersion == 0 && Fixture.Run->GetGoldRewardState().GoldChoices.IsEmpty() && Fixture.Run->GetPartyMembers()[1].Gold == 0 && Fixture.Run->CanContinueAfterRewards());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGoldRewardTerminalRetryTest, "ProjectA.Run.Reward.TerminalRetryDoesNotReroll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunGoldRewardTerminalRetryTest::RunTest(const FString& Parameters)
{
    FGoldRewardFixture Fixture;
    if (!Fixture.Run->PartyDefinition) return false;
    TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog(DuplicateObject<UPartyDefinitionDataAsset>(Fixture.Run->PartyDefinition.Get(), GetTransientPackage()));
    TStrongObjectPtr<URunEncounterPoolDataAsset> Pool(NewObject<URunEncounterPoolDataAsset>());
    Pool->GoldRewardMin = 7;
    Pool->GoldRewardMax = 7;
    Catalog->RunEncounterPool = Pool.Get();
    Fixture.Run->PartyDefinition = Catalog.Get();
    if (!Fixture.Initialize() || !Fixture.Run->BeginEncounter(TEXT("Combat_01")) || !Fixture.Run->MarkCombatStarted()) return false;
    for (const FRunPartyMember& Member : Fixture.Run->GetPartyMembers()) Fixture.Run->UpdatePartyMemberHP(Member.SlotIndex, 70.0f);
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed terminal save cannot expose its generated reward"), Fixture.Run->CompleteEncounter(ECombatResult::Victory));
    TestTrue(TEXT("Terminal failure retains combat empty visible offers and the precombat save"), Fixture.Run->GetPhase() == ERunPhase::Combat && Fixture.Run->GetGoldRewardState().GoldChoices.IsEmpty() && Fixture.Run->GetCompletedNodes().IsEmpty() && BeforeBytes == Fixture.ReadBytes());
    Pool->GoldRewardMin = 11;
    Pool->GoldRewardMax = 11;
    if (!TestTrue(TEXT("The same victory retries after storage recovers"), Fixture.Run->CompleteEncounter(ECombatResult::Victory))) return false;
    TestTrue(TEXT("Retry commits the originally drawn values despite a later catalog change"), Fixture.Run->GetGoldRewardState().GoldChoices == TArray<int32>{7, 7, 7});
    TestTrue(TEXT("An already committed result cannot reroll a second time"), !Fixture.Run->CompleteEncounter(ECombatResult::Victory) && Fixture.Run->GetGoldRewardState().GoldChoices == TArray<int32>{7, 7, 7});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGoldRewardMalformedSaveTest, "ProjectA.Run.Reward.MalformedSaveDoesNotMutateActiveRun", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunGoldRewardMalformedSaveTest::RunTest(const FString& Parameters)
{
    FGoldRewardFixture Fixture;
    if (!Fixture.Initialize() || !Fixture.FinishBattle()) return false;
    TStrongObjectPtr<URunSaveGame> Valid(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Valid) return false;
    const FRunGoldRewardState Before = Fixture.Run->GetGoldRewardState();
    const FRunPartyMember Player = Fixture.Run->GetPartyMembers()[1];
    for (int32 Case = 0; Case < 8; ++Case)
    {
        TStrongObjectPtr<URunSaveGame> Invalid(DuplicateObject<URunSaveGame>(Valid.Get(), GetTransientPackage()));
        FRunGoldRewardClaim Claim;
        Claim.CharacterId = Player.CharacterId;
        Claim.ChoiceIndex = 0;
        switch (Case)
        {
        case 0:
            Invalid->GoldRewardState.SchemaVersion = 999;
            break;
        case 1:
            Invalid->GoldRewardState.GoldChoices.Pop();
            break;
        case 2:
            Invalid->GoldRewardState.GoldChoices[0] = 0;
            break;
        case 3:
            Invalid->GoldRewardState.NodeId = TEXT("Combat_02");
            break;
        case 4:
            Claim.CharacterId = FGuid::NewGuid();
            Invalid->GoldRewardState.Claims.Add(Claim);
            break;
        case 5:
            Invalid->GoldRewardState.Claims.Add(Claim);
            Invalid->GoldRewardState.Claims.Add(Claim);
            break;
        case 6:
            Claim.ChoiceIndex = 3;
            Invalid->GoldRewardState.Claims.Add(Claim);
            break;
        default:
            Claim.CharacterId = Invalid->Party[0].CharacterId;
            Invalid->GoldRewardState.Claims.Add(Claim);
            break;
        }
        if (!FRunCheckpointStorage::Save(Invalid.Get(), Fixture.Slot, Fixture.Error)) return false;
        const TArray<uint8> InvalidBytes = Fixture.ReadBytes();
        TestFalse(TEXT("Continue eligibility rejects malformed reward metadata"), Fixture.Run->CanContinueStandaloneSavedRun(Fixture.Error));
        TestFalse(TEXT("Malformed reward loading is rejected"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error));
        TestFalse(TEXT("Malformed rewards have a visible explanation"), Fixture.Error.IsEmpty());
        TestTrue(TEXT("Rejected loading preserves active choices balance phase and original file"), SameRewards(Before, Fixture.Run->GetGoldRewardState()) && Fixture.Run->GetPartyMembers()[1].Gold == Player.Gold && Fixture.Run->GetPhase() == ERunPhase::Result && InvalidBytes == Fixture.ReadBytes());
    }
    return true;
}

#endif
