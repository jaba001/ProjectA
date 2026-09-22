#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
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
        for (int32 ShopIndex = 1; ShopIndex <= 3; ++ShopIndex)
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
    if (!DeadFixture.Initialize() || !DeadFixture.ReachShop(TEXT("Shop_02"), 3)) return false;
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
    Legacy->GoldRewardState = FRunGoldRewardState();
    for (FRunPartyMember& Member : Legacy->Party)
    {
        Member.Gold = 0;
        Member.bHasSkillLoadout = false;
        Member.Skills.Reset();
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

#endif
