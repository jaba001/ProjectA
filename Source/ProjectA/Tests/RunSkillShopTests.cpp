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
            if (!TestTrue(TEXT("Every direct-control profession initializes"), Fixture.Initialize(SelectedSlot))) return false;
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
            if (!TestEqual(TEXT("Every shop sells the four existing skills"), Offers.Num(), 4)) return false;
            for (const FRunSkillShopOffer& Offer : Offers)
            {
                TestEqual(TEXT("Every prototype skill costs 1G"), Offer.Price, 1);
                TestFalse(TEXT("Products never sell the free unarmed attack"), Offer.Skill == Unarmed);
                TestTrue(TEXT("Every profession can purchase every product"), Fixture.Run->PurchaseShopOffer(Account, CharacterId, Offer.OfferId, Fixture.Error));
                TestFalse(TEXT("A learned skill cannot be bought twice"), Fixture.Run->PurchaseShopOffer(Account, CharacterId, Offer.OfferId, Fixture.Error));
            }
            TestEqual(TEXT("Four successful purchases consume exactly 4G"), Fixture.Member(SelectedSlot).Gold, Before[SelectedSlot].Gold - 4);
            TestEqual(TEXT("The buyer retains unarmed and all four purchases"), Fixture.Member(SelectedSlot).Skills.Num(), 5);
            FProfessionDefinition Profession;
            if (!Fixture.Run->PartyDefinition->ResolveProfession(Fixture.Member(SelectedSlot).ClassId, Profession)) return false;
            TestTrue(TEXT("Every shop can recover every profession even with five equipped skills"), Fixture.Run->PurchaseShopOffer(Account, CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
            TestTrue(TEXT("Recovery restores all HP for 1G without changing the full loadout"), Fixture.Member(SelectedSlot).CurrentHP == Profession.MaxHP && Fixture.Member(SelectedSlot).Gold == Before[SelectedSlot].Gold - 5 && Fixture.Member(SelectedSlot).Skills.Num() == 5);
            for (int32 Index = 0; Index < Before.Num(); ++Index)
            {
                if (Index != SelectedSlot) TestTrue(TEXT("Companion HP balance and loadout remain unchanged"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Before[Index], &Fixture.Member(Index), 0));
            }
            TestTrue(TEXT("Purchases preserve all participant and host identity fields"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Fixture.Run->GetRunIdentity(), 0));
            TestTrue(TEXT("The next battle starts with the purchased Run loadout"), Fixture.Run->LeaveRunEncounter() && Fixture.Run->BeginEncounter(TEXT("Combat_02")) && Fixture.Run->MarkCombatStarted());
            TArray<TObjectPtr<USkillDefinitionDataAsset>> ResolvedSkills;
            TestTrue(TEXT("Spawn resolution includes the acquired skills"), Fixture.Run->PartyDefinition->ResolveMemberSkills(Fixture.Member(SelectedSlot), ResolvedSkills, Fixture.Error) && ResolvedSkills.Num() == 5);
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
    const FName OfferId = Fixture.Run->GetSkillShopState().Offers[0].OfferId;
    TestFalse(TEXT("A purchase cannot bypass the Map phase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error));
    TestFalse(TEXT("Recovery cannot bypass the Map phase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
    if (!TestTrue(TEXT("The rejection fixture enters a shop"), Fixture.ReachShop())) return false;
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const TArray<uint8> DiskBefore = Fixture.ReadBytes();
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    FRunAccountId Outsider = Buyer.OwnerAccountId;
    Outsider.Subject += TEXT("_Other");
    TestFalse(TEXT("A foreign account cannot buy for the selected character"), Fixture.Run->PurchaseShopOffer(Outsider, Buyer.CharacterId, OfferId, Fixture.Error));
    TestFalse(TEXT("A foreign account cannot recover the selected character"), Fixture.Run->PurchaseShopOffer(Outsider, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
    TestFalse(TEXT("The local owner cannot redirect a purchase to an AI companion"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Fixture.Member(0).CharacterId, OfferId, Fixture.Error));
    TestFalse(TEXT("Recovery cannot be redirected to an AI companion"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Fixture.Member(0).CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
    TestFalse(TEXT("Unknown characters are rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, FGuid::NewGuid(), OfferId, Fixture.Error));
    TestFalse(TEXT("Unknown products are rejected"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, TEXT("Missing"), Fixture.Error));
    TestTrue(TEXT("All rejected requests preserve complete party data and durable bytes"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && DiskBefore == Fixture.ReadBytes());
    TestEqual(TEXT("Rejected requests publish no state change"), Events, 0);
    Fixture.Run->OnRunStateChanged.Clear();

    FSkillShopFixture DeadFixture;
    if (!DeadFixture.Initialize() || !DeadFixture.ReachShop(TEXT("Shop_01"), 3)) return false;
    const FRunPartyMember Dead = DeadFixture.Member(3);
    TestFalse(TEXT("A dead direct-control character cannot purchase"), DeadFixture.Run->PurchaseShopOffer(Dead.OwnerAccountId, Dead.CharacterId, DeadFixture.Run->GetSkillShopState().Offers[0].OfferId, DeadFixture.Error));
    TestFalse(TEXT("Shop recovery cannot resurrect a dead character"), DeadFixture.Run->PurchaseShopOffer(Dead.OwnerAccountId, Dead.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), DeadFixture.Error));
    TestTrue(TEXT("The rejected dead character retains its gold and unarmed loadout"), DeadFixture.Member(3).Gold == Dead.Gold && DeadFixture.Member(3).Skills.Num() == 1);

    FSkillShopFixture PoorFixture;
    TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog(DuplicateObject<UPartyDefinitionDataAsset>(PoorFixture.Run->PartyDefinition.Get(), GetTransientPackage()));
    TStrongObjectPtr<URunEncounterPoolDataAsset> Pool(NewObject<URunEncounterPoolDataAsset>());
    Pool->StartingGold = 0;
    for (FRunSkillShopOffer& Offer : Pool->FixedSkillOffers) Offer.Price = 100;
    Pool->Recovery.Price = 100;
    Catalog->RunEncounterPool = Pool.Get();
    PoorFixture.Run->PartyDefinition = Catalog.Get();
    if (!PoorFixture.Initialize() || !PoorFixture.ReachShop(TEXT("Shop_03"))) return false;
    const FRunPartyMember Poor = PoorFixture.Member(3);
    TestFalse(TEXT("An authored insufficient balance rejects a purchase"), PoorFixture.Run->PurchaseShopOffer(Poor.OwnerAccountId, Poor.CharacterId, PoorFixture.Run->GetSkillShopState().Offers[0].OfferId, PoorFixture.Error));
    TestFalse(TEXT("Insufficient funds also reject recovery"), PoorFixture.Run->PurchaseShopOffer(Poor.OwnerAccountId, Poor.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), PoorFixture.Error));
    TestTrue(TEXT("Insufficient funds neither subtract gold nor teach a skill"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Poor, &PoorFixture.Member(3), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillShopAtomicPersistenceTest, "ProjectA.Run.Shop.AtomicPurchaseAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillShopAtomicPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!TestTrue(TEXT("The purchase fixture reaches a durable shop"), Fixture.Initialize(3, true) && Fixture.ReachShop())) return false;
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
        bEventObservedDurablePurchase = Durable && Durable->Party[3].Gold == Buyer.Gold - 1 && Durable->Party[3].Skills.Contains(Offer.Skill) && Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).Skills.Contains(Offer.Skill);
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed durable write rejects the purchase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error));
    TestTrue(TEXT("Failed writes preserve every member and the complete original file"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && BeforeBytes == Fixture.ReadBytes());
    TestEqual(TEXT("Failed persistence emits no purchase notification"), Events, 0);
    TestTrue(TEXT("The same purchase can retry after storage recovers"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error));
    TestTrue(TEXT("The single success notification observes committed memory and disk"), Events == 1 && bEventObservedDurablePurchase);
    TestFalse(TEXT("A retried successful request cannot charge twice"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error));
    TestEqual(TEXT("A duplicate request adds no notification"), Events, 1);
    Fixture.Run->OnRunStateChanged.Clear();
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Standalone Continue restores the purchased shop"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Restore preserves ownership identity balances and all skill paths"), SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Restored->GetRunIdentity(), 0));
    TestTrue(TEXT("Restore preserves the frozen catalog"), FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetSkillShopState(), &Restored->GetSkillShopState(), 0));
    TestFalse(TEXT("A reloaded purchase remains owned"), Restored->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error));
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
    TestFalse(TEXT("Recovery rejects a failed durable write"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
    TestFalse(TEXT("Recovery storage failure supplies a visible reason"), Fixture.Error.IsEmpty());
    TestTrue(TEXT("Failed recovery preserves all HP gold skills and disk bytes"), SameShopParty(Before, Fixture.Run->GetPartyMembers()) && BeforeBytes == Fixture.ReadBytes());
    TestEqual(TEXT("Failed recovery publishes no state change"), Events, 0);
    if (!TestTrue(TEXT("The same recovery succeeds after storage recovers"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error))) return false;
    TestTrue(TEXT("Recovery notification observes HP and gold committed together"), Events == 1 && bEventObservedDurableRecovery);
    for (int32 Index = 0; Index < Before.Num(); ++Index)
    {
        if (Index != 3) TestTrue(TEXT("Recovery leaves every companion unchanged"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Before[Index], &Fixture.Member(Index), 0));
    }
    const TArray<uint8> RecoveredBytes = Fixture.ReadBytes();
    TestFalse(TEXT("A repeated request at full HP cannot charge again"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
    TestTrue(TEXT("Full HP rejection preserves balance save bytes and notification count"), Fixture.Member(3).Gold == Buyer.Gold - 1 && Fixture.Member(3).CurrentHP == Profession.MaxHP && RecoveredBytes == Fixture.ReadBytes() && Events == 1);
    Fixture.Run->OnRunStateChanged.Clear();
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Continue restores the recovered shop"), Restored->LoadStandaloneCheckpoint(Fixture.Error))) return false;
    TestTrue(TEXT("Recovery reload preserves party identity and frozen skill products"), SameShopParty(Fixture.Run->GetPartyMembers(), Restored->GetPartyMembers()) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Restored->GetRunIdentity(), 0) && FRunSkillShopState::StaticStruct()->CompareScriptStruct(&Catalog, &Restored->GetSkillShopState(), 0));
    TestTrue(TEXT("Recovered HP persists into the next battle without refilling gold"), Restored->LeaveRunEncounter() && Restored->BeginEncounter(TEXT("Combat_02")) && Restored->MarkCombatStarted() && Restored->GetPartyMembers()[3].CurrentHP == Profession.MaxHP && Restored->GetPartyMembers()[3].Gold == Buyer.Gold - 1 && Restored->GetPartyMembers()[3].Skills == Buyer.Skills);
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
    TestFalse(TEXT("A legacy shop cannot invent a new purchase"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, TEXT("BPDA_swoard_attack"), Fixture.Error));
    TestFalse(TEXT("A legacy shop without a balance cannot purchase recovery"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, FRunSkillShopState::GetRecoveryOfferId(), Fixture.Error));
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
    for (int32 Case = 0; Case < 6; ++Case)
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
        default:
            Invalid->Phase = static_cast<ERunPhase>(255);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemShopPersistenceTest, "ProjectA.Run.Shop.ItemPurchaseRerollAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemShopPersistenceTest::RunTest(const FString& Parameters)
{
    FSkillShopFixture Fixture;
    if (!Fixture.Initialize(3, true) || !Fixture.ReachShop(FRunItemShopState::GetEncounterId())) return false;
    const FRunPartyMember Buyer = Fixture.Member(3);
    const FRunItemShopState Initial = Fixture.Run->GetItemShopState();
    if (!TestEqual(TEXT("The item shop displays exactly five offers"), Initial.Offers.Num(), 5)) return false;
    TestEqual(TEXT("The whole authored CSV is available"), Initial.Catalog.Num(), 295);
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
