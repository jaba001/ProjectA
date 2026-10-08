#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Tests/RunRewardTestHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FSkillShopFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("SkillShop_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FText Error;

        FSkillShopFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
        }

        ~FSkillShopFixture()
        {
            Run->OnRunStateChanged.Clear();
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        bool Initialize(int32 SelectedSlot = 3, bool bSave = false)
        {
            if (!Run->PartyDefinition) return false;
            if (bSave) Run->EnableCheckpointSaving(Slot);
            TArray<FRunPartyMember> Members;
            const TArray<FName> Professions{TEXT("Warrior"), TEXT("Archer"), TEXT("Mage"), TEXT("Rogue")};
            for (int32 Index = 0; Index < Professions.Num(); ++Index)
            {
                FRunPartyMember& Member = Members.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.ClassId = Professions[Index];
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Shop Character %d"), Index));
                Member.bCreated = true;
                Member.bPlayerControlled = Index == SelectedSlot;
                Member.Gold = 999;
                Member.bHasSkillLoadout = true;
            }
            return Run->InitializeRun(Members, Error) && Run->GetSaveError().IsEmpty();
        }

        bool ReachShop(FName ShopId = TEXT("Shop_01"), int32 DeadSlot = INDEX_NONE, float RemainingHP = 70.0f)
        {
            if (!Run->BeginEncounter(TEXT("Combat_01")) || !Run->MarkCombatStarted()) return false;
            for (const FRunPartyMember& Member : Run->GetPartyMembers()) Run->UpdatePartyMemberHP(Member.SlotIndex, Member.SlotIndex == DeadSlot ? 0.0f : RemainingHP);
            return Run->CompleteEncounter(ECombatResult::Victory) && RunRewardTests::CollectPendingGoldRewards(Run.Get()) && Run->ContinueRun() && Run->SelectRunEncounter(ShopId);
        }

        const FRunPartyMember& Member(int32 SlotIndex) const { return Run->GetPartyMembers()[SlotIndex]; }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
            return Bytes;
        }
    };

    bool SameShopParty(const TArray<FRunPartyMember>& Left, const TArray<FRunPartyMember>& Right)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], 0)) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopLoadoutTest, "ProjectA.Run.Shop.UnarmedStartAndEveryProfession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopLoadoutTest::RunTest(const FString& Parameters)
{
    for (int32 SelectedSlot = 0; SelectedSlot < 4; ++SelectedSlot)
    {
        for (int32 ShopIndex : {1, 3})
        {
            FSkillShopFixture Fixture;
            if (!TestTrue(TEXT("Every direct-control profession initializes a durable Run"), Fixture.Initialize(SelectedSlot, true))) return false;
            const FSoftObjectPath Unarmed = Fixture.Run->PartyDefinition->UnarmedStartingSkill.ToSoftObjectPath();
            for (const FRunPartyMember& Member : Fixture.Run->GetPartyMembers())
            {
                TestTrue(TEXT("All allies start with only the shared unarmed attack"), Member.bHasSkillLoadout && Member.Skills.Num() == 1 && Member.Skills[0] == Unarmed);
                TestEqual(TEXT("Only the selected character receives the fresh 10G balance"), Member.Gold, Member.SlotIndex == SelectedSlot ? 10 : 0);
            }
            if (!TestTrue(TEXT("Each shop opens after the first victory"), Fixture.ReachShop(FName(*FString::Printf(TEXT("Shop_%02d"), ShopIndex)), INDEX_NONE, 1.0f))) return false;
            const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
            const FRunIdentityData Identity = Fixture.Run->GetRunIdentity();
            const FGuid CharacterId = Fixture.Member(SelectedSlot).CharacterId;
            const FRunAccountId Account = Fixture.Member(SelectedSlot).OwnerAccountId;
            const TArray<FRunSkillShopOffer> Offers = Fixture.Run->GetSkillShopState().Offers;
            if (!TestEqual(TEXT("Every current skill shop displays five distinct offers"), Offers.Num(), 5)) return false;
            TSet<FName> DisplayedSkillIds;
            int32 PurchasedCount = 0;
            for (int32 Index = 0; Index < Offers.Num(); ++Index)
            {
                const FRunSkillShopOffer& Offer = Offers[Index];
                TestEqual(TEXT("Every prototype skill costs 1G"), Offer.Price, 1);
                TestFalse(TEXT("Products never sell the free unarmed attack"), Offer.Skill == Unarmed);
                const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
                FCombatRoundSkill Definition;
                if (!TestTrue(TEXT("Every displayed skill resolves to valid runtime content"), IsValid(Skill) && Skill->ResolveRoundSkill(Definition, Fixture.Error))) return false;
                TestFalse(TEXT("Displayed skill identities contain no duplicates"), DisplayedSkillIds.Contains(Definition.SkillId));
                DisplayedSkillIds.Add(Definition.SkillId);
                const bool bPurchased = Fixture.Run->PurchaseShopOffer(Account, CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision);
                TestTrue(TEXT("Every profession can purchase every distinct offer beyond five owned skills"), bPurchased);
                if (bPurchased) ++PurchasedCount;
                TestFalse(TEXT("Repeated requests cannot purchase an already learned skill"), Fixture.Run->PurchaseShopOffer(Account, CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
            }
            TestEqual(TEXT("Only successful purchases consume 1G each"), Fixture.Member(SelectedSlot).Gold, Before[SelectedSlot].Gold - PurchasedCount);
            TestEqual(TEXT("The buyer retains unarmed and all five purchased skills"), Fixture.Member(SelectedSlot).Skills.Num(), 6);
            FProfessionDefinition Profession;
            if (!Fixture.Run->PartyDefinition->ResolveProfession(Fixture.Member(SelectedSlot).ClassId, Profession)) return false;
            TestTrue(TEXT("Every shop can recover every profession with six owned skills"), Fixture.Run->PurchaseShopOffer(Account, CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
            TestTrue(TEXT("Recovery restores all HP for 1G without changing the purchased loadout"), Fixture.Member(SelectedSlot).CurrentHP == Profession.MaxHP && Fixture.Member(SelectedSlot).Gold == Before[SelectedSlot].Gold - PurchasedCount - 1 && Fixture.Member(SelectedSlot).Skills.Num() == 6);
            for (int32 Index = 0; Index < Before.Num(); ++Index)
            {
                if (Index != SelectedSlot) TestTrue(TEXT("Companion HP balance and loadout remain unchanged"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Before[Index], &Fixture.Member(Index), 0));
            }
            TestTrue(TEXT("Purchases preserve all participant and host identity fields"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Fixture.Run->GetRunIdentity(), 0));
            TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
            Restored->EnableCheckpointSaving(Fixture.Slot);
            if (!TestTrue(TEXT("Standalone Continue accepts six owned skills without changing their order or identity"), Restored->LoadStandaloneCheckpoint(Fixture.Error) && SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Restored->GetRunIdentity(), 0))) return false;
            TestTrue(TEXT("The next battle starts with all six restored skills"), Restored->LeaveRunEncounter() && Restored->BeginEncounter(TEXT("Combat_02")) && Restored->MarkCombatStarted());
            TArray<TObjectPtr<USkillDefinitionDataAsset>> ResolvedSkills;
            TestTrue(TEXT("Spawn resolution includes all six restored skills"), Restored->PartyDefinition->ResolveMemberSkills(Restored->GetPartyMembers()[SelectedSlot], ResolvedSkills, Fixture.Error) && ResolvedSkills.Num() == 6);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopRejectionTest, "ProjectA.Run.Shop.RejectedPurchasePreservesState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopRejectionTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!TestTrue(TEXT("The rejection fixture saves its initial Run"), Fixture.Initialize(3, true))) return false;
    const FRunPartyMember Buyer = Fixture.Member(3);
    const FName InitialOfferId = Fixture.Run->GetSkillShopState().Offers[0].OfferId;
    TestFalse(TEXT("A purchase cannot bypass the Map phase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, InitialOfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Recovery cannot bypass the Map phase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    if (!TestTrue(TEXT("The rejection fixture enters a shop"), Fixture.ReachShop())) return false;
    const FName OfferId = Fixture.Run->GetSkillShopState().Offers[0].OfferId;
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const TArray<uint8> DiskBefore = Fixture.ReadBytes();
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    FRunAccountId Outsider = Buyer.OwnerAccountId;
    Outsider.Subject += TEXT("_Other");
    TestFalse(TEXT("A foreign account cannot buy for the selected character"), Fixture.Run->PurchaseShopOffer(Outsider, Buyer.CharacterId, OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("A foreign account cannot recover the selected character"), Fixture.Run->PurchaseShopOffer(Outsider, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("The local owner cannot redirect a purchase to an AI companion"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Fixture.Member(0).CharacterId, OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Recovery cannot be redirected to an AI companion"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Fixture.Member(0).CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Unknown characters are rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, FGuid::NewGuid(), OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Unknown products are rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, TEXT("Missing"), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("All rejected requests preserve complete party data and durable bytes"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && DiskBefore == Fixture.ReadBytes());
    TestEqual(TEXT("Rejected requests publish no state change"), Events, 0);
    Fixture.Run->OnRunStateChanged.Clear();

    FSkillShopFixture DeadFixture;
    if (!DeadFixture.Initialize() || !DeadFixture.ReachShop(TEXT("Shop_01"), 3)) return false;
    const FRunPartyMember Dead = DeadFixture.Member(3);
    TestFalse(TEXT("A dead direct-control character cannot purchase"), DeadFixture.Run->PurchaseShopOffer(Dead.OwnerAccountId, Dead.CharacterId, DeadFixture.Run->GetSkillShopState().Offers[0].OfferId, DeadFixture.Error, DeadFixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Shop recovery cannot resurrect a dead character"), DeadFixture.Run->PurchaseShopOffer(Dead.OwnerAccountId, Dead.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), DeadFixture.Error, DeadFixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("The rejected dead character retains its gold and unarmed loadout"), DeadFixture.Member(3).Gold == Dead.Gold && DeadFixture.Member(3).Skills.Num() == 1);

    FSkillShopFixture PoorFixture;
    TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog(DuplicateObject<UPartyDefinitionDataAsset>(PoorFixture.Run->PartyDefinition.Get(), GetTransientPackage()));
    TStrongObjectPtr<URunEncounterPoolDataAsset> Pool(NewObject<URunEncounterPoolDataAsset>());
    Pool->StartingGold = 0;
    for (FRunSkillShopOffer& Offer : Pool->FixedSkillOffers) Offer.Price = 100;
    Pool->Recovery.Price = 100;
    Catalog->RunEncounterPool = Pool.Get();
    PoorFixture.Run->PartyDefinition = Catalog.Get();
    if (!PoorFixture.Initialize(3, true) || !PoorFixture.ReachShop(TEXT("Shop_03"))) return false;
    TStrongObjectPtr<URunSaveGame> EmptyBalance(Cast<URunSaveGame>(FRunCheckpointStorage::Load(PoorFixture.Slot, PoorFixture.Error)));
    if (!EmptyBalance) return false;
    EmptyBalance->Party[3].Gold = 0;
    if (!FRunCheckpointStorage::Save(EmptyBalance.Get(), PoorFixture.Slot, PoorFixture.Error) || !PoorFixture.Run->LoadStandaloneCheckpoint(PoorFixture.Error)) return false;
    const FRunPartyMember Poor = PoorFixture.Member(3);
    TestFalse(TEXT("An authored insufficient balance rejects a purchase"), PoorFixture.Run->PurchaseShopOffer(Poor.OwnerAccountId, Poor.CharacterId, PoorFixture.Run->GetSkillShopState().Offers[0].OfferId, PoorFixture.Error, PoorFixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Insufficient funds also reject recovery"), PoorFixture.Run->PurchaseShopOffer(Poor.OwnerAccountId, Poor.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), PoorFixture.Error, PoorFixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("Insufficient funds neither subtract gold nor teach a skill"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Poor, &PoorFixture.Member(3), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopAtomicPersistenceTest, "ProjectA.Run.Shop.AtomicPurchaseAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopAtomicPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!TestTrue(TEXT("The purchase fixture reaches a durable shop"), Fixture.Initialize(3, true) && Fixture.ReachShop())) return false;
    const TArray<FRunSkillShopOffer> Offers = Fixture.Run->GetSkillShopState().Offers;
    for (int32 Index = 1; Index < Offers.Num(); ++Index)
    {
        const FRunPartyMember CurrentBuyer = Fixture.Member(3);
        if (!TestTrue(TEXT("The durable buyer learns the other current products before the sixth-skill transaction"), Fixture.Run->PurchaseShopOffer(CurrentBuyer.OwnerAccountId, CurrentBuyer.CharacterId, Offers[Index].OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision))) return false;
    }
    if (!TestEqual(TEXT("The atomic purchase crosses the former five-skill boundary"), Fixture.Member(3).Skills.Num(), 5)) return false;
    const FRunPartyMember Buyer = Fixture.Member(3);
    const FRunSkillShopOffer Offer = Fixture.Run->GetSkillShopState().Offers[0];
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    const FRunIdentityData Identity = Fixture.Run->GetRunIdentity();
    int32 Events = 0;
    bool bEventObservedDurablePurchase = false;
    Fixture.Run->OnRunStateChanged.AddLambda([&Fixture, &Events, &bEventObservedDurablePurchase, &Offer, &Buyer]()
    {
        ++Events;
        TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        bEventObservedDurablePurchase = Durable && Durable->Party[3].Gold == Buyer.Gold - 1 && Durable->Party[3].Skills.Contains(Offer.Skill) && Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).Skills.Contains(Offer.Skill) && SameShopParty(Durable->Party, Fixture.Run->GetPartyMembers());
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed durable write rejects the purchase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("Failed writes preserve every member and the complete original file"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && BeforeBytes == Fixture.ReadBytes());
    TestEqual(TEXT("Failed persistence emits no purchase notification"), Events, 0);
    TestTrue(TEXT("The same purchase can retry after storage recovers"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("The single success notification observes all six committed skills in memory and disk"), Events == 1 && bEventObservedDurablePurchase && Fixture.Member(3).Skills.Num() == 6);
    TestFalse(TEXT("A retried successful request cannot charge twice"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestEqual(TEXT("A duplicate request adds no notification"), Events, 1);
    Fixture.Run->OnRunStateChanged.Clear();
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Standalone Continue restores the purchased shop"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Restore preserves ownership identity balances and all skill paths"), SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Restored->GetRunIdentity(), 0));
    TestTrue(TEXT("Restore preserves the frozen catalog"), FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetSkillShopState(), &Restored->GetSkillShopState(), 0));
    TestFalse(TEXT("A reloaded purchase remains owned"), Restored->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Restored->GetSkillShopState().Revision));
    TestTrue(TEXT("Purchased skills survive shop exit and the next combat"), Restored->LeaveRunEncounter() && Restored->BeginEncounter(TEXT("Combat_02")) && Restored->MarkCombatStarted() && Restored->GetPartyMembers()[3].Skills.Contains(Offer.Skill));
    TestEqual(TEXT("The next encounter never grants another starting balance"), Restored->GetPartyMembers()[3].Gold, Buyer.Gold - 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunShopRecoveryPersistenceTest, "ProjectA.Run.Shop.RecoveryAtomicPurchaseAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunShopRecoveryPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!TestTrue(TEXT("The recovery fixture creates a durable Run"), Fixture.Initialize(3, true))) return false;
    FProfessionDefinition Profession;
    if (!Fixture.Run->PartyDefinition->ResolveProfession(Fixture.Member(3).ClassId, Profession)) return false;
    const float WoundedHP = Profession.MaxHP * 0.2f;
    if (!TestTrue(TEXT("A wounded owner reaches the third shop"), Fixture.ReachShop(TEXT("Shop_03"), INDEX_NONE, WoundedHP))) return false;
    const FRunPartyMember Buyer = Fixture.Member(3);
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    const FRunIdentityData Identity = Fixture.Run->GetRunIdentity();
    const FRunSkillShopState Catalog = Fixture.Run->GetSkillShopState();
    TestEqual(TEXT("The recovery service costs 1G"), Catalog.Recovery.Price, 1);
    int32 Events = 0;
    bool bEventObservedDurableRecovery = false;
    Fixture.Run->OnRunStateChanged.AddLambda([&Fixture, &Events, &bEventObservedDurableRecovery, &Profession, &Buyer]()
    {
        ++Events;
        TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        bEventObservedDurableRecovery = Durable && Durable->Party[3].Gold == Buyer.Gold - 1 && Durable->Party[3].CurrentHP == Profession.MaxHP && Durable->Party[3].Skills == Buyer.Skills && Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).CurrentHP == Profession.MaxHP;
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Recovery rejects a failed durable write"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("Recovery storage failure supplies a visible reason"), Fixture.Error.IsEmpty());
    TestTrue(TEXT("Failed recovery preserves all HP gold skills and disk bytes"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && BeforeBytes == Fixture.ReadBytes());
    TestEqual(TEXT("Failed recovery publishes no state change"), Events, 0);
    if (!TestTrue(TEXT("The same recovery succeeds after storage recovers"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision))) return false;
    TestTrue(TEXT("Recovery notification observes HP and gold committed together"), Events == 1 && bEventObservedDurableRecovery);
    for (int32 Index = 0; Index < Before.Num(); ++Index)
    {
        if (Index != 3) TestTrue(TEXT("Recovery leaves every companion unchanged"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Before[Index], &Fixture.Member(Index), 0));
    }
    const TArray<uint8> RecoveredBytes = Fixture.ReadBytes();
    TestFalse(TEXT("A repeated request at full HP cannot charge again"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("Full HP rejection preserves balance save bytes and notification count"), Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).CurrentHP == Profession.MaxHP && RecoveredBytes == Fixture.ReadBytes() && Events == 1);
    Fixture.Run->OnRunStateChanged.Clear();
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Continue restores the recovered shop"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    FRunSkillShopState ExpectedShop = Catalog;
    ++ExpectedShop.Revision;
    TestTrue(TEXT("Recovery reload preserves party identity and frozen skill products with the committed revision"), SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Restored->GetRunIdentity(), 0) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&ExpectedShop, &Restored->GetSkillShopState(), 0));
    TestTrue(TEXT("Recovered HP persists into the next battle without refilling gold"), Restored->LeaveRunEncounter() && Restored->BeginEncounter(TEXT("Combat_02")) && Restored->MarkCombatStarted() && Restored->GetPartyMembers()[3].CurrentHP == Profession.MaxHP && Restored->GetPartyMembers()[3].Gold == Buyer.Gold - 1 && Restored->GetPartyMembers()[3].Skills == Buyer.Skills);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopRerollPersistenceTest, "ProjectA.Run.Shop.SkillRerollEscalationAtomicWriteAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopRerollPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true) || !Fixture.ReachShop()) return false;
    const FRunPartyMember Buyer = Fixture.Member(3);
    const FRunSkillShopState Initial = Fixture.Run->GetSkillShopState();
    const TArray<FRunPartyMember> BeforeParty = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    TestEqual(TEXT("New skill shops freeze melee and all sixty current VFX skills"), Initial.Catalog.Num(), 61);
    TestEqual(TEXT("Entering a skill shop resets the first reroll to 1G"), Initial.RerollPrice, 1);
    TestTrue(TEXT("New skill-shop stock carries a positive revision"), Initial.Revision > 0);
    const auto CheckOffers = [this, &Fixture](const FRunSkillShopState& State)
    {
        if (!TestEqual(TEXT("Every skill reroll displays five distinct current products"), State.Offers.Num(), 5)) return false;
        TSet<FName> SkillIds;
        TSet<FName> OfferIds;
        for (const FRunSkillShopOffer& Offer : State.Offers)
        {
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
            FCombatRoundSkill Definition;
            if (!TestTrue(TEXT("Rerolled products resolve through their existing skill assets"), IsValid(Skill) && Skill->ResolveRoundSkill(Definition, Fixture.Error))) return false;
            TestFalse(TEXT("Displayed skill identities remain unique"), SkillIds.Contains(Definition.SkillId));
            TestFalse(TEXT("Displayed offer identities remain unique"), OfferIds.Contains(Offer.OfferId));
            TestTrue(TEXT("Rerolled products preserve their runtime tags and selection conditions"), Offer.Tags == Definition.EffectTags && (State.Query.IsEmpty() || State.Query.Matches(Offer.Tags)));
            TestTrue(TEXT("Every displayed skill belongs to the frozen catalog"), State.Catalog.ContainsByPredicate([&Offer](const FRunSkillShopOffer& Product) { return Product.Skill == Offer.Skill && Product.Price == Offer.Price && Product.DisplayName.ToString() == Offer.DisplayName.ToString() && Product.Tags == Offer.Tags && Product.BaseWeight == Offer.BaseWeight; }));
            SkillIds.Add(Definition.SkillId);
            OfferIds.Add(Offer.OfferId);
        }
        return true;
    };
    if (!CheckOffers(Initial)) return false;
    TestFalse(TEXT("A skill reroll without the displayed revision is rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error));
    TestFalse(TEXT("A stale skill reroll cannot spend gold"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, Initial.Revision - 1));
    TestFalse(TEXT("An AI companion cannot fund a shared skill reroll"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Fixture.Member(0).CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, Initial.Revision));
    int32 Events = 0;
    bool bDurableAtNotification = false;
    Fixture.Run->OnRunStateChanged.AddLambda([&Fixture, &Events, &bDurableAtNotification]()
    {
        ++Events;
        TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        bDurableAtNotification = Durable && SameShopParty(Durable->Party, Fixture.Run->GetPartyMembers()) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Durable->SkillShopState, &Fixture.Run->GetSkillShopState(), 0);
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed skill reroll write rejects the entire transaction"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, Initial.Revision));
    TestTrue(TEXT("Failed skill rerolls preserve gold stock price revision and durable bytes"), SameShopParty(BeforeParty, Fixture.Run->GetPartyMembers()) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Initial, &Fixture.Run->GetSkillShopState(), 0) && BeforeBytes == Fixture.ReadBytes() && Events == 0);
    if (!TestTrue(TEXT("The failed skill reroll retries with the original revision"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, Initial.Revision))) return false;
    const FRunSkillShopState FirstReroll = Fixture.Run->GetSkillShopState();
    TestTrue(TEXT("The first skill reroll costs 1G and raises the next cost to 2G"), Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).Skills == Buyer.Skills && FirstReroll.RerollPrice == 2 && FirstReroll.Revision == Initial.Revision + 1 && Events == 1 && bDurableAtNotification);
    if (!CheckOffers(FirstReroll)) return false;
    const TArray<uint8> FirstBytes = Fixture.ReadBytes();
    TestFalse(TEXT("Replaying a successful skill reroll with its old revision is rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, Initial.Revision));
    TestTrue(TEXT("A stale reroll leaves the committed 2G cost and gold unchanged"), FirstBytes == Fixture.ReadBytes() && Fixture.Member(3).Gold == Buyer.Gold - 1 && Events == 1);
    if (!TestTrue(TEXT("The next skill reroll uses the displayed 2G price"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, FirstReroll.Revision))) return false;
    TestTrue(TEXT("Two skill rerolls cost exactly 3G and raise the next price to 3G"), Fixture.Member(3).Gold == Buyer.Gold - 3 && Fixture.Member(3).Skills == Buyer.Skills && Fixture.Run->GetSkillShopState().RerollPrice == 3 && Fixture.Run->GetSkillShopState().Revision == FirstReroll.Revision + 1 && Events == 2 && bDurableAtNotification);
    if (!CheckOffers(Fixture.Run->GetSkillShopState())) return false;
    Fixture.Run->OnRunStateChanged.Clear();
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Continue restores the exact skill reroll result"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Continue retains stock escalation catalog skills and gold without rolling again"), SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetSkillShopState(), &Restored->GetSkillShopState(), 0));
    TStrongObjectPtr<URunSaveGame> LimitedCatalog(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!LimitedCatalog) return false;
    TArray<FRunSkillShopOffer> FiveProducts;
    for (const FRunSkillShopOffer& Offer : LimitedCatalog->SkillShopState.Offers)
    {
        const FRunSkillShopOffer* Product = LimitedCatalog->SkillShopState.Catalog.FindByPredicate([&Offer](const FRunSkillShopOffer& Candidate) { return Candidate.Skill == Offer.Skill; });
        if (!Product) return false;
        FiveProducts.Add(*Product);
    }
    // Limit the catalog to five displayed products so reappearance does not depend on random samples.
    // 후보를 이미 표시된 상품 다섯 개로 제한하여 무작위 표본에 의존하지 않고 재등장을 검증합니다.
    LimitedCatalog->SkillShopState.Catalog = FiveProducts;
    if (!FRunCheckpointStorage::Save(LimitedCatalog.Get(), Fixture.Slot, Fixture.Error) || !Restored->LoadStandaloneCheckpoint(Fixture.Error)) return false;
    const int32 ReappearanceRevision = Restored->GetSkillShopState().Revision;
    if (!TestTrue(TEXT("Already displayed products remain eligible for the next skill reroll"), Restored->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, ReappearanceRevision))) return false;
    if (!CheckOffers(Restored->GetSkillShopState())) return false;
    TestTrue(TEXT("The third reroll costs 3G and retains all five previously displayed candidates"), Restored->GetPartyMembers()[3].Gold == Buyer.Gold - 6 && Restored->GetSkillShopState().RerollPrice == 4 && Restored->GetSkillShopState().Catalog.Num() == FiveProducts.Num());
    TStrongObjectPtr<URunSaveGame> EmptyBalance(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!EmptyBalance) return false;
    EmptyBalance->Party[3].Gold = 0;
    if (!FRunCheckpointStorage::Save(EmptyBalance.Get(), Fixture.Slot, Fixture.Error) || !Restored->LoadStandaloneCheckpoint(Fixture.Error)) return false;
    const TArray<uint8> EmptyBalanceBytes = Fixture.ReadBytes();
    const FRunSkillShopState UnaffordableShop = Restored->GetSkillShopState();
    TestFalse(TEXT("Insufficient personal gold rejects the escalating skill reroll"), Restored->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error, UnaffordableShop.Revision));
    TestTrue(TEXT("An unaffordable reroll preserves its stock next price revision and save file"), EmptyBalanceBytes == Fixture.ReadBytes() && Restored->GetPartyMembers()[3].Gold == 0 && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&UnaffordableShop, &Restored->GetSkillShopState(), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopFrozenLegacyTest, "ProjectA.Run.Shop.LegacyFixedCatalogPreservedWithoutReroll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopFrozenLegacyTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true) || !Fixture.ReachShop()) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Legacy || Legacy->SkillShopState.Catalog.IsEmpty()) return false;
    FRunSkillShopState Frozen;
    Frozen.SchemaVersion = 1;
    Frozen.Recovery = Legacy->SkillShopState.Recovery;
    Frozen.Offers.Add(Legacy->SkillShopState.Catalog[0]);
    Frozen.Offers[0].DisplayName = FText::FromString(TEXT("Legacy Saved Skill Name"));
    Legacy->SkillShopState = Frozen;
    const TArray<FRunPartyMember> FrozenParty = Legacy->Party;
    if (!TestTrue(TEXT("Schema 1 fixed-product saves remain loadable"), FRunCheckpointStorage::Save(Legacy.Get(), Fixture.Slot, Fixture.Error) && Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    const TArray<uint8> FrozenBytes = Fixture.ReadBytes();
    TestTrue(TEXT("Legacy loading retains exact products names loadouts balances and revision zero"), FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Frozen, &Fixture.Run->GetSkillShopState(), 0) && SameShopParty(FrozenParty, Fixture.Run->GetPartyMembers()));
    const FRunPartyMember Buyer = Fixture.Member(3);
    TestFalse(TEXT("Legacy fixed-product saves cannot reroll without a frozen candidate catalog"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRerollOfferId(), Fixture.Error));
    TestTrue(TEXT("A rejected legacy reroll preserves all original saved bytes and balances"), FrozenBytes == Fixture.ReadBytes() && SameShopParty(FrozenParty, Fixture.Run->GetPartyMembers()));
    if (!TestTrue(TEXT("An existing legacy skill remains purchasable without a new revision argument"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Frozen.Offers[0].OfferId, Fixture.Error))) return false;
    TestTrue(TEXT("Legacy purchases debit once without converting or replacing fixed stock"), Fixture.Member(3).Gold == Buyer.Gold - Frozen.Offers[0].Price && Fixture.Member(3).Skills.Contains(Frozen.Offers[0].Skill) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Frozen, &Fixture.Run->GetSkillShopState(), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopLegacyTest, "ProjectA.Run.Shop.LegacyLoadDoesNotGrantGold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopLegacyTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true) || !Fixture.ReachShop()) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!TestNotNull(TEXT("A native isolated save is available for legacy defaults"), Legacy.Get())) return false;
    Legacy->SkillShopState = FRunSkillShopState();
    Legacy->ItemShopState = FRunItemShopState();
    Legacy->GoldRewardState = FRunGoldRewardState();
    for (FRunPartyMember& Member : Legacy->Party)
    {
        Member.Gold = 0;
        Member.bHasSkillLoadout = false;
        Member.Skills.Reset();
        Member.Items.Reset();
        Member.Equipment = FRunEquipmentState();
    }
    if (!TestTrue(TEXT("An older shop checkpoint remains readable"), FRunCheckpointStorage::Save(Legacy.Get(), Fixture.Slot, Fixture.Error) && Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    TestTrue(TEXT("The old shop receives no retroactive products"), Fixture.Run->GetSkillShopState().SchemaVersion == 0 && Fixture.Run->GetSkillShopState().Offers.IsEmpty());
    for (const FRunPartyMember& Member : Fixture.Run->GetPartyMembers())
    {
        FProfessionDefinition Profession;
        TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
        TestTrue(TEXT("Legacy members retain their historical profession loadout and zero gold"), Member.Gold == 0 && !Member.bHasSkillLoadout && Member.Skills.IsEmpty() && Fixture.Run->PartyDefinition->ResolveProfession(Member.ClassId, Profession) && Fixture.Run->PartyDefinition->ResolveMemberSkills(Member, Skills, Fixture.Error) && Skills == Profession.StartingSkills);
    }
    const FRunPartyMember Buyer = Fixture.Member(3);
    TestFalse(TEXT("A legacy shop cannot invent a new purchase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, TEXT("BPDA_swoard_attack"), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestFalse(TEXT("A legacy shop without a balance cannot purchase recovery"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error, Fixture.Run->GetSkillShopState().Revision));
    TestTrue(TEXT("Rejected legacy purchase leaves the original file untouched"), BeforeBytes == Fixture.ReadBytes());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopMalformedSaveTest, "ProjectA.Run.Shop.MalformedSaveReportsReasonWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopMalformedSaveTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true)) return false;
    TStrongObjectPtr<URunSaveGame> Valid(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!TestNotNull(TEXT("Malformed save cases start from a valid native checkpoint"), Valid.Get())) return false;
    const TArray<FRunPartyMember> BeforeParty = Fixture.Run->GetPartyMembers();
    const FRunIdentityData BeforeIdentity = Fixture.Run->GetRunIdentity();
    const FRunSkillShopState BeforeShop = Fixture.Run->GetSkillShopState();
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    for (int32 Case = 0; Case < 11; ++Case)
    {
        TStrongObjectPtr<URunSaveGame> Invalid(DuplicateObject<URunSaveGame>(Valid.Get(), GetTransientPackage()));
        switch (Case)
        {
        case 0:
            Invalid->Party[3].Gold = -1;
            break;
        case 1:
            Invalid->Party[3].Gold = 0;
            Invalid->Party[3].Skills.Reset();
            Invalid->Party[3].bHasSkillLoadout = false;
            break;
        case 2:
            Invalid->Party[3].Skills.Add(FSoftObjectPath(Invalid->Party[3].Skills[0]));
            break;
        case 3:
            Invalid->SkillShopState.Recovery.Price = 0;
            break;
        case 4:
            Invalid->SkillShopState.Offers[0].OfferId = FRunSkillShopState::GetRecoveryOfferId();
            break;
        case 5:
            Invalid->Phase = static_cast<ERunPhase>(255);
            break;
        case 6:
            Invalid->SkillShopState.Revision = -1;
            break;
        case 7:
            Invalid->SkillShopState.RerollPrice = 0;
            break;
        case 8:
            Invalid->SkillShopState.Catalog[0].Price = 0;
            break;
        case 9:
            ++Invalid->SkillShopState.Offers[0].Price;
            break;
        default:
            Invalid->SkillShopState.Catalog.Reset();
            break;
        }
        if (!TestTrue(TEXT("Each malformed fixture is written only to its isolated test slot"), FRunCheckpointStorage::Save(Invalid.Get(), Fixture.Slot, Fixture.Error))) return false;
        const TArray<uint8> InvalidBytes = Fixture.ReadBytes();
        Fixture.Error = FText::GetEmpty();
        TestFalse(TEXT("Continue eligibility rejects malformed party shop or phase"), Fixture.Run->CanContinueStandaloneSavedRun(Fixture.Error));
        TestFalse(TEXT("Eligibility rejection always supplies a visible reason"), Fixture.Error.IsEmpty());
        Fixture.Error = FText::GetEmpty();
        TestFalse(TEXT("Actual loading also rejects the same malformed checkpoint"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error));
        TestFalse(TEXT("Successful helper validation never clears the later rejection reason"), Fixture.Error.IsEmpty());
        TestTrue(TEXT("Rejected load preserves the active party identity shop and phase"), SameShopParty(BeforeParty, Fixture.Run->GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&BeforeIdentity, &Fixture.Run->GetRunIdentity(), 0) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&BeforeShop, &Fixture.Run->GetSkillShopState(), 0) && Fixture.Run->GetPhase() == ERunPhase::Map && Fixture.Run->GetCompletedNodes().IsEmpty());
        TestTrue(TEXT("Rejected loading never repairs or overwrites the user's original file"), InvalidBytes == Fixture.ReadBytes());
    }
    TestEqual(TEXT("All malformed checkpoint rejections emit no state changes"), Events, 0);
    Fixture.Run->OnRunStateChanged.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemShopCatalogNamesTest, "ProjectA.Run.Shop.CatalogDisplayNamesAndLegacyColumns", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemShopCatalogNamesTest::RunTest(const FString& Parameters)
{
    FString LegacyCsv = TEXT("무기 종류,위치,에셋 이름,가격(G)\n");
    FString NamedCsv = TEXT("무기 종류,위치,에셋 이름,가격(G),게임 내 이름\n");
    for (int32 Index = 0; Index < 5; ++Index)
    {
        LegacyCsv += FString::Printf(TEXT("검,/Game/Test,Weapon_%d,1\n"), Index);
        NamedCsv += FString::Printf(TEXT("검,/Game/Test,Weapon_%d,1,\"서약의 검, %d\"\n"), Index, Index);
    }
    TArray<FRunItemDefinition> Legacy;
    TArray<FRunItemDefinition> Named;
    FText Error;
    if (!TestTrue(TEXT("The original four-column CSV remains supported"), RunItemShopCatalog::LoadFromString(LegacyCsv, Legacy, Error))) return false;
    if (!TestTrue(TEXT("The fifth column supports Korean names and quoted commas"), RunItemShopCatalog::LoadFromString(NamedCsv, Named, Error))) return false;
    if (!TestEqual(TEXT("Names do not change catalog size"), Named.Num(), Legacy.Num())) return false;
    for (int32 Index = 0; Index < Named.Num(); ++Index)
    {
        TestEqual(TEXT("Legacy CSV names still use source asset names"), Legacy[Index].DisplayName.ToString(), Legacy[Index].Asset.GetAssetName());
        TestEqual(TEXT("The authored name survives CSV parsing exactly"), Named[Index].DisplayName.ToString(), FString::Printf(TEXT("서약의 검, %d"), Index));
        TestTrue(TEXT("Naming does not change asset identity price or GAS classification"), Named[Index].Asset == Legacy[Index].Asset && Named[Index].Price == Legacy[Index].Price && Named[Index].Tags == Legacy[Index].Tags);
    }
    FRunItemShopState LegacyState;
    LegacyState.SchemaVersion = 1;
    LegacyState.Catalog = Legacy;
    TestTrue(TEXT("Frozen original asset names remain valid for rerolls"), RunItemShopCatalog::Roll(LegacyState, false, FGameplayTagQuery(), Error) && RunItemShopCatalog::Validate(LegacyState, Error));
    FRunItemShopState NamedState;
    NamedState.SchemaVersion = 1;
    NamedState.Catalog = Named;
    if (!TestTrue(TEXT("Authored names remain valid for rerolls"), RunItemShopCatalog::Roll(NamedState, false, FGameplayTagQuery(), Error))) return false;
    NamedState.Offers[0].Item.DisplayName = FText::FromString(TEXT("카탈로그와 다른 이름"));
    TestFalse(TEXT("An offer cannot override its frozen catalog name"), RunItemShopCatalog::Validate(NamedState, Error));
    TArray<FRunItemDefinition> RejectedCatalog;
    TestFalse(TEXT("Unknown fifth-column headers are rejected"), RunItemShopCatalog::LoadFromString(NamedCsv.Replace(TEXT("게임 내 이름"), TEXT("잘못된 열")), RejectedCatalog, Error));
    TestFalse(TEXT("Rows must retain the declared column count"), RunItemShopCatalog::LoadFromString(NamedCsv.Replace(TEXT(",\"서약의 검, 0\""), TEXT("")), RejectedCatalog, Error));
    TestFalse(TEXT("An authored blank name cannot silently fall back to the asset name"), RunItemShopCatalog::LoadFromString(NamedCsv.Replace(TEXT("서약의 검, 0"), TEXT(" ")), RejectedCatalog, Error));
    TestFalse(TEXT("Display names cannot inject line breaks into the UI"), RunItemShopCatalog::LoadFromString(NamedCsv.Replace(TEXT("서약의 검, 0"), TEXT("이름\n두 번째 줄")), RejectedCatalog, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemShopCatalogPriceTest, "ProjectA.Run.Shop.CatalogPriceWholeInteger", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemShopCatalogPriceTest::RunTest(const FString& Parameters)
{
    const auto MakeCsv = [](bool bNamed, const FString& Price)
    {
        FString Csv = bNamed ? TEXT("무기 종류,위치,에셋 이름,가격(G),게임 내 이름\n") : TEXT("무기 종류,위치,에셋 이름,가격(G)\n");
        for (int32 Index = 0; Index < 5; ++Index) Csv += FString::Printf(TEXT("검,/Game/Test,Weapon_%d,\"%s\"%s\n"), Index, Index == 2 ? *Price : TEXT("1"), bNamed ? TEXT(",가격 검증") : TEXT(""));
        return Csv;
    };
    for (const bool bNamed : {false, true})
    {
        TArray<FRunItemDefinition> Catalog;
        FText Error;
        for (const FString Price : {TEXT("1"), TEXT("0001"), TEXT("+1"), TEXT(" 1 "), TEXT("2147483647")})
        {
            if (!TestTrue(TEXT("Both CSV schemas accept whole positive int32 prices"), RunItemShopCatalog::LoadFromString(MakeCsv(bNamed, Price), Catalog, Error))) return false;
            TestEqual(TEXT("Whole price and int32 upper boundary are retained"), Catalog[2].Price, Price == TEXT("2147483647") ? MAX_int32 : 1);
        }
        const TArray<FRunItemDefinition> Before = Catalog;
        for (const FString Price : {TEXT(""), TEXT(" "), TEXT("+"), TEXT("0"), TEXT("-1"), TEXT("1abc"), TEXT("1.5"), TEXT("1.0"), TEXT("1,000"), TEXT("1e3"), TEXT("1 2"), TEXT("NaN"), TEXT("2147483648"), TEXT("4294967297"), TEXT("999999999999999999999")})
        {
            TestFalse(TEXT("Malformed prices cannot silently become a smaller positive cost"), RunItemShopCatalog::LoadFromString(MakeCsv(bNamed, Price), Catalog, Error));
            TestFalse(TEXT("Invalid prices explain the rejected CSV row"), Error.IsEmpty());
            if (!TestEqual(TEXT("Failed parsing preserves the previous complete catalog"), Catalog.Num(), Before.Num())) return false;
            for (int32 Index = 0; Index < Catalog.Num(); ++Index) TestTrue(TEXT("Partial rows never replace existing items or prices"), FRunItemDefinition::StaticStruct()->CompareScriptStruct(&Catalog[Index], &Before[Index], 0));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemShopCatalogRarityTest, "ProjectA.Run.Shop.CatalogAuthoredRarity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemShopCatalogRarityTest::RunTest(const FString& Parameters)
{
    const TCHAR* ShopColorNames[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
    const FName ShopColorTags[] = {TEXT("Item.Rarity.White"), TEXT("Item.Rarity.Green"), TEXT("Item.Rarity.Blue"), TEXT("Item.Rarity.Purple"), TEXT("Item.Rarity.Orange")};
    for (const bool bHasRationale : {false, true})
    {
        FString Csv = bHasRationale ? TEXT("무기 종류,위치,에셋 이름,가격(G),게임 내 이름,등급,분류 근거\n") : TEXT("무기 종류,위치,에셋 이름,가격(G),게임 내 이름,등급\n");
        for (int32 Index = 0; Index < 5; ++Index) Csv += FString::Printf(TEXT("검,/Game/Test,Weapon_%d,1,등급 검증 %d,%s%s\n"), Index, Index, ShopColorNames[Index], bHasRationale ? TEXT(",\"원본 형태, 장식 확인\"") : TEXT(""));
        TArray<FRunItemDefinition> Catalog;
        FText Error;
        if (!TestTrue(TEXT("Both authored rarity CSV schemas retain five source definitions"), RunItemShopCatalog::LoadFromString(Csv, Catalog, Error) && Catalog.Num() == 5)) return false;
        for (int32 Index = 0; Index < Catalog.Num(); ++Index)
        {
            const FRunItemDefinition& Item = Catalog[Index];
            TestEqual(TEXT("Authored Korean colors resolve to existing gameplay rarity tags"), Item.CatalogRarityTag, FGameplayTag::RequestGameplayTag(ShopColorTags[Index]));
            TestTrue(TEXT("Catalog parsing does not generate item copies or change prices"), Item.GenerationVersion == 0 && !Item.ItemInstanceId.IsValid() && !Item.RarityTag.IsValid() && Item.GrantedSkills.IsEmpty() && Item.Price == 1);
            FRunItemDefinition Legacy = Item;
            Legacy.CatalogRarityTag = FGameplayTag();
            TestFalse(TEXT("The frozen authored grade is part of base catalog identity"), RunItemShopCatalog::IsSameBaseDefinition(Item, Legacy));
        }
        const TArray<FRunItemDefinition> Before = Catalog;
        for (const FString InvalidColor : {TEXT(""), TEXT(" "), TEXT("검은색"), TEXT("Item.Rarity.White"), TEXT("흰색 ")})
        {
            TestFalse(TEXT("Empty or unsupported authored grades cannot fall back to random rarity"), RunItemShopCatalog::LoadFromString(Csv.Replace(TEXT("흰색"), *InvalidColor), Catalog, Error));
            if (!TestEqual(TEXT("Rejected rarity CSV preserves the complete previous catalog size"), Catalog.Num(), Before.Num())) return false;
            for (int32 Index = 0; Index < Catalog.Num(); ++Index) TestTrue(TEXT("Rejected rarity CSV preserves every original catalog definition"), RunItemShopCatalog::IsSameDefinition(Catalog[Index], Before[Index]));
        }
        TestFalse(TEXT("Unknown rarity column headers are rejected"), RunItemShopCatalog::LoadFromString(Csv.Replace(TEXT("등급,분류"), TEXT("알수없음,분류")).Replace(TEXT("등급\n"), TEXT("알수없음\n")), Catalog, Error));
        if (bHasRationale)
        {
            TestFalse(TEXT("Unknown rationale column headers are rejected"), RunItemShopCatalog::LoadFromString(Csv.Replace(TEXT("분류 근거"), TEXT("알수없음")), Catalog, Error));
            TestFalse(TEXT("Declared rationale columns cannot contain only whitespace"), RunItemShopCatalog::LoadFromString(Csv.Replace(TEXT("원본 형태, 장식 확인"), TEXT(" ")), Catalog, Error));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemShopPersistenceTest, "ProjectA.Run.Shop.ItemPurchaseRerollAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemShopPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true) || !Fixture.ReachShop(FRunItemShopState::GetEncounterId())) return false;
    const FRunPartyMember Buyer = Fixture.Member(3);
    const FRunItemShopState Initial = Fixture.Run->GetItemShopState();
    if (!TestEqual(TEXT("The item shop displays exactly five offers"), Initial.Offers.Num(), 5)) return false;
    TestEqual(TEXT("The whole authored CSV is available"), Initial.Catalog.Num(), 289);
    TestTrue(TEXT("New runs load authored gameplay names"), Initial.Catalog.ContainsByPredicate([](const FRunItemDefinition& Item) { return Item.DisplayName.ToString() != Item.Asset.GetAssetName(); }));
    TSet<FSoftObjectPath> Assets;
    for (const FRunItemShopOffer& Offer : Initial.Offers)
    {
        TestFalse(TEXT("Displayed stock contains no duplicate asset"), Assets.Contains(Offer.Item.Asset));
        Assets.Add(Offer.Item.Asset);
        const FRunItemDefinition* CatalogItem = Initial.Catalog.FindByPredicate([&Offer](const FRunItemDefinition& Item) { return Item.Asset == Offer.Item.Asset; });
        if (!TestNotNull(TEXT("Every displayed product belongs to the frozen catalog"), CatalogItem)) return false;
        TestEqual(TEXT("The product keeps its authored catalog name"), Offer.Item.DisplayName.ToString(), CatalogItem->DisplayName.ToString());
        TestEqual(TEXT("Each test item costs 1G"), Offer.Item.Price, 1);
    }
    const FName OfferId = Initial.Offers[0].OfferId;
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    TestFalse(TEXT("A companion cannot spend the buyer's money"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Fixture.Member(0).CharacterId, OfferId, Fixture.Error, Initial.Revision));
    TestFalse(TEXT("Item shops cannot execute skill products"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Fixture.Run->GetSkillShopState().Offers[0].OfferId, Fixture.Error, Initial.Revision));
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed item write rejects the purchase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Initial.Revision));
    TestTrue(TEXT("Failed purchase preserves money inventory stock and disk"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Fixture.Member(3), &Buyer, 0) && !Fixture.Run->GetItemShopState().Offers[0].bSold && Fixture.Run->GetItemShopState().Revision == Initial.Revision && Fixture.ReadBytes() == BeforeBytes);
    if (!TestTrue(TEXT("A valid purchase is saved"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Initial.Revision))) return false;
    TestTrue(TEXT("The item is owned without changing combat skills or starting equipment"), Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).Items.Num() == Buyer.Items.Num() + 1 && Fixture.Member(3).Items.Last().Asset == Initial.Offers[0].Item.Asset && Fixture.Member(3).Skills == Buyer.Skills && FRunEquipmentState::StaticStruct()->CompareScriptStruct(&Fixture.Member(3).Equipment, &Buyer.Equipment, 0));
    const int32 PurchasedRevision = Fixture.Run->GetItemShopState().Revision;
    TestFalse(TEXT("A sold slot cannot be purchased again"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, PurchasedRevision));
    TestFalse(TEXT("A stale reroll cannot charge the buyer"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunItemShopState::GetRerollOfferId(), Fixture.Error, Initial.Revision));
    const TArray<uint8> PurchasedBytes = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed reroll write is rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunItemShopState::GetRerollOfferId(), Fixture.Error, PurchasedRevision));
    TestTrue(TEXT("Failed reroll preserves purchased stock balance and disk"), Fixture.Run->GetItemShopState().Revision == PurchasedRevision && Fixture.Run->GetItemShopState().Offers[0].bSold && Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.ReadBytes() == PurchasedBytes);
    if (!TestTrue(TEXT("The reroll retry succeeds"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunItemShopState::GetRerollOfferId(), Fixture.Error, PurchasedRevision))) return false;
    TestEqual(TEXT("One purchase and one reroll charge exactly 2G"), Fixture.Member(3).Gold, Buyer.Gold - 2);
    TestFalse(TEXT("The previous slot identity cannot buy replacement stock"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Fixture.Run->GetItemShopState().Revision));
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Continue restores the item shop"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Continue keeps exact stock gold and owned items"), FRunItemShopState::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetItemShopState(), &Restored->GetItemShopState(), 0) && SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()));
    TStrongObjectPtr<URunSaveGame> LegacyNames(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!LegacyNames) return false;
    for (FRunItemDefinition& Item : LegacyNames->ItemShopState.Catalog) Item.DisplayName = FText::FromString(Item.Asset.GetAssetName());
    for (FRunItemShopOffer& Offer : LegacyNames->ItemShopState.Offers) Offer.Item.DisplayName = FText::FromString(Offer.Item.Asset.GetAssetName());
    for (FRunPartyMember& Member : LegacyNames->Party)
    {
        for (FRunItemDefinition& Item : Member.Items) Item.DisplayName = FText::FromString(Item.Asset.GetAssetName());
    }
    if (!FRunCheckpointStorage::Save(LegacyNames.Get(), Fixture.Slot, Fixture.Error)) return false;
    if (!TestTrue(TEXT("Continue still accepts original asset names saved before game naming"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Loading legacy names never rewrites the frozen catalog offers or owned copies"), FRunItemShopState::StaticStruct()->CompareScriptStruct(&LegacyNames->ItemShopState, &Restored->GetItemShopState(), 0) && SameShopParty(LegacyNames->Party, Restored->GetPartyMembers()));
    TStrongObjectPtr<URunSaveGame> EmptyBalance(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!EmptyBalance) return false;
    EmptyBalance->Party[3].Gold = 0;
    if (!FRunCheckpointStorage::Save(EmptyBalance.Get(), Fixture.Slot, Fixture.Error) || !Restored->LoadStandaloneCheckpoint(Fixture.Error)) return false;
    const TArray<uint8> EmptyBalanceBytes = Fixture.ReadBytes();
    TestFalse(TEXT("Zero gold cannot reroll"), Restored->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunItemShopState::GetRerollOfferId(), Fixture.Error, Restored->GetItemShopState().Revision));
    TestFalse(TEXT("Zero gold cannot purchase stock"), Restored->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Restored->GetItemShopState().Offers[0].OfferId, Fixture.Error, Restored->GetItemShopState().Revision));
    TestTrue(TEXT("Insufficient funds leave the save untouched"), Fixture.ReadBytes() == EmptyBalanceBytes);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunStartingEquipmentTest, "ProjectA.Run.Equipment.StartingLoadoutsAndOccupancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunStartingEquipmentTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!TestTrue(TEXT("The four professions initialize with source equipment"), Fixture.Initialize())) return false;
    const FGameplayTag Main = URunEquipmentCatalog::GetWeaponSlot(0);
    const FGameplayTag Off = URunEquipmentCatalog::GetWeaponSlot(1);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FRunPartyMember& Member = Fixture.Member(Index);
        TestTrue(TEXT("Human and AI starting equipment is valid and explicit"), Member.Equipment.bHasLoadout && RunEquipmentRules::Validate(Member, Fixture.Error));
        if (!TestEqual(TEXT("Only the warrior owns two starting copies"), Member.Items.Num(), Index == 0 ? 2 : 1)) return false;
        TestEqual(TEXT("Every starting item is equipped exactly once"), Member.Equipment.Slots.Num(), Member.Items.Num());
        TestEqual(TEXT("Equipment does not add starting combat skills"), Member.Skills.Num(), 1);
    }
    TestTrue(TEXT("The warrior holds a sword and a separate shield"), Fixture.Member(0).Items[0].Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Sword"))) && Fixture.Member(0).Items[1].Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield"))) && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(0), Main) == 0 && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(0), Off) == 1);
    TestTrue(TEXT("The archer's single bow occupies both hands"), Fixture.Member(1).Items[0].Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Bow"))) && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(1), Main) == 0 && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(1), Off) == 0);
    TestTrue(TEXT("The mage's single staff occupies both hands"), Fixture.Member(2).Items[0].Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.StaffWand"))) && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(2), Main) == 0 && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(2), Off) == 0);
    TestTrue(TEXT("The rogue holds one dagger with the other hand empty"), Fixture.Member(3).Items[0].Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger"))) && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(3), Main) == 0 && RunEquipmentRules::FindItemIndexAtSlot(Fixture.Member(3), Off) == INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEquipmentSwapTest, "ProjectA.Run.Equipment.SwapUnequipAndTwoHandCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEquipmentSwapTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize() || Fixture.Member(0).Items.Num() != 2 || Fixture.Member(1).Items.IsEmpty() || Fixture.Member(3).Items.IsEmpty()) return false;
    FRunPartyMember Member = Fixture.Member(0);
    const int32 Dagger = Member.Items.Add(Fixture.Member(3).Items[0]);
    const int32 Bow = Member.Items.Add(Fixture.Member(1).Items[0]);
    const FGameplayTag Main = URunEquipmentCatalog::GetWeaponSlot(0);
    const FGameplayTag Off = URunEquipmentCatalog::GetWeaponSlot(1);
    const auto Change = [&Member, &Fixture](int32 ItemIndex, FGameplayTag Target)
    {
        FRunEquipmentCommand Command;
        Command.CharacterId = Member.CharacterId;
        Command.ItemIndex = ItemIndex;
        Command.TargetSlot = Target;
        Command.ExpectedRevision = Member.Equipment.Revision;
        return RunEquipmentRules::Apply(Member, Command, Fixture.Error);
    };
    if (!TestTrue(TEXT("A bag dagger can replace the shield"), Change(Dagger, Off))) return false;
    TestFalse(TEXT("The replaced shield returns to the bag"), RunEquipmentRules::IsItemEquipped(Member, 1));
    if (!TestTrue(TEXT("Compatible equipped single-hand copies swap"), Change(0, Off))) return false;
    TestTrue(TEXT("The dagger and sword exchange hands without duplication"), RunEquipmentRules::FindItemIndexAtSlot(Member, Main) == Dagger && RunEquipmentRules::FindItemIndexAtSlot(Member, Off) == 0 && Member.Equipment.Slots.Num() == 2);
    if (!TestTrue(TEXT("A two-handed bow replaces both held items"), Change(Bow, Main))) return false;
    TestTrue(TEXT("Both occupied hands resolve to one physical bow copy"), Member.Equipment.Slots.Num() == 1 && RunEquipmentRules::FindItemIndexAtSlot(Member, Main) == Bow && RunEquipmentRules::FindItemIndexAtSlot(Member, Off) == Bow && !RunEquipmentRules::IsItemEquipped(Member, Dagger) && !RunEquipmentRules::IsItemEquipped(Member, 0));
    FRunPartyMember Overlap = Member;
    FRunEquipmentSlot& OverlappingSword = Overlap.Equipment.Slots.AddDefaulted_GetRef();
    OverlappingSword.SlotTag = Main;
    OverlappingSword.ItemIndex = 0;
    TestFalse(TEXT("A malformed two-handed overlap is rejected"), RunEquipmentRules::Validate(Overlap, Fixture.Error));
    if (!TestTrue(TEXT("Dropping equipment into the bag unequips it"), Change(Bow, FGameplayTag()))) return false;
    TestTrue(TEXT("Unequip clears both hands without deleting owned copies"), Member.Equipment.Slots.IsEmpty() && Member.Items.Num() == 4);
    Member.Equipment = FRunEquipmentState();
    TestTrue(TEXT("A saved legacy inventory supports its first explicit equipment command"), Change(0, Main) && Member.Equipment.bHasLoadout);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEquipmentGrantedSkillRightsTest, "ProjectA.Run.Equipment.GrantedSkillSourcesAndAtomicChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEquipmentGrantedSkillRightsTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize() || Fixture.Member(0).Items.Num() != 2 || Fixture.Member(1).Items.IsEmpty() || Fixture.Member(3).Items.IsEmpty()) return false;
    FRunPartyMember Member = Fixture.Member(0);
    Member.InnateSkills = {Fixture.Run->PartyDefinition->UnarmedStartingSkill.ToSoftObjectPath()};
    TStrongObjectPtr<USkillDefinitionDataAsset> Shared(NewObject<USkillDefinitionDataAsset>());
    TStrongObjectPtr<USkillDefinitionDataAsset> SwordOnly(NewObject<USkillDefinitionDataAsset>());
    TStrongObjectPtr<USkillDefinitionDataAsset> BowOnly(NewObject<USkillDefinitionDataAsset>());
    Shared->bUseRoundDefinition = true;
    SwordOnly->bUseRoundDefinition = true;
    BowOnly->bUseRoundDefinition = true;
    const FSoftObjectPath SharedPath(Shared.Get());
    const FSoftObjectPath SwordPath(SwordOnly.Get());
    const FSoftObjectPath BowPath(BowOnly.Get());
    Member.Items[0].GrantedSkills = {SharedPath, SwordPath};
    Member.Items[1].GrantedSkills.Reset();
    const int32 Dagger = Member.Items.Add(Fixture.Member(3).Items[0]);
    const int32 Bow = Member.Items.Add(Fixture.Member(1).Items[0]);
    Member.Items[Dagger].GrantedSkills = {SharedPath};
    Member.Items[Bow].GrantedSkills = {BowPath};
    const FGameplayTag Main = URunEquipmentCatalog::GetWeaponSlot(0);
    const FGameplayTag Off = URunEquipmentCatalog::GetWeaponSlot(1);
    const auto Change = [&Member, &Fixture](int32 ItemIndex, FGameplayTag Target)
    {
        FRunEquipmentCommand Command;
        Command.CharacterId = Member.CharacterId;
        Command.ItemIndex = ItemIndex;
        Command.TargetSlot = Target;
        Command.ExpectedRevision = Member.Equipment.Revision;
        return RunEquipmentRules::Apply(Member, Command, Fixture.Error);
    };
    if (!TestTrue(TEXT("Equipping two sources merges their shared skill into one button"), Change(Dagger, Off))) return false;
    TArray<FSoftObjectPath> Expected = Member.InnateSkills;
    Expected.Add(SharedPath);
    Expected.Add(SwordPath);
    TestTrue(TEXT("Innate then equipped source order is preserved without duplicate skill IDs"), Member.Skills == Expected);
    TestTrue(TEXT("Each weapon retains its original independent grant source"), Member.Items[0].GrantedSkills == TArray<FSoftObjectPath>{SharedPath, SwordPath} && Member.Items[Dagger].GrantedSkills == TArray<FSoftObjectPath>{SharedPath});
    if (!TestTrue(TEXT("The first shared-skill source can be unequipped"), Change(0, FGameplayTag()))) return false;
    Expected = Member.InnateSkills;
    Expected.Add(SharedPath);
    TestTrue(TEXT("The remaining equipped source keeps the shared skill and removes the sword-only skill"), Member.Skills == Expected && Member.Items[0].GrantedSkills.Contains(SwordPath));
    if (!TestTrue(TEXT("The final shared-skill source can be unequipped"), Change(Dagger, FGameplayTag()))) return false;
    TestTrue(TEXT("Removing the last source retains only innate skills"), Member.Skills == Member.InnateSkills);
    if (!TestTrue(TEXT("Re-equipping a copy restores its fixed grants"), Change(0, Main) && Change(Dagger, Off))) return false;
    if (!TestTrue(TEXT("A two-handed bow replaces both equipped weapon sources"), Change(Bow, Main))) return false;
    Expected = Member.InnateSkills;
    Expected.Add(BowPath);
    TestTrue(TEXT("Both occupied hands provide one bow source and one bow skill"), Member.Equipment.Slots.Num() == 1 && RunEquipmentRules::FindItemIndexAtSlot(Member, Main) == Bow && RunEquipmentRules::FindItemIndexAtSlot(Member, Off) == Bow && Member.Skills == Expected);
    Member.Items[Dagger].GrantedSkills = {SharedPath, SharedPath};
    const FRunPartyMember Before = Member;
    TestFalse(TEXT("Duplicate grants inside one weapon reject the equipment transaction"), Change(Dagger, Off));
    TestTrue(TEXT("Failed grant validation preserves equipment revision skills and all owned copies"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Member, &Before, 0));
    Member.Items[Dagger].GrantedSkills = {FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/T12/MissingWeaponSkill.MissingWeaponSkill"))};
    const FRunPartyMember BeforeMissing = Member;
    TestFalse(TEXT("A missing granted skill rejects the equipment transaction"), Change(Dagger, Off));
    TestTrue(TEXT("Missing grant rejection publishes no equipment or skill changes"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Member, &BeforeMissing, 0));
    Member.InnateSkills.Reset();
    const TArray<FSoftObjectPath> LegacySkills = Member.Skills;
    TestTrue(TEXT("An older Run can unequip without adopting weapon-driven skill rights"), Change(Bow, FGameplayTag()) && Member.Skills == LegacySkills);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEquipmentAuthorityTest, "ProjectA.Run.Equipment.ShopOwnerRevisionAndIndexValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEquipmentAuthorityTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true)) return false;
    const FRunPartyMember Owner = Fixture.Member(3);
    FRunEquipmentCommand Command;
    Command.CharacterId = Owner.CharacterId;
    Command.ItemIndex = 0;
    Command.ExpectedRevision = Owner.Equipment.Revision;
    TestFalse(TEXT("Equipment cannot change outside a shop"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Command, Fixture.Error));
    if (!Fixture.ReachShop()) return false;
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    FRunAccountId Outsider = Owner.OwnerAccountId;
    Outsider.Subject += TEXT("_EquipmentOutsider");
    TestFalse(TEXT("A foreign account cannot change another character's equipment"), Fixture.Run->ChangeEquipment(Outsider, Command, Fixture.Error));
    FRunEquipmentCommand Invalid = Command;
    Invalid.CharacterId = Fixture.Member(0).CharacterId;
    Invalid.ExpectedRevision = Fixture.Member(0).Equipment.Revision;
    TestFalse(TEXT("The owner cannot redirect equipment changes to an AI companion"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Invalid, Fixture.Error));
    Invalid = Command;
    ++Invalid.ExpectedRevision;
    TestFalse(TEXT("A stale drag revision is rejected"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Invalid, Fixture.Error));
    Invalid = Command;
    Invalid.ItemIndex = MAX_int32;
    TestFalse(TEXT("An unknown item copy is rejected"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Invalid, Fixture.Error));
    Invalid = Command;
    Invalid.TargetSlot = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Head"));
    TestFalse(TEXT("Weapon drops onto nonweapon slots are rejected"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Invalid, Fixture.Error));
    TestTrue(TEXT("Every rejected request preserves all members and durable bytes"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && BeforeBytes == Fixture.ReadBytes());
    TestEqual(TEXT("Rejected equipment commands emit no state changes"), Events, 0);
    Fixture.Run->OnRunStateChanged.Clear();
    FSkillShopFixture DeadFixture;
    if (!DeadFixture.Initialize() || !DeadFixture.ReachShop(TEXT("Shop_01"), 3)) return false;
    Command.CharacterId = DeadFixture.Member(3).CharacterId;
    Command.ExpectedRevision = DeadFixture.Member(3).Equipment.Revision;
    TestFalse(TEXT("A dead owner can inspect equipment but cannot change it"), DeadFixture.Run->ChangeEquipment(DeadFixture.Member(3).OwnerAccountId, Command, DeadFixture.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEquipmentPersistenceTest, "ProjectA.Run.Equipment.AtomicChangeAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEquipmentPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true) || !Fixture.ReachShop()) return false;
    const FRunPartyMember Owner = Fixture.Member(3);
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    FRunEquipmentCommand Command;
    Command.CharacterId = Owner.CharacterId;
    Command.ItemIndex = 0;
    Command.ExpectedRevision = Owner.Equipment.Revision;
    int32 Events = 0;
    bool bDurableAtNotification = false;
    Fixture.Run->OnRunStateChanged.AddLambda([&Fixture, &Events, &bDurableAtNotification, &Owner]()
    {
        ++Events;
        TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        bDurableAtNotification = Durable && Durable->Party[3].Equipment.Slots.IsEmpty() && Durable->Party[3].Equipment.Revision == Owner.Equipment.Revision + 1 && SameShopParty(Durable->Party, Fixture.Run->GetPartyMembers());
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed durable write rejects unequip"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Command, Fixture.Error));
    TestTrue(TEXT("Failed unequip preserves party revisions and exact save bytes"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && BeforeBytes == Fixture.ReadBytes());
    TestEqual(TEXT("Failed unequip publishes no state change"), Events, 0);
    if (!TestTrue(TEXT("The original drag can retry after storage recovers"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Command, Fixture.Error))) return false;
    TestTrue(TEXT("One notification observes already committed equipment"), Events == 1 && bDurableAtNotification);
    TestTrue(TEXT("Equipment changes neither consume items nor charge gold or alter skills"), Fixture.Member(3).Items.Num() == Owner.Items.Num() && Fixture.Member(3).Gold == Owner.Gold && Fixture.Member(3).Skills == Owner.Skills);
    TestFalse(TEXT("Replaying the committed drag is rejected"), Fixture.Run->ChangeEquipment(Owner.OwnerAccountId, Command, Fixture.Error));
    TestEqual(TEXT("A replay publishes no additional state change"), Events, 1);
    Fixture.Run->OnRunStateChanged.Clear();
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Continue restores equipment and individual owned copies"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Reload preserves the complete equipment state without granting starts again"), SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()));
    return true;
}

#endif
