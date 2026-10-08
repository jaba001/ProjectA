#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

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
    bool SameTargetRewards(const FRunGoldRewardState& Left, const FRunGoldRewardState& Right)
    {
        if (Left.SchemaVersion != Right.SchemaVersion || Left.NodeId != Right.NodeId || Left.GoldChoices != Right.GoldChoices || Left.BonusGold != Right.BonusGold || Left.ItemChoices.Num() != Right.ItemChoices.Num() || Left.Claims.Num() != Right.Claims.Num()) return false;
        for (int32 Index = 0; Index < Left.ItemChoices.Num(); ++Index)
        {
            if (!RunItemShopCatalog::IsSameDefinition(Left.ItemChoices[Index], Right.ItemChoices[Index])) return false;
        }
        for (int32 Index = 0; Index < Left.Claims.Num(); ++Index)
        {
            if (Left.Claims[Index].CharacterId != Right.Claims[Index].CharacterId || Left.Claims[Index].ChoiceIndex != Right.Claims[Index].ChoiceIndex) return false;
        }
        return true;
    }

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
        return Saved.Phase == Run.GetPhase() && Saved.Result == Run.GetLastResult() && Saved.CurrentNode == Run.GetCurrentNodeId() && Saved.CurrentEncounter == Run.GetCurrentEncounterId() && Saved.CompletedNodes == Run.GetCompletedNodes() && Saved.WeaponSkillAcquisitionVersion == (Run.UsesWeaponSkills() ? 1 : 0) && FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Saved.WeaponSkillRules, &Run.GetWeaponSkillRules(), 0) && SameTargetTransactionParty(Saved.Party, Run.GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Saved.Identity, &Run.GetRunIdentity(), 0) && FRunTargetState::StaticStruct()->CompareScriptStruct(&Saved.TargetRun, &Run.GetTargetRunState(), 0) && FRunEncounterProgress::StaticStruct()->CompareScriptStruct(&Saved.EncounterProgress, &Run.GetEncounterProgress(), 0) && SameTargetRewards(Saved.GoldRewardState, Run.GetGoldRewardState()) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Saved.SkillShopState, &Run.GetSkillShopState(), 0) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&Saved.ItemShopState, &Run.GetItemShopState(), 0);
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

        bool InitializeEveryProfession()
        {
            if (!Run->PartyDefinition) return false;
            TArray<FRunPartyMember> Party;
            const FName Classes[] = {TEXT("Warrior"), TEXT("Archer"), TEXT("Mage"), TEXT("Rogue")};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Classes); ++Index)
            {
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.ClassId = Classes[Index];
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Weapon transaction %d"), Index));
                Member.bCreated = true;
                Member.bPlayerControlled = Index == 0;
            }
            return Run->InitializeTargetRun(Party, Error);
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
            return Bytes;
        }

        bool BeginNextSyntheticCombat()
        {
            while (Run->GetPhase() == ERunPhase::EncounterChoice)
            {
                if (Run->GetEncounterProgress().Offers.Num() != 3 || !Run->SelectRunEncounter(Run->GetEncounterProgress().Offers[0].EncounterId) || !Run->LeaveRunEncounter()) return false;
            }
            return Run->GetPhase() == ERunPhase::Map && Run->GetNodes().IsValidIndex(Run->GetCompletedNodes().Num()) && Run->BeginEncounter(Run->GetNodes()[Run->GetCompletedNodes().Num()].NodeId) && Run->MarkCombatStarted();
        }

        bool FreezeInitialBasicShop()
        {
            TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Saved || Saved->ItemShopState.Revision != 0 || !Saved->TargetRun.CompletedEncounterChoices.IsEmpty()) return false;
            // Use a deterministic test-only seed with a basic shop so transaction checks do not depend on random themes.
            // 거래 검증이 무작위 테마에 의존하지 않도록 기본상점이 있는 테스트 전용 시드를 고정합니다.
            for (int32 Seed = 0; Seed < 10000; ++Seed)
            {
                Saved->TargetRun.EncounterSeed = Seed;
                if (!UTargetRunDefinitionDataAsset::BuildOffers(Saved->TargetRun, 0, 0, Saved->EncounterProgress.Offers)) return false;
                if (!Saved->EncounterProgress.Offers.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag() == FRunEncounterOffer::GetBasicItemShopTag(); })) continue;
                return FRunCheckpointStorage::Save(Saved.Get(), Slot, Error) && Run->LoadStandaloneCheckpoint(Error);
            }
            return false;
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
    const FRunWeaponSkillRulesState FrozenWeaponRules = Fixture.Run->GetWeaponSkillRules();
    TestTrue(TEXT("New target Runs freeze weapon acquisition and exclude every skill-shop candidate"), Fixture.Run->UsesWeaponSkills() && Fixture.Run->GetSkillShopState().SchemaVersion == 0 && !FrozenTarget.EncounterPool.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag().MatchesTag(FRunEncounterOffer::GetSkillShopTag()); }));
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
            TestFalse(TEXT("No encounter rotation reintroduces the retired skill shop"), Fixture.Run->GetEncounterProgress().Offers.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag().MatchesTag(FRunEncounterOffer::GetSkillShopTag()); }));
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
            const FRunGoldRewardState Rewards = Fixture.Run->GetGoldRewardState();
            const FRunPartyMember BeforeClaim = Fixture.Run->GetPartyMembers()[0];
            if (!TestTrue(TEXT("PvE requires three fixed items and one gold award within its frozen group range before Continue"), Rewards.SchemaVersion == 2 && Rewards.GoldChoices.IsEmpty() && Rewards.ItemChoices.Num() == 3 && Rewards.BonusGold >= FMath::Min3(Group.GoldChoices[0], Group.GoldChoices[1], Group.GoldChoices[2]) && Rewards.BonusGold <= FMath::Max3(Group.GoldChoices[0], Group.GoldChoices[1], Group.GoldChoices[2]) && !Fixture.Run->CanContinueAfterRewards() && !Fixture.Run->ContinueRun())) return false;
            const int32 Choice = (CombatIndex / 2) % 3;
            if (!TestTrue(TEXT("The original human claims the public PvE reward"), Fixture.Run->SelectGoldReward(BuyerAccount, BuyerId, Node, Choice, Fixture.Error) && Fixture.ReloadBoundary(*this))) return false;
            const FRunPartyMember& AfterClaim = Fixture.Run->GetPartyMembers()[0];
            TestTrue(TEXT("Claiming appends only the displayed copy without changing equipped skills or slots"), AfterClaim.Items.Num() == BeforeClaim.Items.Num() + 1 && RunItemShopCatalog::IsSameDefinition(AfterClaim.Items.Last(), Rewards.ItemChoices[Choice]) && AfterClaim.Skills == BeforeClaim.Skills && AfterClaim.InnateSkills == BeforeClaim.InnateSkills && FRunEquipmentState::StaticStruct()->CompareScriptStruct(&BeforeClaim.Equipment, &AfterClaim.Equipment, 0));
            ExpectedGold += Rewards.BonusGold;
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
        TestTrue(TEXT("Every Continue retains the frozen generation rules"), FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&FrozenWeaponRules, &Fixture.Run->GetWeaponSkillRules(), 0));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunItemRewardAtomicTest, "ProjectA.Run.Target.ItemRewards.AtomicResultAndClaimRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunItemRewardAtomicTest::RunTest(const FString& Parameters)
{
    FTargetRunTransactionFixture Fixture;
    if (!TestTrue(TEXT("A normal weapon-based Target Run reaches its first synthetic PvE"), Fixture.InitializeEveryProfession() && Fixture.BeginNextSyntheticCombat())) return false;
    TStrongObjectPtr<URunSaveGame> BeforeResult(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!TestNotNull(TEXT("The preceding stable boundary remains durable before result generation"), BeforeResult.Get())) return false;
    // Synthetic combat has no confirmed turn checkpoint, so retain the stable file separately from its current public state.
    // 합성 전투에는 확정 턴 체크포인트가 없으므로 안정된 파일과 현재 공개 상태를 따로 보관합니다.
    BeforeResult->Phase = Fixture.Run->GetPhase();
    BeforeResult->Result = Fixture.Run->GetLastResult();
    BeforeResult->CurrentNode = Fixture.Run->GetCurrentNodeId();
    BeforeResult->CurrentEncounter = Fixture.Run->GetCurrentEncounterId();
    BeforeResult->GoldRewardState = Fixture.Run->GetGoldRewardState();
    const TArray<uint8> BeforeResultBytes = Fixture.ReadBytes();
    int32 Publications = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Publications]() { ++Publications; });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("An injected result write failure rejects the new item and gold offer"), Fixture.Run->CompleteEncounter(ECombatResult::Victory));
    TestTrue(TEXT("Failed result publication preserves every public value, previous save bytes and event count"), SameTargetTransactionState(*BeforeResult.Get(), *Fixture.Run.Get()) && BeforeResultBytes == Fixture.ReadBytes() && Publications == 0);
    // Inspect only the unpublished test fixture value; production does not expose a reroll or pending-reward API.
    // 테스트 fixture의 미공개 값만 조회하며 운영 코드에 재추첨·대기 보상 API를 노출하지 않습니다.
    const FStructProperty* PendingProperty = FindFProperty<FStructProperty>(URunStateSubsystem::StaticClass(), TEXT("PendingGoldRewardState"));
    if (!TestTrue(TEXT("The private pending reward is reflected with its expected structure"), PendingProperty && PendingProperty->Struct == FRunGoldRewardState::StaticStruct())) return false;
    const FRunGoldRewardState Pending = *PendingProperty->ContainerPtrToValuePtr<FRunGoldRewardState>(Fixture.Run.Get());
    if (!TestTrue(TEXT("The rejected result retains three generated copies and its common gold privately"), Pending.SchemaVersion == 2 && Pending.ItemChoices.Num() == 3 && Pending.BonusGold > 0)) return false;
    if (!TestTrue(TEXT("Result retry publishes the exact pending IDs grades skills and gold once"), Fixture.Run->CompleteEncounter(ECombatResult::Victory) && SameTargetRewards(Pending, Fixture.Run->GetGoldRewardState()) && Publications == 1)) return false;
    Fixture.Run->OnRunStateChanged.Clear();
    if (!Fixture.ReloadBoundary(*this)) return false;
    const FRunPartyMember Buyer = Fixture.Run->GetPartyMembers()[0];
    const FRunGoldRewardState Rewards = Fixture.Run->GetGoldRewardState();
    const TArray<uint8> BeforeClaimBytes = Fixture.ReadBytes();
    TStrongObjectPtr<URunSaveGame> BeforeClaim(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!BeforeClaim) return false;
    Publications = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Publications]() { ++Publications; });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("An injected claim write failure rejects the item gold and receipt together"), Fixture.Run->SelectGoldReward(Buyer.OwnerAccountId, Buyer.CharacterId, Rewards.NodeId, 1, Fixture.Error));
    TestTrue(TEXT("Failed claim preserves inventory skills equipment balance receipt offers and save bytes"), SameTargetTransactionState(*BeforeClaim.Get(), *Fixture.Run.Get()) && BeforeClaimBytes == Fixture.ReadBytes() && Publications == 0);
    if (!TestTrue(TEXT("The same claim retries with one successful publication"), Fixture.Run->SelectGoldReward(Buyer.OwnerAccountId, Buyer.CharacterId, Rewards.NodeId, 1, Fixture.Error) && Publications == 1)) return false;
    Fixture.Run->OnRunStateChanged.Clear();
    const FRunPartyMember& Awarded = Fixture.Run->GetPartyMembers()[0];
    TestTrue(TEXT("Retry adds exactly the selected frozen copy and common gold while preserving active equipment and skills"), Awarded.Items.Num() == Buyer.Items.Num() + 1 && RunItemShopCatalog::IsSameDefinition(Awarded.Items.Last(), Rewards.ItemChoices[1]) && Awarded.Gold == Buyer.Gold + Rewards.BonusGold && Awarded.Skills == Buyer.Skills && Awarded.InnateSkills == Buyer.InnateSkills && FRunEquipmentState::StaticStruct()->CompareScriptStruct(&Awarded.Equipment, &Buyer.Equipment, 0));
    TestTrue(TEXT("A successful claim leaves all three displayed choices unchanged and records only the recipient index"), Fixture.Run->GetGoldRewardState().Claims.Num() == 1 && Fixture.Run->GetGoldRewardState().Claims[0].CharacterId == Buyer.CharacterId && Fixture.Run->GetGoldRewardState().Claims[0].ChoiceIndex == 1 && Fixture.Run->GetGoldRewardState().BonusGold == Rewards.BonusGold);
    for (int32 Index = 0; Index < Rewards.ItemChoices.Num(); ++Index)
    {
        TestTrue(TEXT("The displayed card retains its original generated copy after claiming"), RunItemShopCatalog::IsSameDefinition(Rewards.ItemChoices[Index], Fixture.Run->GetGoldRewardState().ItemChoices[Index]));
    }
    if (!Fixture.ReloadBoundary(*this)) return false;
    TStrongObjectPtr<URunSaveGame> Claimed(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Claimed) return false;
    const TArray<uint8> ClaimedBytes = Fixture.ReadBytes();
    for (int32 Choice = 0; Choice < 3; ++Choice)
    {
        TestFalse(TEXT("No card can claim another item or gold after reloading the receipt"), Fixture.Run->SelectGoldReward(Buyer.OwnerAccountId, Buyer.CharacterId, Rewards.NodeId, Choice, Fixture.Error));
    }
    TestTrue(TEXT("Duplicate requests preserve the complete durable claim and allow Continue"), SameTargetTransactionState(*Claimed.Get(), *Fixture.Run.Get()) && ClaimedBytes == Fixture.ReadBytes() && Fixture.Run->CanContinueAfterRewards());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunItemRewardCompatibilityTest, "ProjectA.Run.Target.ItemRewards.LegacyGoldAndMalformedCopies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunItemRewardCompatibilityTest::RunTest(const FString& Parameters)
{
    FTargetRunTransactionFixture Fixture;
    if (!TestTrue(TEXT("A weapon-based target fixture creates its first PvE result"), Fixture.InitializeEveryProfession() && Fixture.BeginNextSyntheticCombat() && Fixture.Run->CompleteEncounter(ECombatResult::Victory))) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Legacy) return false;
    // Reconstruct a saved gold-only result without changing the existing Run's weapon acquisition contract.
    // 기존 Run의 무기 획득 계약을 바꾸지 않고 저장된 골드 전용 결과를 구성합니다.
    Legacy->GoldRewardState = FRunGoldRewardState();
    Legacy->GoldRewardState.SchemaVersion = 1;
    Legacy->GoldRewardState.NodeId = Legacy->CurrentNode;
    Legacy->GoldRewardState.GoldChoices = Legacy->TargetRun.Groups[0].GoldChoices;
    const FRunPartyMember Buyer = Legacy->Party[0];
    const FRunGoldRewardState OldOffers = Legacy->GoldRewardState;
    if (!TestTrue(TEXT("Already saved gold choices load unchanged on a weapon-based Run"), FRunCheckpointStorage::Save(Legacy.Get(), Fixture.Slot, Fixture.Error) && Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error) && SameTargetRewards(OldOffers, Fixture.Run->GetGoldRewardState()) && Fixture.ReloadBoundary(*this))) return false;
    if (!TestTrue(TEXT("A saved gold-only choice still grants its exact gold without adding an item"), Fixture.Run->SelectGoldReward(Buyer.OwnerAccountId, Buyer.CharacterId, OldOffers.NodeId, 2, Fixture.Error) && Fixture.ReloadBoundary(*this))) return false;
    const FRunGoldRewardState OldClaim = Fixture.Run->GetGoldRewardState();
    TestTrue(TEXT("The historical offer and receipt remain schema one with no retroactive item grant"), OldClaim.SchemaVersion == 1 && OldClaim.GoldChoices == OldOffers.GoldChoices && OldClaim.ItemChoices.IsEmpty() && OldClaim.BonusGold == 0 && OldClaim.Claims.Num() == 1 && OldClaim.Claims[0].ChoiceIndex == 2 && Fixture.Run->GetPartyMembers()[0].Items.Num() == Buyer.Items.Num() && Fixture.Run->GetPartyMembers()[0].Gold == Buyer.Gold + OldOffers.GoldChoices[2]);
    TestFalse(TEXT("The historical receipt also rejects duplicate collection"), Fixture.Run->SelectGoldReward(Buyer.OwnerAccountId, Buyer.CharacterId, OldOffers.NodeId, 0, Fixture.Error));
    if (!TestTrue(TEXT("Continue keeps the previous offer and receipt until the next combat begins"), Fixture.Run->ContinueRun() && SameTargetRewards(OldClaim, Fixture.Run->GetGoldRewardState()) && Fixture.ReloadBoundary(*this) && Fixture.BeginNextSyntheticCombat() && Fixture.Run->CompleteEncounter(ECombatResult::Victory) && Fixture.ReloadBoundary(*this))) return false;
    const FRunGoldRewardState Empty;
    TestTrue(TEXT("The intervening Snapshot offers no items gold recipients or claim requirement"), SameTargetRewards(Empty, Fixture.Run->GetGoldRewardState()) && Fixture.Run->GetGoldRewardRecipientIds().IsEmpty() && Fixture.Run->CanContinueAfterRewards() && Fixture.Run->GetPartyMembers()[0].Items.Num() == Buyer.Items.Num());
    if (!TestTrue(TEXT("Only the next PvE result adopts the new item and common-gold reward"), Fixture.Run->ContinueRun() && Fixture.BeginNextSyntheticCombat() && Fixture.Run->CompleteEncounter(ECombatResult::Victory) && Fixture.Run->GetGoldRewardState().SchemaVersion == 2 && Fixture.ReloadBoundary(*this))) return false;
    TStrongObjectPtr<URunSaveGame> Valid(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!TestTrue(TEXT("The new valid PvE save contains three frozen item copies"), Valid.IsValid() && Valid->GoldRewardState.ItemChoices.Num() == 3)) return false;
    const TArray<int32>& Range = Valid->TargetRun.Groups[1].GoldChoices;
    for (int32 Case = 0; Case < 5; ++Case)
    {
        TStrongObjectPtr<URunSaveGame> Invalid(DuplicateObject<URunSaveGame>(Valid.Get(), GetTransientPackage()));
        FRunGoldRewardState& Rewards = Invalid->GoldRewardState;
        if (Case == 0) Rewards.BonusGold = FMath::Min3(Range[0], Range[1], Range[2]) - 1;
        else if (Case == 1) Rewards.BonusGold = FMath::Max3(Range[0], Range[1], Range[2]) + 1;
        else if (Case == 2) Rewards.ItemChoices[1] = Rewards.ItemChoices[0];
        else if (Case == 3) Rewards.ItemChoices[0].GrantedSkills.Add(FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/UnknownRewardSkill.UnknownRewardSkill")));
        else
        {
            FRunGoldRewardClaim& Claim = Rewards.Claims.AddDefaulted_GetRef();
            Claim.CharacterId = Buyer.CharacterId;
            Claim.ChoiceIndex = 0;
        }
        if (!TestTrue(TEXT("A malformed disposable reward fixture can be serialized for the loader"), FRunCheckpointStorage::Save(Invalid.Get(), Fixture.Slot, Fixture.Error))) return false;
        const TArray<uint8> InvalidBytes = Fixture.ReadBytes();
        TestFalse(TEXT("Loading rejects out-of-range gold duplicate copies unknown skills and a receipt without its item"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error));
        TestTrue(TEXT("Rejected loading preserves public progression inventory gold offers and the original file"), SameTargetTransactionState(*Valid.Get(), *Fixture.Run.Get()) && InvalidBytes == Fixture.ReadBytes());
    }
    return TestTrue(TEXT("Restoring the valid frozen reward fixture allows a fresh reload"), FRunCheckpointStorage::Save(Valid.Get(), Fixture.Slot, Fixture.Error) && Fixture.ReloadBoundary(*this));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunWeaponTransactionsTest, "ProjectA.Run.Target.WeaponCopiesPurchaseEquipmentAtomicAndContinue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunWeaponTransactionsTest::RunTest(const FString& Parameters)
{
    FTargetRunTransactionFixture Fixture;
    if (!TestTrue(TEXT("Every profession initializes with the new frozen weapon contract"), Fixture.InitializeEveryProfession() && Fixture.FreezeInitialBasicShop() && Fixture.ReloadBoundary(*this))) return false;
    const FRunWeaponSkillRulesState Rules = Fixture.Run->GetWeaponSkillRules();
    TestTrue(TEXT("The delegated trial data freezes one skill and five equally weighted grades"), Fixture.Run->UsesWeaponSkills() && Rules.SchemaVersion == 1 && Rules.SkillCount == 1 && Rules.Rarities.Num() == 5 && Rules.Rarities.ContainsByPredicate([](const FRunWeaponRarityRule& Rarity) { return Rarity.BaseWeight == 1.f; }));
    for (const FRunWeaponRarityRule& Rarity : Rules.Rarities) TestEqual(TEXT("Each development grade has the same weight"), Rarity.BaseWeight, 1.f);
    TSet<FGuid> StartingIds;
    const FSoftObjectPath Unarmed = Fixture.Run->PartyDefinition->UnarmedStartingSkill.ToSoftObjectPath();
    for (const FRunPartyMember& Member : Fixture.Run->GetPartyMembers())
    {
        if (!TestTrue(TEXT("Every profession retains unarmed plus its equipped starting weapon skill"), Member.InnateSkills == TArray<FSoftObjectPath>{Unarmed} && Member.Skills.Num() == 2 && Member.Skills[0] == Unarmed && !Member.Items.IsEmpty())) return false;
        for (const FRunItemDefinition& Item : Member.Items)
        {
            TestTrue(TEXT("All starting copies have independent persistent identities and tag-compatible fixed results"), Item.GenerationVersion == 1 && !StartingIds.Contains(Item.ItemInstanceId) && RunWeaponSkillRules::ValidateGeneratedCopy(Item, Rules, Fixture.Error));
            TestEqual(TEXT("Only a matching starting weapon receives the delegated skill count"), Item.GrantedSkills.Num(), Rules.WeaponQuery.Matches(Item.Tags) ? 1 : 0);
            StartingIds.Add(Item.ItemInstanceId);
            for (const FSoftObjectPath& Skill : Item.GrantedSkills) TestTrue(TEXT("An equipped starting weapon exposes its fixed skill"), Member.Skills.Contains(Skill));
        }
    }
    const FRunEncounterOffer* ItemVisit = Fixture.Run->GetEncounterProgress().Offers.FindByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag() == FRunEncounterOffer::GetBasicItemShopTag(); });
    if (!TestTrue(TEXT("The deterministic fixture opens its initial basic shop"), ItemVisit && Fixture.Run->SelectRunEncounter(ItemVisit->EncounterId) && Fixture.ReloadBoundary(*this))) return false;
    const FRunItemShopState Displayed = Fixture.Run->GetItemShopState();
    if (!TestEqual(TEXT("The new shop displays five generated fixed copies"), Displayed.Offers.Num(), 5)) return false;
    const FRunItemShopOffer Offer = Displayed.Offers[0];
    const FRunPartyMember Buyer = Fixture.Run->GetPartyMembers()[0];
    const TArray<FRunPartyMember> BeforePurchase = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    int32 Publications = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Publications]() { ++Publications; });
    FRunAccountId Stranger = Buyer.OwnerAccountId;
    Stranger.Subject += TEXT("-weapon-stranger");
    TestFalse(TEXT("Generated items retain original buyer ownership checks"), Fixture.Run->PurchaseShopOffer(Stranger, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Displayed.Revision));
    TestFalse(TEXT("An AI companion cannot purchase a generated copy"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, BeforePurchase[1].CharacterId, Offer.OfferId, Fixture.Error, Displayed.Revision));
    TestFalse(TEXT("A stale displayed-copy revision cannot purchase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Displayed.Revision - 1));
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed generated-item purchase rejects its durable write"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Displayed.Revision));
    TestTrue(TEXT("Purchase failure preserves exact displayed results stock party balances bytes and publication count"), BeforeBytes == Fixture.ReadBytes() && SameTargetTransactionParty(BeforePurchase, Fixture.Run->GetPartyMembers()) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&Displayed, &Fixture.Run->GetItemShopState(), 0) && Publications == 0);
    if (!TestTrue(TEXT("The same generated-copy purchase can retry once"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Displayed.Revision) && Publications == 1)) return false;
    Fixture.Run->OnRunStateChanged.Clear();
    const FRunPartyMember Purchased = Fixture.Run->GetPartyMembers()[0];
    TestTrue(TEXT("Buying copies the displayed ID grade and skill result without granting bag skills"), Purchased.Items.Num() == Buyer.Items.Num() + 1 && RunItemShopCatalog::IsSameDefinition(Purchased.Items.Last(), Offer.Item) && Purchased.Skills == Buyer.Skills && Purchased.Gold == Buyer.Gold - Offer.Item.Price && Offer.Item.Price == 1);
    TestFalse(TEXT("An accepted displayed copy cannot be bought twice"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetItemShopState().Revision));
    if (!Fixture.ReloadBoundary(*this)) return false;
    const FRunPartyMember Equipped = Fixture.Run->GetPartyMembers()[0];
    const FRunEquipmentSlot* WeaponSlot = Equipped.Equipment.Slots.FindByPredicate([&Equipped](const FRunEquipmentSlot& Slot) { return !Equipped.Items[Slot.ItemIndex].GrantedSkills.IsEmpty(); });
    if (!TestNotNull(TEXT("The initial human owns an equipped skill-bearing weapon"), WeaponSlot)) return false;
    const FGameplayTag OriginalSlot = WeaponSlot->SlotTag;
    FRunEquipmentCommand Command;
    Command.CharacterId = Buyer.CharacterId;
    Command.ItemIndex = WeaponSlot->ItemIndex;
    Command.ExpectedRevision = Equipped.Equipment.Revision;
    const TArray<FRunPartyMember> BeforeUnequip = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeUnequipBytes = Fixture.ReadBytes();
    Publications = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Publications]() { ++Publications; });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed unequip cannot revoke a weapon skill"), Fixture.Run->ChangeEquipment(Buyer.OwnerAccountId, Command, Fixture.Error));
    TestTrue(TEXT("Equipment failure preserves every owned result skill revision save byte and publication"), BeforeUnequipBytes == Fixture.ReadBytes() && SameTargetTransactionParty(BeforeUnequip, Fixture.Run->GetPartyMembers()) && Publications == 0);
    if (!TestTrue(TEXT("Unequip retry atomically removes the final weapon source"), Fixture.Run->ChangeEquipment(Buyer.OwnerAccountId, Command, Fixture.Error) && Publications == 1 && Fixture.Run->GetPartyMembers()[0].Skills == Equipped.InnateSkills)) return false;
    TestFalse(TEXT("An accepted equipment revision cannot execute again"), Fixture.Run->ChangeEquipment(Buyer.OwnerAccountId, Command, Fixture.Error));
    Fixture.Run->OnRunStateChanged.Clear();
    if (!Fixture.ReloadBoundary(*this)) return false;
    const FRunPartyMember Unequipped = Fixture.Run->GetPartyMembers()[0];
    TestTrue(TEXT("Unequip preserves the copy result independently of its active skill right"), RunItemShopCatalog::IsSameDefinition(Unequipped.Items[Command.ItemIndex], Equipped.Items[Command.ItemIndex]) && Unequipped.Gold == Equipped.Gold);
    Command.TargetSlot = OriginalSlot;
    Command.ExpectedRevision = Unequipped.Equipment.Revision;
    if (!TestTrue(TEXT("Re-equipping the same copy restores its original fixed skill"), Fixture.Run->ChangeEquipment(Buyer.OwnerAccountId, Command, Fixture.Error) && Fixture.Run->GetPartyMembers()[0].Skills == Equipped.Skills && Fixture.ReloadBoundary(*this))) return false;
    const FRunItemShopState BeforeReroll = Fixture.Run->GetItemShopState();
    const TArray<FRunPartyMember> BeforeRerollParty = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeRerollBytes = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed item reroll cannot replace fixed displayed copies"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunItemShopState::GetRerollOfferId(), Fixture.Error, BeforeReroll.Revision));
    TestTrue(TEXT("Reroll failure retains saved displayed copies and buyer balance"), BeforeRerollBytes == Fixture.ReadBytes() && SameTargetTransactionParty(BeforeRerollParty, Fixture.Run->GetPartyMembers()) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&BeforeReroll, &Fixture.Run->GetItemShopState(), 0));
    if (!TestTrue(TEXT("A successful reroll publishes new generated copies with the frozen rules"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunItemShopState::GetRerollOfferId(), Fixture.Error, BeforeReroll.Revision) && Fixture.ReloadBoundary(*this))) return false;
    for (const FRunItemShopOffer& NewOffer : Fixture.Run->GetItemShopState().Offers) TestFalse(TEXT("A reroll creates fresh display identities while acquired copies remain owned"), BeforeReroll.Offers.ContainsByPredicate([&NewOffer](const FRunItemShopOffer& OldOffer) { return OldOffer.Item.ItemInstanceId == NewOffer.Item.ItemInstanceId; }));
    TestTrue(TEXT("Continue preserves frozen rules and the purchased copy through every transaction"), FRunWeaponSkillRulesState::StaticStruct()->CompareScriptStruct(&Rules, &Fixture.Run->GetWeaponSkillRules(), 0) && RunItemShopCatalog::IsSameDefinition(Fixture.Run->GetPartyMembers()[0].Items.Last(), Offer.Item));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetRunLegacyAcquisitionTest, "ProjectA.Run.Target.LegacyAcquiredSkillsAndShopsRemainFrozen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTargetRunLegacyAcquisitionTest::RunTest(const FString& Parameters)
{
    FTargetRunTransactionFixture Fixture;
    if (!Fixture.InitializeEveryProfession()) return false;
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Save) return false;
    // Reconstruct the historical Target acquisition contract only in a disposable saved fixture.
    // 일회성 저장 fixture에만 기존 Target 획득 계약을 구성합니다.
    Save->WeaponSkillAcquisitionVersion = 0;
    Save->WeaponSkillRules = FRunWeaponSkillRulesState();
    Save->TargetRun.EncounterSelectionVersion = 0;
    Save->TargetRun.EncounterSeed = 0;
    Save->ItemShopState.SelectionVersion = 0;
    const URunEncounterPoolDataAsset* Pool = Fixture.Run->PartyDefinition->RunEncounterPool ? Fixture.Run->PartyDefinition->RunEncounterPool.Get() : GetDefault<URunEncounterPoolDataAsset>();
    if (!Pool->BuildSkillShop(Save->SkillShopState, Fixture.Error) || Save->SkillShopState.Catalog.Num() < 5) return false;
    const UTargetRunDefinitionDataAsset* Definition = Fixture.Run->PartyDefinition->TargetRunDefinition ? Fixture.Run->PartyDefinition->TargetRunDefinition.Get() : GetDefault<UTargetRunDefinitionDataAsset>();
    Save->TargetRun.EncounterPool = Definition->EncounterPool;
    Save->TargetRun.EncounterQuery = Definition->EncounterQuery;
    if (!UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, 0, 0, Save->EncounterProgress.Offers)) return false;
    for (FRunPartyMember& Member : Save->Party)
    {
        Member.Skills = Member.InnateSkills;
        Member.InnateSkills.Reset();
        for (int32 Index = 0; Index < 5; ++Index) Member.Skills.Add(Save->SkillShopState.Catalog[Index].Skill);
        for (FRunItemDefinition& Item : Member.Items)
        {
            const FRunItemDefinition* Base = Save->ItemShopState.Catalog.FindByPredicate([&Item](const FRunItemDefinition& Candidate) { return Candidate.Asset == Item.Asset; });
            if (!Base) return false;
            Item = *Base;
        }
    }
    if (!TestTrue(TEXT("The historical Target save is accepted without adopting the new acquisition contract"), FRunCheckpointStorage::Save(Save.Get(), Fixture.Slot, Fixture.Error) && Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error) && Fixture.ReloadBoundary(*this))) return false;
    TestTrue(TEXT("Legacy Continue preserves six acquired skills and its original skill shop"), !Fixture.Run->UsesWeaponSkills() && Fixture.Run->GetWeaponSkillRules().SchemaVersion == 0 && Fixture.Run->GetSkillShopState().SchemaVersion == 1 && SameTargetTransactionParty(Save->Party, Fixture.Run->GetPartyMembers()));
    const FRunEncounterOffer* SkillVisit = Fixture.Run->GetEncounterProgress().Offers.FindByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag().MatchesTag(FRunEncounterOffer::GetSkillShopTag()); });
    if (!TestTrue(TEXT("An already saved legacy Target still opens its original skill shop"), SkillVisit && Fixture.Run->SelectRunEncounter(SkillVisit->EncounterId) && Fixture.ReloadBoundary(*this))) return false;
    TestEqual(TEXT("Historical skill-shop display still contains five candidates"), Fixture.Run->GetSkillShopState().Offers.Num(), 5);
    TestTrue(TEXT("Opening the old shop never replaces acquired skills with equipment-derived skills"), SameTargetTransactionParty(Save->Party, Fixture.Run->GetPartyMembers()));
    const FRunPartyMember Buyer = Fixture.Run->GetPartyMembers()[0];
    if (!TestTrue(TEXT("The legacy human still has starting equipment"), !Buyer.Equipment.Slots.IsEmpty())) return false;
    FRunEquipmentCommand Command;
    Command.CharacterId = Buyer.CharacterId;
    Command.ItemIndex = Buyer.Equipment.Slots[0].ItemIndex;
    Command.ExpectedRevision = Buyer.Equipment.Revision;
    TestTrue(TEXT("Legacy unequip keeps its original acquired skill rights"), Fixture.Run->ChangeEquipment(Buyer.OwnerAccountId, Command, Fixture.Error) && Fixture.Run->GetPartyMembers()[0].Skills == Buyer.Skills && Fixture.ReloadBoundary(*this));
    return true;
}

#endif
