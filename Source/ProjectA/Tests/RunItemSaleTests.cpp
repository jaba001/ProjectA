#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunDungeonPlan.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemSaleRules.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/RunRewardTestHelpers.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    FRunItemDefinition MakeSaleTestItem(const TCHAR* Name, const TCHAR* Tag, int32 Price = 5)
    {
        FRunItemDefinition Item;
        Item.Asset = FSoftObjectPath(FString::Printf(TEXT("/Game/User_JeHoon/Validation/T12/%s.%s"), Name, Name));
        Item.DisplayName = FText::FromString(Name);
        Item.Tags.AddTag(FGameplayTag::RequestGameplayTag(Tag));
        Item.Price = Price;
        return Item;
    }

    FRunItemSaleCommand MakeSaleCommand(const FRunPartyMember& Member, const FRunItemShopState& Shop, const FRunEncounterProgress& Encounter, int32 ItemIndex)
    {
        FRunItemSaleCommand Command;
        Command.CharacterId = Member.CharacterId;
        Command.ItemIndex = ItemIndex;
        Command.Asset = Member.Items[ItemIndex].Asset;
        Command.ItemInstanceId = Member.Items[ItemIndex].ItemInstanceId;
        Command.ExpectedEquipmentRevision = Member.Equipment.Revision;
        Command.ExpectedShopRevision = Shop.Revision;
        Command.EncounterId = Encounter.SelectedEncounterId;
        return Command;
    }

    bool SameSaleParty(const TArray<FRunPartyMember>& Left, const TArray<FRunPartyMember>& Right)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], 0)) return false;
        }
        return true;
    }

    struct FItemSaleFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("ItemSale_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FText Error;

        FItemSaleFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
        }

        ~FItemSaleFixture()
        {
            Run->OnRunStateChanged.Clear();
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        bool Initialize(bool bTarget = false)
        {
            if (!Run->PartyDefinition) return false;
            TArray<FRunPartyMember> Party;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.ClassId = Index == 0 ? TEXT("Warrior") : TEXT("Archer");
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Sale owner %d"), Index));
                Member.bCreated = true;
                Member.bPlayerControlled = Index == 0;
            }
            return bTarget ? Run->InitializeTargetRun(Party, Error) : Run->InitializeRun(Party, Error);
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
            return Bytes;
        }

        bool ReachPrototypeItemShop()
        {
            if (!Initialize() || !Run->BeginEncounter(TEXT("Combat_01")) || !Run->MarkCombatStarted()) return false;
            for (const FRunPartyMember& Member : Run->GetPartyMembers()) Run->UpdatePartyMemberHP(Member.SlotIndex, 70.0f);
            return Run->CompleteEncounter(ECombatResult::Victory) && RunRewardTests::CollectPendingGoldRewards(Run.Get()) && Run->ContinueRun() && Run->SelectRunEncounter(TEXT("Shop_02"));
        }

        bool ReachRewardItemShop()
        {
            if (!Initialize(true)) return false;
            TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Saved) return false;
            bool bFound = false;
            for (int32 Seed = 0; Seed < 10000; ++Seed)
            {
                // Freeze a basic shop after the first reward so the transaction test is independent of random themes.
                // 거래 검증이 무작위 테마와 무관하도록 첫 보상 뒤의 기본상점을 고정합니다.
                Saved->TargetRun.EncounterSeed = Seed;
                TArray<FRunEncounterOffer> AfterReward;
                if (!UTargetRunDefinitionDataAsset::BuildOffers(Saved->TargetRun, 1, 0, AfterReward)) return false;
                if (!AfterReward.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag() == FRunEncounterOffer::GetBasicItemShopTag(); })) continue;
                if (!UTargetRunDefinitionDataAsset::BuildOffers(Saved->TargetRun, 0, 0, Saved->EncounterProgress.Offers) || !RunDungeonPlan::Build(*Saved.Get(), Saved->DungeonState, Error)) return false;
                bFound = true;
                break;
            }
            if (!bFound || !FRunCheckpointStorage::Save(Saved.Get(), Slot, Error) || !Run->LoadStandaloneCheckpoint(Error)) return false;
            while (Run->GetPhase() == ERunPhase::EncounterChoice)
            {
                if (Run->GetEncounterProgress().Offers.IsEmpty() || !Run->SelectRunEncounter(Run->GetEncounterProgress().Offers[0].EncounterId) || !Run->LeaveRunEncounter()) return false;
            }
            if (!Run->BeginEncounter(TEXT("TargetCombat_01"), RunPveDifficulty::GetMediumTag()) || !Run->MarkCombatStarted() || !Run->CompleteEncounter(ECombatResult::Victory) || !RunRewardTests::CollectPendingGoldRewards(Run.Get()) || !Run->ContinueRun()) return false;
            const FRunEncounterOffer* Basic = Run->GetEncounterProgress().Offers.FindByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag() == FRunEncounterOffer::GetBasicItemShopTag(); });
            return Basic && Run->SelectRunEncounter(Basic->EncounterId);
        }

        bool Reload()
        {
            TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Instance.Get()));
            Restored->EnableCheckpointSaving(Slot);
            if (!Restored->LoadStandaloneCheckpoint(Error)) return false;
            Run->OnRunStateChanged.Clear();
            Run.Reset(Restored.Get());
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemSaleRulesTest, "ProjectA.Run.ItemSale.PriceOwnershipIdentityAndShiftedEquipment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemSaleRulesTest::RunTest(const FString& Parameters)
{
    FRunPartyMember Member;
    Member.CharacterId = FGuid::NewGuid();
    Member.bCreated = true;
    Member.bHasSkillLoadout = true;
    Member.CurrentHP = 20.0f;
    Member.Gold = 10;
    Member.Equipment.bHasLoadout = true;
    Member.Equipment.Revision = 4;
    Member.Items = {MakeSaleTestItem(TEXT("UnsupportedSaleSword"), TEXT("Item.Weapon.Sword")), MakeSaleTestItem(TEXT("SaleDagger"), TEXT("Item.Weapon.Dagger")), MakeSaleTestItem(TEXT("SaleShield"), TEXT("Item.Weapon.Shield"))};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FRunEquipmentSlot& Slot = Member.Equipment.Slots.AddDefaulted_GetRef();
        Slot.SlotTag = URunEquipmentCatalog::GetWeaponSlot(Index);
        Slot.ItemIndex = Index + 1;
    }
    Member.Skills = {FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack.BPDA_DefaulatAttack"))};
    FRunEncounterProgress Encounter;
    Encounter.SchemaVersion = 2;
    FRunEncounterOffer& Visit = Encounter.Offers.AddDefaulted_GetRef();
    Visit.EncounterId = TEXT("SaleShop");
    Visit.EncounterTag = FRunEncounterOffer::GetBasicItemShopTag();
    Encounter.SelectedEncounterId = Visit.EncounterId;
    FRunItemShopState Shop;
    Shop.SchemaVersion = 1;
    Shop.Revision = 5;
    const FRunItemSaleCommand Command = MakeSaleCommand(Member, Shop, Encounter, 0);
    FText Error;
    TestFalse(TEXT("The bag item intentionally has no supported equipment profile"), URunEquipmentCatalog::Get().ResolveProfile(Member.Items[0]) != nullptr);
    const int32 Prices[] = {MIN_int32, -1, 0, 1, 2, 3, 5, MAX_int32};
    const int32 ExpectedPrices[] = {0, 0, 0, 1, 1, 1, 2, 1073741823};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Prices); ++Index)
    {
        FRunItemDefinition Item = Member.Items[0];
        Item.Price = Prices[Index];
        TestEqual(TEXT("Price uses a safe half floor with a one-gold minimum only for positive prices"), RunItemSaleRules::GetPrice(Item), ExpectedPrices[Index]);
    }
    for (const FGameplayTag Tag : {FRunEncounterOffer::GetBasicItemShopTag(), FRunEncounterOffer::GetRarityItemShopTag(), FRunEncounterOffer::GetTagItemShopTag(), FRunEncounterOffer::GetItemShopTag()})
    {
        Encounter.Offers[0].EncounterTag = Tag;
        TestTrue(TEXT("All item shop tags buy eligible inventory regardless of their sale-stock query"), RunItemSaleRules::Validate(Member, Shop, Encounter, Command, Error));
    }
    Encounter.Offers[0].EncounterTag = FRunEncounterOffer::GetBasicItemShopTag();
    for (int32 Case = 0; Case < 13; ++Case)
    {
        FRunPartyMember Candidate = Member;
        FRunItemShopState CandidateShop = Shop;
        FRunEncounterProgress CandidateEncounter = Encounter;
        FRunItemSaleCommand Invalid = Command;
        if (Case == 0) Invalid.CharacterId = FGuid::NewGuid();
        else if (Case == 1) Invalid.Asset = Member.Items[1].Asset;
        else if (Case == 2) Invalid.ItemInstanceId = FGuid::NewGuid();
        else if (Case == 3) --Invalid.ExpectedEquipmentRevision;
        else if (Case == 4) --Invalid.ExpectedShopRevision;
        else if (Case == 5) Invalid.EncounterId = TEXT("AnotherVisit");
        else if (Case == 6) Invalid = MakeSaleCommand(Member, Shop, Encounter, 1);
        else if (Case == 7) Candidate.Gold = MAX_int32;
        else if (Case == 8) Candidate.CurrentHP = 0.0f;
        else if (Case == 9) Candidate.Items[0].Price = 0;
        else if (Case == 10) Candidate.Equipment.Revision = Invalid.ExpectedEquipmentRevision = MAX_int32;
        else if (Case == 11) CandidateShop.Revision = Invalid.ExpectedShopRevision = MAX_int32;
        else CandidateEncounter.Offers[0].EncounterTag = FRunEncounterOffer::GetConsumableShopTag();
        const FRunPartyMember Before = Candidate;
        const FRunItemShopState BeforeShop = CandidateShop;
        TestFalse(FString::Printf(TEXT("Invalid sale case %d is rejected"), Case), RunItemSaleRules::Apply(Candidate, CandidateShop, CandidateEncounter, Invalid, Error));
        TestTrue(TEXT("Rejected sale preserves all item, equipment, skill and stock fields"), !Error.IsEmpty() && FRunPartyMember::StaticStruct()->CompareScriptStruct(&Before, &Candidate, 0) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&BeforeShop, &CandidateShop, 0));
    }
    const FRunPartyMember Before = Member;
    if (!TestTrue(TEXT("An unsupported legacy bag item can be sold"), RunItemSaleRules::Apply(Member, Shop, Encounter, Command, Error))) return false;
    TestTrue(TEXT("Removing an earlier bag copy shifts both equipped indices without changing copies, skills or loadout mode"), Member.Items.Num() == 2 && RunItemShopCatalog::IsSameDefinition(Member.Items[0], Before.Items[1]) && RunItemShopCatalog::IsSameDefinition(Member.Items[1], Before.Items[2]) && Member.Equipment.Slots[0].ItemIndex == 0 && Member.Equipment.Slots[1].ItemIndex == 1 && Member.Equipment.bHasLoadout == Before.Equipment.bHasLoadout && Member.Skills == Before.Skills && Member.InnateSkills == Before.InnateSkills && Member.Gold == 12 && Member.Equipment.Revision == 5 && Shop.Revision == 6);
    TestFalse(TEXT("An old request cannot sell the item shifted into its former index"), RunItemSaleRules::Apply(Member, Shop, Encounter, Command, Error));
    Member = Before;
    Member.Equipment = FRunEquipmentState();
    const FRunItemSaleCommand LegacyCommand = MakeSaleCommand(Member, Shop, Encounter, 0);
    if (!TestTrue(TEXT("A legacy inventory sale advances its revision without enabling equipment"), RunItemSaleRules::Apply(Member, Shop, Encounter, LegacyCommand, Error))) return false;
    TestTrue(TEXT("Legacy skills and visual policy remain unchanged after sale"), !Member.Equipment.bHasLoadout && Member.Equipment.Revision == 1 && Member.Equipment.Slots.IsEmpty() && Member.Skills == Before.Skills && RunEquipmentRules::Validate(Member, Error));
    FRunPartyMember Uncreated;
    Uncreated.Equipment.Revision = 1;
    TestFalse(TEXT("An uncreated empty slot cannot acquire an inventory revision"), RunEquipmentRules::Validate(Uncreated, Error));
    Uncreated.bCreated = true;
    TestFalse(TEXT("A character without a skill loadout cannot acquire a legacy sale revision"), RunEquipmentRules::Validate(Uncreated, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemSaleAtomicTest, "ProjectA.Run.ItemSale.PublicAuthorityAtomicWriteRetryAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemSaleAtomicTest::RunTest(const FString& Parameters)
{
    FItemSaleFixture Fixture;
    if (!TestTrue(TEXT("A durable prototype item shop opens through public APIs"), Fixture.ReachPrototypeItemShop())) return false;
    const FRunPartyMember Buyer = Fixture.Run->GetPartyMembers()[0];
    const FRunItemShopOffer Offer = Fixture.Run->GetItemShopState().Offers[0];
    if (!TestTrue(TEXT("Purchase creates a real bag copy"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, Offer.OfferId, Fixture.Error, Fixture.Run->GetItemShopState().Revision))) return false;
    const FRunPartyMember Purchased = Fixture.Run->GetPartyMembers()[0];
    const FRunItemSaleCommand Command = MakeSaleCommand(Purchased, Fixture.Run->GetItemShopState(), Fixture.Run->GetEncounterProgress(), Purchased.Items.Num() - 1);
    FRunAccountId WrongOwner = Buyer.OwnerAccountId;
    WrongOwner.Subject += TEXT("_other");
    TestFalse(TEXT("A foreign account cannot sell the player's copy"), Fixture.Run->SellItem(WrongOwner, Command, Fixture.Error));
    const FRunPartyMember Companion = Fixture.Run->GetPartyMembers()[1];
    FRunItemSaleCommand CompanionCommand = MakeSaleCommand(Companion, Fixture.Run->GetItemShopState(), Fixture.Run->GetEncounterProgress(), 0);
    TestFalse(TEXT("The shared owner cannot sell an AI companion's inventory"), Fixture.Run->CanSellItem(Buyer.OwnerAccountId, CompanionCommand, Fixture.Error));
    const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
    const FRunItemShopState Stock = Fixture.Run->GetItemShopState();
    const TArray<uint8> Bytes = Fixture.ReadBytes();
    int32 Publications = 0;
    bool bDurableWhenPublished = false;
    Fixture.Run->OnRunStateChanged.AddLambda([&Fixture, &Publications, &bDurableWhenPublished]()
    {
        ++Publications;
        TStrongObjectPtr<URunSaveGame> Durable(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
        bDurableWhenPublished = Durable && SameSaleParty(Durable->Party, Fixture.Run->GetPartyMembers()) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&Durable->ItemShopState, &Fixture.Run->GetItemShopState(), 0);
    });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Injected save failure rejects the complete sale"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error));
    TestTrue(TEXT("Save failure preserves party, gold, equipment, skills, stock, bytes and publications"), SameSaleParty(Before, Fixture.Run->GetPartyMembers()) && FRunItemShopState::StaticStruct()->CompareScriptStruct(&Stock, &Fixture.Run->GetItemShopState(), 0) && Bytes == Fixture.ReadBytes() && Publications == 0);
    if (!TestTrue(TEXT("The same captured sale can retry after write failure"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error))) return false;
    const FRunPartyMember Sold = Fixture.Run->GetPartyMembers()[0];
    TestTrue(TEXT("The one-gold sale preserves every prior copy and publishes only after saving"), Publications == 1 && bDurableWhenPublished && Sold.Items.Num() == Purchased.Items.Num() - 1 && Sold.Gold == Purchased.Gold + 1 && Sold.Skills == Purchased.Skills && Sold.InnateSkills == Purchased.InnateSkills && Sold.Equipment.Revision == Purchased.Equipment.Revision + 1);
    FRunItemShopState ExpectedStock = Stock;
    ++ExpectedStock.Revision;
    TestTrue(TEXT("Sale has no buyback or reroll and preserves every displayed product"), FRunItemShopState::StaticStruct()->CompareScriptStruct(&ExpectedStock, &Fixture.Run->GetItemShopState(), 0));
    TestFalse(TEXT("Duplicate sale request cannot grant gold twice"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error));
    FRunEquipmentCommand OldDrag;
    OldDrag.CharacterId = Buyer.CharacterId;
    OldDrag.ItemIndex = Command.ItemIndex;
    OldDrag.TargetSlot = URunEquipmentCatalog::GetWeaponSlot(0);
    OldDrag.ExpectedRevision = Command.ExpectedEquipmentRevision;
    TestFalse(TEXT("A pre-sale drag is invalid after inventory removal"), Fixture.Run->ChangeEquipment(Buyer.OwnerAccountId, OldDrag, Fixture.Error));
    const TArray<FRunPartyMember> SoldParty = Fixture.Run->GetPartyMembers();
    const TArray<uint8> SoldBytes = Fixture.ReadBytes();
    if (!TestTrue(TEXT("A fresh subsystem reloads the sold inventory"), Fixture.Reload())) return false;
    TestTrue(TEXT("Continue preserves sale results without rewriting or paying again"), SameSaleParty(SoldParty, Fixture.Run->GetPartyMembers()) && SoldBytes == Fixture.ReadBytes());
    TestFalse(TEXT("Duplicate sale remains rejected after Continue"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error));
    if (!TestTrue(TEXT("Leave the item shop normally"), Fixture.Run->LeaveRunEncounter())) return false;
    TestFalse(TEXT("An old sale is unavailable after leaving the shop"), Fixture.Run->CanSellItem(Buyer.OwnerAccountId, Command, Fixture.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemSaleRewardReceiptTest, "ProjectA.Run.ItemSale.RewardReceiptAtomicSaleContinueAndCorruption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemSaleRewardReceiptTest::RunTest(const FString& Parameters)
{
    FItemSaleFixture Fixture;
    if (!TestTrue(TEXT("A new target reaches a real item reward and its following basic shop"), Fixture.ReachRewardItemShop())) return false;
    const FRunPartyMember Buyer = Fixture.Run->GetPartyMembers()[0];
    const FRunGoldRewardState Reward = Fixture.Run->GetGoldRewardState();
    if (!TestTrue(TEXT("An unsold schema-two receipt exists"), Reward.SchemaVersion == 2 && Reward.Claims.Num() == 1 && !Reward.Claims[0].bItemSold)) return false;
    const FRunItemDefinition Award = Reward.ItemChoices[Reward.Claims[0].ChoiceIndex];
    const int32 ItemIndex = Buyer.Items.IndexOfByPredicate([&Award](const FRunItemDefinition& Item) { return Item.ItemInstanceId == Award.ItemInstanceId; });
    if (!TestTrue(TEXT("The selected award is owned as its exact original bag copy"), Buyer.Items.IsValidIndex(ItemIndex) && RunItemShopCatalog::IsSameDefinition(Award, Buyer.Items[ItemIndex]))) return false;
    const FRunItemSaleCommand Command = MakeSaleCommand(Buyer, Fixture.Run->GetItemShopState(), Fixture.Run->GetEncounterProgress(), ItemIndex);
    const TArray<uint8> Before = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("A failed reward sale cannot publish a sold receipt"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error));
    TestTrue(TEXT("Failure retains the award, its unsold receipt, gold and exact bytes"), Before == Fixture.ReadBytes() && !Fixture.Run->GetGoldRewardState().Claims[0].bItemSold && Fixture.Run->GetPartyMembers()[0].Gold == Buyer.Gold && RunItemShopCatalog::IsSameDefinition(Fixture.Run->GetPartyMembers()[0].Items[ItemIndex], Award));
    if (!TestTrue(TEXT("Reward sale succeeds without discarding the prior claim"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error) && Fixture.Reload())) return false;
    TestTrue(TEXT("The sold receipt survives Continue and cannot pay or claim twice"), Fixture.Run->GetGoldRewardState().Claims[0].bItemSold && Fixture.Run->GetGoldRewardState().Claims[0].CharacterId == Buyer.CharacterId && Fixture.Run->GetGoldRewardState().Claims[0].ChoiceIndex == Reward.Claims[0].ChoiceIndex && !Fixture.Run->SelectGoldReward(Buyer.OwnerAccountId, Buyer.CharacterId, Reward.NodeId, Reward.Claims[0].ChoiceIndex, Fixture.Error) && !Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error));
    TStrongObjectPtr<URunSaveGame> Sold(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Sold) return false;
    for (int32 Case = 0; Case < 3; ++Case)
    {
        TStrongObjectPtr<URunSaveGame> Corrupt(DuplicateObject<URunSaveGame>(Sold.Get(), GetTransientPackage()));
        if (Case == 0) Corrupt->GoldRewardState.Claims[0].bItemSold = false;
        else if (Case == 1) Corrupt->Party[0].Items.Add(Award);
        else Corrupt->GoldRewardState.Claims[0].CharacterId = Corrupt->Party[1].CharacterId;
        if (!TestTrue(TEXT("Write an isolated malformed receipt fixture"), FRunCheckpointStorage::Save(Corrupt.Get(), Fixture.Slot, Fixture.Error))) return false;
        TStrongObjectPtr<URunStateSubsystem> Rejected(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
        Rejected->EnableCheckpointSaving(Fixture.Slot);
        TestFalse(TEXT("Missing sold marker, reintroduced sold copy or changed recipient cannot load"), Rejected->LoadStandaloneCheckpoint(Fixture.Error));
    }
    if (!FRunCheckpointStorage::Save(Sold.Get(), Fixture.Slot, Fixture.Error) || !Fixture.Reload()) return false;
    const FRunItemShopOffer NextOffer = Fixture.Run->GetItemShopState().Offers[0];
    TestTrue(TEXT("Subsequent shop commits continue to accept the valid sold receipt"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, NextOffer.OfferId, Fixture.Error, Fixture.Run->GetItemShopState().Revision) && Fixture.Run->LeaveRunEncounter() && Fixture.Reload());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunItemSaleLegacyTest, "ProjectA.Run.ItemSale.LegacyInventoryAndGoldReceiptCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunItemSaleLegacyTest::RunTest(const FString& Parameters)
{
    FItemSaleFixture Fixture;
    if (!TestTrue(TEXT("A prototype with original acquired-skill policy reaches its item shop"), Fixture.ReachPrototypeItemShop())) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Legacy || Legacy->WeaponSkillAcquisitionVersion != 0 || Legacy->GoldRewardState.Claims.Num() != 1) return false;
    const FRunItemDefinition Unsupported = MakeSaleTestItem(TEXT("LegacySaleSword"), TEXT("Item.Weapon.Sword"));
    Legacy->ItemShopState.Catalog.Add(Unsupported);
    Legacy->Party[0].Items.Add(Unsupported);
    Legacy->Party[0].Equipment = FRunEquipmentState();
    if (!TestTrue(TEXT("An old no-loadout inventory and unsupported bag copy remain loadable"), FRunCheckpointStorage::Save(Legacy.Get(), Fixture.Slot, Fixture.Error) && Fixture.Reload())) return false;
    const FRunPartyMember Buyer = Fixture.Run->GetPartyMembers()[0];
    const FRunItemSaleCommand Command = MakeSaleCommand(Buyer, Fixture.Run->GetItemShopState(), Fixture.Run->GetEncounterProgress(), Buyer.Items.Num() - 1);
    if (!TestTrue(TEXT("Selling a legacy unsupported copy persists its new inventory revision"), Fixture.Run->SellItem(Buyer.OwnerAccountId, Command, Fixture.Error) && Fixture.Reload())) return false;
    const FRunPartyMember Sold = Fixture.Run->GetPartyMembers()[0];
    TestTrue(TEXT("Old skills, unconfigured equipment visuals and schema-one gold receipt remain unchanged"), Sold.Equipment.Revision == 1 && !Sold.Equipment.bHasLoadout && Sold.Equipment.Slots.IsEmpty() && Sold.Skills == Buyer.Skills && Sold.InnateSkills == Buyer.InnateSkills && Sold.Gold == Buyer.Gold + 2 && Fixture.Run->GetGoldRewardState().SchemaVersion == 1 && !Fixture.Run->GetGoldRewardState().Claims[0].bItemSold);
    TStrongObjectPtr<URunSaveGame> Corrupt(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Fixture.Slot, Fixture.Error)));
    if (!Corrupt) return false;
    Corrupt->GoldRewardState.Claims[0].bItemSold = true;
    if (!FRunCheckpointStorage::Save(Corrupt.Get(), Fixture.Slot, Fixture.Error)) return false;
    TStrongObjectPtr<URunStateSubsystem> Rejected(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Rejected->EnableCheckpointSaving(Fixture.Slot);
    TestFalse(TEXT("A gold-only receipt cannot claim that an item was sold"), Rejected->LoadStandaloneCheckpoint(Fixture.Error));
    return true;
}

#endif
