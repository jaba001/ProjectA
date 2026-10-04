#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Round/CombatPlanValidator.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Unit/UnitDataRules.h"
#include "Engine/GameInstance.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FRecoveryFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("Recovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FText Error;

        FRecoveryFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
        }

        ~FRecoveryFixture()
        {
            Run->OnRunStateChanged.Clear();
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        bool Initialize(bool bTarget = true)
        {
            if (!Run->PartyDefinition) return false;
            TArray<FRunPartyMember> Party;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.ClassId = TEXT("Warrior");
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Recovery %d"), Index));
                Member.bCreated = true;
                Member.bPlayerControlled = Index == 0;
            }
            return bTarget ? Run->InitializeTargetRun(Party, Error) : Run->InitializeRun(Party, Error);
        }

        bool SetFixtureHealth(float HP, bool bNoGold = false)
        {
            TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
            if (!Save) return false;
            // Seed only this disposable saved scenario; production purchase still runs through its public transaction.
            // 일회성 저장 시나리오만 구성하며 실제 구매는 공개 트랜잭션을 통해 실행합니다.
            Save->Party[0].CurrentHP = HP;
            if (bNoGold) Save->Party[0].Gold = 0;
            return FRunCheckpointStorage::Save(Save.Get(), Slot, Error) && Run->LoadStandaloneCheckpoint(Error);
        }

        bool SelectService(FGameplayTag Tag)
        {
            for (int32 Visit = 0; Visit < 3 && Run->GetPhase() == ERunPhase::EncounterChoice; ++Visit)
            {
                const TArray<FRunEncounterOffer> Offers = Run->GetEncounterProgress().Offers;
                const FRunEncounterOffer* Desired = Offers.FindByPredicate([Tag](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag() == Tag; });
                if (Desired) return Run->SelectRunEncounter(Desired->EncounterId);
                if (Offers.IsEmpty() || !Run->SelectRunEncounter(Offers[0].EncounterId) || !Run->LeaveRunEncounter()) return false;
            }
            return false;
        }

        TArray<uint8> Bytes() const
        {
            TArray<uint8> Result;
            UGameplayStatics::LoadDataFromSlot(Result, Slot, 0);
            return Result;
        }
    };

    bool SameRecoveryParty(const TArray<FRunPartyMember>& Left, const TArray<FRunPartyMember>& Right)
    {
        if (Left.Num() != Right.Num()) return false;
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], 0)) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRecoveryLegacyCompatibilityTest, "ProjectA.Run.Recovery.LegacyNoGrantAndNewRunFrozenStock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRecoveryLegacyCompatibilityTest::RunTest(const FString& Parameters)
{
    FRecoveryFixture Legacy;
    if (!TestTrue(TEXT("The existing prototype initializes unchanged"), Legacy.Initialize(false))) return false;
    TestEqual(TEXT("The old route remains ten combats"), Legacy.Run->GetNodes().Num(), 10);
    TestTrue(TEXT("Older run initialization grants no consumables"), Legacy.Run->GetPartyMembers().ContainsByPredicate([](const FRunPartyMember& Member) { return Member.Consumables.IsEmpty(); }));
    if (!TestTrue(TEXT("The old save reloads without recovery migration"), Legacy.Run->LoadStandaloneCheckpoint(Legacy.Error))) return false;
    for (const FRunPartyMember& Member : Legacy.Run->GetPartyMembers()) TestTrue(TEXT("Reload grants no retroactive stock"), Member.Consumables.IsEmpty());
    FRecoveryFixture Target;
    if (!TestTrue(TEXT("The new target route creates its frozen trial recovery contract"), Target.Initialize())) return false;
    TestEqual(TEXT("The new target route contains twenty combats"), Target.Run->GetNodes().Num(), 20);
    const FRunRecoveryState Rules = Target.Run->GetRecoveryState();
    USkillDefinitionDataAsset* Consumable = Cast<USkillDefinitionDataAsset>(Rules.HealingSkill.TryLoad());
    TestFalse(TEXT("The stock-only ability cannot be equipped or acquired as a normal skill"), UnitDataRules::ValidateSkills({Consumable}, true, Target.Error));
    TestTrue(TEXT("Trial service values are saved"), Rules.SchemaVersion == 1 && Rules.StartingQuantity == 1 && Rules.ConsumablePrice == 1 && Rules.RecoveryHP == 25.f && Rules.RecoveryPrice == 1 && Rules.RevivalFraction == .25f && Rules.RevivalPrice == 1);
    const TArray<FRunPartyMember> Before = Target.Run->GetPartyMembers();
    for (const FRunPartyMember& Member : Before) TestTrue(TEXT("Each new character owns exactly one tagged consumable outside acquired skill slots"), Member.Consumables.Num() == 1 && Member.Consumables[0].Quantity == 1 && Member.Consumables[0].Skill == Rules.HealingSkill && Member.Skills.Num() == 1);
    if (!Target.Run->LoadStandaloneCheckpoint(Target.Error)) return false;
    TestTrue(TEXT("Continue preserves quantities and frozen rules without another starting grant"), SameRecoveryParty(Before, Target.Run->GetPartyMembers()) && FRunRecoveryState::StaticStruct()->CompareScriptStruct(&Rules, &Target.Run->GetRecoveryState(), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRecoveryTransactionTest, "ProjectA.Run.Recovery.ServiceOwnershipAtomicWriteDuplicateAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunRecoveryTransactionTest::RunTest(const FString& Parameters)
{
    for (FGameplayTag Tag : {FRunEncounterOffer::GetRecoveryTag(), FRunEncounterOffer::GetConsumableShopTag(), FRunEncounterOffer::GetRevivalTag()})
    {
        const bool bRevival = Tag == FRunEncounterOffer::GetRevivalTag();
        const bool bConsumable = Tag == FRunEncounterOffer::GetConsumableShopTag();
        FRecoveryFixture Fixture;
        if (!TestTrue(TEXT("The public route reaches each tagged service"), Fixture.Initialize() && Fixture.SetFixtureHealth(bRevival ? 0.f : 30.f) && Fixture.SelectService(Tag))) return false;
        const TArray<FRunPartyMember> Before = Fixture.Run->GetPartyMembers();
        const FRunPartyMember Buyer = Before[0];
        const FName OfferId = Tag.GetTagName();
        const int32 Revision = Fixture.Run->GetRecoveryState().Revision;
        const TArray<uint8> OriginalBytes = Fixture.Bytes();
        const FGameplayViewState View = FGameplayViewState::FromRun(Fixture.Run.Get(), FText::GetEmpty());
        TestTrue(TEXT("The server presentation exposes only the eligible original human buyer, including revival after death"), View.ShopBuyerCharacterIds.Contains(Buyer.CharacterId) && !View.ShopBuyerCharacterIds.Contains(Before[1].CharacterId));
        FRunAccountId Stranger = Buyer.OwnerAccountId;
        Stranger.Subject += TEXT("-other");
        TestFalse(TEXT("A different account cannot purchase for this character"), Fixture.Run->PurchaseShopOffer(Stranger, Buyer.CharacterId, OfferId, Fixture.Error, Revision));
        TestFalse(TEXT("The owner cannot purchase for an AI companion"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Before[1].CharacterId, OfferId, Fixture.Error, Revision));
        TestFalse(TEXT("A stale revision cannot execute a service"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Revision - 1));
        TestFalse(TEXT("A content tag from another service cannot replace the selected offer"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, TEXT("Encounter.Service.Unknown"), Fixture.Error, Revision));
        int32 Publications = 0;
        Fixture.Run->OnRunStateChanged.AddLambda([&Publications]() { ++Publications; });
        FRunCheckpointStorage::FailNextWriteForTesting();
        TestFalse(TEXT("The service rejects a failed durable write"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Revision));
        TestTrue(TEXT("Failed writes retain all HP, balances, stock, bytes and revision"), SameRecoveryParty(Before, Fixture.Run->GetPartyMembers()) && OriginalBytes == Fixture.Bytes() && Fixture.Run->GetRecoveryState().Revision == Revision && Publications == 0);
        if (!TestTrue(TEXT("Retry commits the service atomically"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Revision))) return false;
        const FRunPartyMember After = Fixture.Run->GetPartyMembers()[0];
        FProfessionDefinition Profession;
        if (!Fixture.Run->ResolveMemberProfession(After, Profession, Fixture.Error)) return false;
        TestEqual(TEXT("Exactly one trial price is charged"), After.Gold, Buyer.Gold - 1);
        TestEqual(TEXT("The service changes only its intended stock quantity"), After.Consumables[0].Quantity, bConsumable ? 2 : 1);
        TestEqual(TEXT("Recovery and revival use their frozen trial values"), After.CurrentHP, bConsumable ? 30.f : bRevival ? Profession.MaxHP * .25f : 55.f);
        const TArray<uint8> Committed = Fixture.Bytes();
        TestFalse(TEXT("A duplicated accepted revision never charges or applies twice"), Fixture.Run->PurchaseShopOffer(Buyer.OwnerAccountId, Buyer.CharacterId, OfferId, Fixture.Error, Revision));
        TestTrue(TEXT("Duplicate rejection preserves durable bytes and emits no extra publication"), Committed == Fixture.Bytes() && Publications == 1);
        Fixture.Run->OnRunStateChanged.Clear();
        if (!TestTrue(TEXT("Continue restores the service visit"), Fixture.Run->LoadStandaloneCheckpoint(Fixture.Error))) return false;
        TestTrue(TEXT("Resume preserves healed or revived HP, gold and consumable quantity"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&After, &Fixture.Run->GetPartyMembers()[0], 0));
        for (int32 Index = 1; Index < Before.Num(); ++Index) TestTrue(TEXT("Every AI companion remains unchanged"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Before[Index], &Fixture.Run->GetPartyMembers()[Index], 0));
    }
    FRecoveryFixture Full;
    if (!Full.Initialize() || !Full.SelectService(FRunEncounterOffer::GetRecoveryTag())) return false;
    const FRunPartyMember FullBuyer = Full.Run->GetPartyMembers()[0];
    TestFalse(TEXT("Full HP rejects paid recovery"), Full.Run->PurchaseShopOffer(FullBuyer.OwnerAccountId, FullBuyer.CharacterId, FRunEncounterOffer::GetRecoveryTag().GetTagName(), Full.Error, Full.Run->GetRecoveryState().Revision));
    FRecoveryFixture Poor;
    if (!Poor.Initialize() || !Poor.SetFixtureHealth(30.f, true) || !Poor.SelectService(FRunEncounterOffer::GetConsumableShopTag())) return false;
    const FRunPartyMember PoorBuyer = Poor.Run->GetPartyMembers()[0];
    TestFalse(TEXT("Insufficient gold cannot buy a consumable"), Poor.Run->PurchaseShopOffer(PoorBuyer.OwnerAccountId, PoorBuyer.CharacterId, FRunEncounterOffer::GetConsumableShopTag().GetTagName(), Poor.Error, Poor.Run->GetRecoveryState().Revision));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunConsumableCheckpointTest, "ProjectA.Run.Recovery.ReadyBoundaryStockPlanSerializationAndValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunConsumableCheckpointTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    FRunConsumableStack Stack;
    Stack.ItemTag = RunRecoveryRules::GetHealingItemTag();
    Stack.Skill = RunRecoveryRules::GetHealingSkillPath();
    Stack.Quantity = 1;
    FCombatRoundSkill Skill;
    FText Error;
    if (!RunRecoveryRules::ResolveStack(Stack, Skill, Error)) return false;
    FCombatCheckpointData& Checkpoint = Save->CombatCheckpoint;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FCombatCheckpointUnit& Unit = Checkpoint.Units.AddDefaulted_GetRef();
        Unit.RoundUnitId = Index + 1;
        Unit.Team = Index == 0 ? ETeam::Player : ETeam::Enemy;
        Unit.HP = Index == 0 ? 40.f : 100.f;
        Unit.MaxHP = 100.f;
        Unit.AP = 2;
        Unit.GridCoord = FIntPoint(0, Index * 3);
        if (Index == 0) Unit.Consumables = {Stack};
        FCombatCheckpointRoundPlan& Plan = Checkpoint.RoundPlans.AddDefaulted_GetRef();
        Plan.UnitId = Unit.RoundUnitId;
        Plan.bReady = true;
        Plan.Command.UnitId = Unit.RoundUnitId;
        Plan.Command.TargetCoord = Plan.Command.DestinationCoord = Unit.GridCoord;
        if (Index == 0)
        {
            Plan.Command.SkillId = Skill.SkillId;
            Plan.Command.TargetUnitId = Unit.RoundUnitId;
        }
    }
    if (!TestTrue(TEXT("A Ready consumable command validates separately from the five acquired skills"), CombatPlanValidation::ValidateCheckpointPlans(Checkpoint, Error))) return false;
    TArray<uint8> Bytes;
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes)) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The standard SaveGame serializer retains the Ready state"), Restored.Get())) return false;
    TestTrue(TEXT("Resume keeps exactly the pre-release HP, AP, stock, tag, stable DA path and request"), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Checkpoint, &Restored->CombatCheckpoint, 0) && CombatPlanValidation::ValidateCheckpointPlans(Restored->CombatCheckpoint, Error));
    const auto Rejected = [this, &Checkpoint, &Error](TFunctionRef<void(FCombatCheckpointData&)> Mutate, const TCHAR* Reason)
    {
        FCombatCheckpointData Invalid = Checkpoint;
        Mutate(Invalid);
        TestFalse(Reason, CombatPlanValidation::ValidateCheckpointPlans(Invalid, Error));
    };
    Rejected([](FCombatCheckpointData& Value) { Value.Units[0].Consumables[0].Quantity = 0; }, TEXT("An empty saved stack cannot fund a prepared action"));
    Rejected([](FCombatCheckpointData& Value) { Value.Units[0].Consumables[0].ItemTag = FGameplayTag(); }, TEXT("Missing content classification cannot bypass item validation"));
    Rejected([](FCombatCheckpointData& Value) { Value.Units[0].Consumables[0].Skill = FSoftObjectPath(TEXT("/Engine/Transient.HealthPotion")); }, TEXT("A transient asset cannot become a persistent action identity"));
    Rejected([](FCombatCheckpointData& Value) { Value.Units[0].PartyControlMode = EPartyControlMode::ServerAI; }, TEXT("An AI cannot author a saved consumable request"));
    Rejected([](FCombatCheckpointData& Value) { Value.Units[0].HP = 100.f; }, TEXT("Saved full HP rejects redundant recovery"));
    Rejected([](FCombatCheckpointData& Value) { Value.RoundPlans[0].Command.TargetUnitId = 2; }, TEXT("Saved consumables remain self-targeted"));
    return true;
}

#endif
