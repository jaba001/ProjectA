#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Game/Snapshot/PartySnapshotSelectionLibrary.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Game/Run/RunRecoveryTypes.h"
#include "GAS/Ability/GA_AreaAttack.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "Unit/EnemyUnit.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SnapshotSelectionAllowed, "Validation.Snapshot.Allowed");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SnapshotSelectionExcluded, "Validation.Snapshot.Excluded");

    struct FSnapshotCatalogFixture
    {
        UOpponentSnapshotCatalogDataAsset* Catalog = NewObject<UOpponentSnapshotCatalogDataAsset>();
        USkillDefinitionDataAsset* BasicAttack = NewObject<USkillDefinitionDataAsset>(Catalog);
        USkillDefinitionDataAsset* AreaAttack = NewObject<USkillDefinitionDataAsset>(Catalog);
        FPartySnapshot Snapshot;

        FSnapshotCatalogFixture()
        {
            Catalog->EnemyClasses.Add(TEXT("Archer"), AEnemyUnit::StaticClass());
            BasicAttack->SkillId = TEXT("BasicAttack");
            BasicAttack->AbilityClass = UGA_DefaultAttack::StaticClass();
            AreaAttack->SkillId = TEXT("AreaAttack");
            AreaAttack->AbilityClass = UGA_AreaAttack::StaticClass();
            AreaAttack->AreaType = ESkillAreaType::AroundTarget;
            AreaAttack->AreaRadius = 1;
            Catalog->Skills.Add(BasicAttack->SkillId, BasicAttack);
            Catalog->Skills.Add(AreaAttack->SkillId, AreaAttack);
            Snapshot.SnapshotId = TEXT("CatalogTest");
            FPartySnapshotMember& Member = Snapshot.Members.AddDefaulted_GetRef();
            Member.MemberId = TEXT("Archer_01");
            Member.ClassId = TEXT("Archer");
            Member.CharacterName = TEXT("Snapshot Archer");
            Member.FormationSlot = 2;
            Member.SkillIds = { BasicAttack->SkillId, AreaAttack->SkillId };
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpponentSnapshotCatalogValidationTest, "ProjectA.Snapshot.CatalogValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpponentSnapshotCatalogValidationTest::RunTest(const FString& Parameters)
{
    FSnapshotCatalogFixture Fixture;
    FText Error = FText::FromString(TEXT("Previous error"));
    TestTrue(TEXT("Trusted class and skill identifiers allow encounter execution"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 4, Error));
    TestTrue(TEXT("Successful validation clears an earlier error"), Error.IsEmpty());

    FPartySnapshot Candidate = Fixture.Snapshot;
    Candidate.Members[0].ClassId = TEXT("UnknownClass");
    TestFalse(TEXT("Unknown class cannot select a fallback enemy"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    TestFalse(TEXT("Unknown class explains the rejection"), Error.IsEmpty());
    Candidate = Fixture.Snapshot;
    Candidate.Members[0].SkillIds[1] = TEXT("UnknownSkill");
    TestFalse(TEXT("Unknown skill cannot silently change the opponent build"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));

    USkillDefinitionDataAsset* Alias = NewObject<USkillDefinitionDataAsset>(Fixture.Catalog);
    Alias->SkillId = TEXT("BasicAttackAlias");
    Alias->AbilityClass = UGA_DefaultAttack::StaticClass();
    Fixture.Catalog->Skills.Add(Alias->SkillId, Alias);
    Candidate = Fixture.Snapshot;
    Candidate.Members[0].SkillIds[1] = Alias->SkillId;
    TestTrue(TEXT("Distinct skill identifiers remain structurally valid storage data"), UPartySnapshotLibrary::ValidateSnapshot(Candidate, Error));
    TestTrue(TEXT("Distinct authored skills may share a retired ability class"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    Fixture.Catalog->Skills.Add(TEXT("SameAssetAlias"), Fixture.BasicAttack);
    Candidate.Members[0].SkillIds[1] = TEXT("SameAssetAlias");
    TestTrue(TEXT("Catalog aliases remain structurally distinct snapshot identifiers"), UPartySnapshotLibrary::ValidateSnapshot(Candidate, Error));
    TestFalse(TEXT("Two catalog identifiers cannot duplicate one resolved skill asset"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));

    Candidate = Fixture.Snapshot;
    ++Candidate.SchemaVersion;
    TestFalse(TEXT("Unsupported snapshot schema cannot execute"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    Candidate = Fixture.Snapshot;
    ++Candidate.ContentVersion;
    TestTrue(TEXT("Storage can retain another positive content version"), UPartySnapshotLibrary::ValidateSnapshot(Candidate, Error));
    TestFalse(TEXT("Mismatched content version cannot execute with this catalog"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    Fixture.Catalog->ContentVersion = 0;
    TestFalse(TEXT("An invalid catalog version cannot execute"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 4, Error));
    Fixture.Catalog->ContentVersion = Fixture.Snapshot.ContentVersion;

    Candidate = Fixture.Snapshot;
    Candidate.Members[0].EquipmentIds.Add(TEXT("Sword"));
    TestTrue(TEXT("Storage preserves future equipment identifiers"), UPartySnapshotLibrary::ValidateSnapshot(Candidate, Error));
    TestFalse(TEXT("Unsupported equipment cannot be silently ignored during execution"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    Candidate = Fixture.Snapshot;
    Candidate.Members[0].TacticsId = TEXT("Defensive");
    TestTrue(TEXT("Storage preserves a future tactics identifier"), UPartySnapshotLibrary::ValidateSnapshot(Candidate, Error));
    TestFalse(TEXT("Unsupported tactics cannot be silently ignored during execution"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));

    Candidate = Fixture.Snapshot;
    Candidate.Members[0].Stats.CurrentHP = 0.0f;
    TestTrue(TEXT("Storage can preserve a defeated party member"), UPartySnapshotLibrary::ValidateSnapshot(Candidate, Error));
    TestFalse(TEXT("Zero HP cannot spawn a living opponent"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    Candidate = Fixture.Snapshot;
    Candidate.Members[0].FormationSlot = -1;
    TestFalse(TEXT("Negative formation cannot execute"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    TestFalse(TEXT("A formation index equal to arena slot count cannot execute"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 2, Error));
    TestFalse(TEXT("An arena with no slots cannot execute"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 0, Error));
    Candidate = Fixture.Snapshot;
    Candidate.Members[0].FormationSlot = 3;
    TestTrue(TEXT("The final supported formation slot can execute"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));
    Candidate.Members[0].FormationSlot = 4;
    TestFalse(TEXT("Storage formation bounds remain enforced even with a larger arena"), Fixture.Catalog->ValidateForEncounter(Candidate, 8, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpponentSnapshotSkillResolutionTest, "ProjectA.Snapshot.SkillResolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpponentSnapshotSkillResolutionTest::RunTest(const FString& Parameters)
{
    FSnapshotCatalogFixture Fixture;
    FPartySnapshotMember Member = Fixture.Snapshot.Members[0];
    Member.SkillIds = { Fixture.AreaAttack->SkillId, Fixture.BasicAttack->SkillId };
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Resolved;
    FText Error = FText::FromString(TEXT("Previous error"));
    if (!TestTrue(TEXT("Ordered skill identifiers resolve through the trusted catalog"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error)))
    {
        return false;
    }
    TestTrue(TEXT("Successful skill resolution clears an earlier error"), Error.IsEmpty());
    if (!TestEqual(TEXT("Every requested skill is resolved"), Resolved.Num(), 2))
    {
        return false;
    }
    TestEqual(TEXT("The first identifier retains the default attack position"), Resolved[0].Get(), Fixture.AreaAttack);
    TestEqual(TEXT("The second identifier retains its slot position"), Resolved[1].Get(), Fixture.BasicAttack);

    const TArray<TObjectPtr<USkillDefinitionDataAsset>> Previous = Resolved;
    for (FName DeletedId : {FName(TEXT("SweepingStrike")), FName(TEXT("AOE")), FName(TEXT("RangedAttack"))})
    {
        Fixture.Catalog->Skills.Add(DeletedId, Fixture.AreaAttack);
        Member.SkillIds = {Fixture.BasicAttack->SkillId, DeletedId};
        TestFalse(TEXT("Each deleted identifier cannot resolve even when a catalog still maps it to a valid skill"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
        TestTrue(TEXT("Each deleted identifier preserves the caller's previous loadout"), Resolved == Previous);
        TestFalse(TEXT("Each deleted identifier explains rejection"), Error.IsEmpty());
    }
    Member.SkillIds = { Fixture.BasicAttack->SkillId, TEXT("MissingAfterValidSkill") };
    TestFalse(TEXT("A later unresolved identifier rejects the whole skill list"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Partial resolution preserves the caller's previous loadout"), Resolved == Previous);
    TestFalse(TEXT("Failed skill resolution explains the rejection"), Error.IsEmpty());

    Member.SkillIds.Reset();
    TestFalse(TEXT("An empty skill list cannot execute"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Empty input preserves the caller's previous loadout"), Resolved == Previous);
    Member.SkillIds.Init(Fixture.BasicAttack->SkillId, 6);
    TestFalse(TEXT("Six duplicate identifiers cannot execute"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Duplicate skill slots preserve the caller's previous loadout"), Resolved == Previous);

    Member.SkillIds = { Fixture.BasicAttack->SkillId, Fixture.BasicAttack->SkillId };
    TestFalse(TEXT("Duplicate skill asset resolution cannot collapse two slots into one"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Duplicate skill asset rejection preserves the caller's previous loadout"), Resolved == Previous);
    Fixture.AreaAttack->ActionPointCost = 0;
    Member.SkillIds = { Fixture.BasicAttack->SkillId, Fixture.AreaAttack->SkillId };
    TestFalse(TEXT("Invalid trusted skill content still cannot execute"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Invalid trusted content preserves the caller's previous loadout"), Resolved == Previous);
    FCombatRoundSkill InvalidDefinition;
    FText AssetError;
    Fixture.AreaAttack->ResolveRoundSkill(InvalidDefinition, AssetError);
    TestEqual(TEXT("Catalog propagates the exact round definition error"), Error.ToString(), AssetError.ToString());
    Fixture.AreaAttack->bUseRoundDefinition = true;
    Fixture.AreaAttack->AbilityClass = nullptr;
    Fixture.AreaAttack->RoundDefinition.Kind = ECombatRoundSkillKind::Wait;
    Fixture.AreaAttack->RoundDefinition.Approach = ECombatRoundApproach::None;
    Fixture.AreaAttack->RoundDefinition.ActionPointCost = 0;
    Fixture.AreaAttack->RoundDefinition.Power = 0.0f;
    TestTrue(TEXT("Explicit round profile resolves without an executable ability class"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestEqual(TEXT("Explicit profile preserves its ordered data asset"), Resolved[1].Get(), Fixture.AreaAttack);
    TestTrue(TEXT("Snapshot encounter accepts a trusted null-GA round profile"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 4, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySnapshotCandidateSelectionTest, "ProjectA.Snapshot.CandidateSelection.InMemory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartySnapshotCandidateSelectionTest::RunTest(const FString& Parameters)
{
    FSnapshotCatalogFixture Fixture;
    FPartySnapshotCandidate Valid;
    Valid.Snapshot = Fixture.Snapshot;
    Valid.Snapshot.Members[0].Stats.CurrentHP = 37.0f;
    Valid.ProgressStage = 7;
    Valid.Tags.AddTag(TAG_SnapshotSelectionAllowed);
    TArray<FPartySnapshotCandidate> Candidates;
    FPartySnapshotCandidate Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("WrongVersion");
    ++Rejected.Snapshot.ContentVersion;
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("WrongStage");
    ++Rejected.ProgressStage;
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("WrongTags");
    Rejected.Tags.Reset();
    Rejected.Tags.AddTag(TAG_SnapshotSelectionExcluded);
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("DeadOpponent");
    Rejected.Snapshot.Members[0].Stats.CurrentHP = 0.0f;
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("UntrustedClass");
    Rejected.Snapshot.Members[0].ClassId = TEXT("/Game/Untrusted/BP_Opponent.BP_Opponent_C");
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("UnknownSkill");
    Rejected.Snapshot.Members[0].SkillIds = {TEXT("UnknownSkill")};
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("UnsupportedEquipment");
    Rejected.Snapshot.Members[0].EquipmentIds.Add(TEXT("Sword"));
    Candidates.Add(Rejected);
    Rejected = Valid;
    Rejected.Snapshot.SnapshotId = TEXT("UnsupportedSchema");
    ++Rejected.Snapshot.SchemaVersion;
    Candidates.Add(Rejected);

    const FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchTag(TAG_SnapshotSelectionAllowed);
    FRandomStream Random(731);
    FPartySnapshot Selected = Fixture.Snapshot;
    Selected.SnapshotId = TEXT("PreviousSelection");
    const FPartySnapshot Previous = Selected;
    FText Error;
    TestFalse(TEXT("A pool without a compatible living trusted opponent is rejected"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, 7, Query, 4, Random, Selected, Error));
    TestTrue(TEXT("Rejected pools preserve the previous selection"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &Previous, 0));
    TestEqual(TEXT("Rejected pools do not consume the seeded draw"), Random.GetCurrentSeed(), 731);
    TestFalse(TEXT("Rejected pools explain why no selection was made"), Error.IsEmpty());

    Candidates.Add(Valid);
    TestTrue(TEXT("The matching living candidate is selected through the trusted catalog"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, 7, Query, 4, Random, Selected, Error));
    TestTrue(TEXT("Selection preserves the complete submitted stats, ordered skills and formation"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &Valid.Snapshot, 0));
    TestTrue(TEXT("Successful selection clears prior failure text"), Error.IsEmpty());
    Selected.Members[0].Stats.CurrentHP = 1.0f;
    TestEqual(TEXT("Changing the selected value cannot consume the candidate owner's stored HP"), Candidates.Last().Snapshot.Members[0].Stats.CurrentHP, 37.0f);

    Candidates.Add(Valid);
    const FPartySnapshot BeforeDuplicate = Selected;
    const int32 BeforeDuplicateSeed = Random.GetCurrentSeed();
    TestFalse(TEXT("Duplicate eligible snapshot IDs cannot bias the uniform draw"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, 7, Query, 4, Random, Selected, Error));
    TestTrue(TEXT("Duplicate rejection preserves the previous selection"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &BeforeDuplicate, 0));
    TestEqual(TEXT("Duplicate rejection preserves the random stream"), Random.GetCurrentSeed(), BeforeDuplicateSeed);
    TestFalse(TEXT("Missing trusted catalogs cannot select an opponent"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, nullptr, 7, Query, 4, Random, Selected, Error));
    TestFalse(TEXT("Unspecified progress stages cannot select an opponent"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, INDEX_NONE, Query, 4, Random, Selected, Error));
    TestFalse(TEXT("Unsupported formation slot counts cannot select an opponent"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, 7, Query, 5, Random, Selected, Error));

    Candidates = {Valid, Valid};
    Candidates[1].Snapshot.SnapshotId = TEXT("SecondEligible");
    FRandomStream SelectionRandom(961);
    FRandomStream ReplayRandom(961);
    FPartySnapshot Replayed;
    if (!TestTrue(TEXT("Multiple valid opponents can be selected with a saved seed"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, 7, Query, 4, SelectionRandom, Selected, Error))) return false;
    if (!TestTrue(TEXT("The same pool and seed can reproduce a local selection"), UPartySnapshotSelectionLibrary::SelectOpponent(Candidates, Fixture.Catalog, 7, Query, 4, ReplayRandom, Replayed, Error))) return false;
    TestTrue(TEXT("Selection resolves back to an eligible submitted snapshot"), Candidates.ContainsByPredicate([&Selected](const FPartySnapshotCandidate& Candidate) { return FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &Candidate.Snapshot, 0); }));
    TestTrue(TEXT("Replaying the draw preserves the complete selected opponent"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &Replayed, 0));
    TestEqual(TEXT("Replayed selection advances the random stream identically"), SelectionRandom.GetCurrentSeed(), ReplayRandom.GetCurrentSeed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySnapshotConsumableBoundaryTest, "ProjectA.Snapshot.ConsumableBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartySnapshotConsumableBoundaryTest::RunTest(const FString& Parameters)
{
    FSnapshotCatalogFixture Fixture;
    FText Error;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Resolved;
    if (!TestTrue(TEXT("Ordinary trusted skills resolve before the boundary check"), Fixture.Catalog->ResolveSkills(Fixture.Snapshot.Members[0], Resolved, Error))) return false;
    const TArray<TObjectPtr<USkillDefinitionDataAsset>> Previous = Resolved;
    USkillDefinitionDataAsset* Consumable = NewObject<USkillDefinitionDataAsset>(Fixture.Catalog);
    Consumable->SkillId = TEXT("ConsumableAlias");
    Consumable->bUseRoundDefinition = true;
    Consumable->RoundDefinition.Kind = ECombatRoundSkillKind::Wait;
    Consumable->RoundDefinition.Approach = ECombatRoundApproach::None;
    Consumable->RoundDefinition.ActionPointCost = 0;
    Consumable->RoundDefinition.Power = 0.f;
    Consumable->RoundDefinition.EffectTags.AddTag(RunRecoveryRules::GetHealingItemTag());
    FCombatRoundSkill ConsumableProfile;
    if (!TestTrue(TEXT("The fixture has a valid round profile carrying the consumable child tag"), Consumable->ResolveRoundSkill(ConsumableProfile, Error))) return false;
    Fixture.Catalog->Skills.Add(Consumable->SkillId, Consumable);
    Fixture.Snapshot.Members[0].SkillIds.Add(Consumable->SkillId);
    TestTrue(TEXT("Opaque snapshot storage does not resolve catalog skill semantics"), UPartySnapshotLibrary::ValidateSnapshot(Fixture.Snapshot, Error));
    TestFalse(TEXT("Consumable tags cannot enter an opponent's ordinary skill slots"), Fixture.Catalog->ResolveSkills(Fixture.Snapshot.Members[0], Resolved, Error));
    TestTrue(TEXT("A late consumable rejection preserves the complete previous resolved list"), Resolved == Previous);
    TestFalse(TEXT("Encounter validation rejects quantity-free consumable execution"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 4, Error));
    FPartySnapshotCandidate Candidate;
    Candidate.Snapshot = Fixture.Snapshot;
    Candidate.ProgressStage = 3;
    FPartySnapshot Selected;
    Selected.SnapshotId = TEXT("PreservedBeforeConsumable");
    const FPartySnapshot PreviousSelection = Selected;
    FRandomStream Random(213);
    TestFalse(TEXT("The candidate selector also rejects consumable-bearing opponents"), UPartySnapshotSelectionLibrary::SelectOpponent({Candidate}, Fixture.Catalog, 3, FGameplayTagQuery(), 4, Random, Selected, Error));
    TestTrue(TEXT("Consumable rejection preserves the prior selection"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &PreviousSelection, 0));
    TestEqual(TEXT("Consumable rejection preserves the draw seed"), Random.GetCurrentSeed(), 213);

    Fixture.Snapshot.Members[0].SkillIds.Reset();
    for (int32 Index = 0; Index < 6; ++Index)
    {
        USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Fixture.Catalog);
        Skill->SkillId = FName(*FString::Printf(TEXT("TrustedOrderedSkill_%d"), Index));
        Skill->AbilityClass = UGA_DefaultAttack::StaticClass();
        Fixture.Catalog->Skills.Add(Skill->SkillId, Skill);
        Fixture.Snapshot.Members[0].SkillIds.Add(Skill->SkillId);
    }
    if (!TestTrue(TEXT("Six distinct trusted nonconsumable skills retain the current unlimited loadout contract"), Fixture.Catalog->ResolveSkills(Fixture.Snapshot.Members[0], Resolved, Error))) return false;
    TestEqual(TEXT("No former five-skill limit truncates the opponent loadout"), Resolved.Num(), 6);
    for (int32 Index = 0; Index < Resolved.Num(); ++Index) TestEqual(TEXT("Every skill retains its submitted order"), Resolved[Index]->SkillId, Fixture.Snapshot.Members[0].SkillIds[Index]);
    TestTrue(TEXT("The extended ordinary loadout remains encounter-compatible"), Fixture.Catalog->ValidateForEncounter(Fixture.Snapshot, 4, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySnapshotLocalSlotSelectionTest, "ProjectA.Snapshot.CandidateSelection.LocalSlots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPartySnapshotLocalSlotSelectionTest::RunTest(const FString& Parameters)
{
    struct FSnapshotSelectionSlot
    {
        const FName Id = FName(*(TEXT("T14_Selection_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));

        ~FSnapshotSelectionSlot()
        {
            UGameplayStatics::DeleteGameInSlot(UPartySnapshotLibrary::GetSaveSlotName(Id), 0);
        }
    };

    FSnapshotCatalogFixture Fixture;
    TStrongObjectPtr<UOpponentSnapshotCatalogDataAsset> KeepCatalog(Fixture.Catalog);
    FSnapshotSelectionSlot FirstSlot;
    FSnapshotSelectionSlot SecondSlot;
    FSnapshotSelectionSlot MissingSlot;
    FSnapshotSelectionSlot InvalidSlot;
    FText Error;
    FPartySnapshot First = Fixture.Snapshot;
    First.SnapshotId = TEXT("StoredFirstOpponent");
    First.Members[0].Stats.CurrentHP = 37.f;
    FPartySnapshot Second = First;
    Second.SnapshotId = TEXT("StoredSecondOpponent");
    Second.Members[0].Stats.CurrentHP = 63.f;
    if (!TestTrue(TEXT("The first disposable opponent is saved"), UPartySnapshotLibrary::SaveSnapshot(FirstSlot.Id, First, Error))) return false;
    if (!TestTrue(TEXT("The second disposable opponent is saved"), UPartySnapshotLibrary::SaveSnapshot(SecondSlot.Id, Second, Error))) return false;
    TArray<uint8> FirstBytes;
    TArray<uint8> SecondBytes;
    if (!TestTrue(TEXT("The first original save bytes are captured"), UGameplayStatics::LoadDataFromSlot(FirstBytes, UPartySnapshotLibrary::GetSaveSlotName(FirstSlot.Id), 0))) return false;
    if (!TestTrue(TEXT("The second original save bytes are captured"), UGameplayStatics::LoadDataFromSlot(SecondBytes, UPartySnapshotLibrary::GetSaveSlotName(SecondSlot.Id), 0))) return false;
    FPartySnapshotSlotCandidate FirstEntry;
    FirstEntry.SlotId = FirstSlot.Id;
    FirstEntry.ProgressStage = 7;
    FirstEntry.Tags.AddTag(TAG_SnapshotSelectionAllowed);
    FPartySnapshotSlotCandidate SecondEntry = FirstEntry;
    SecondEntry.SlotId = SecondSlot.Id;
    const FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchTag(TAG_SnapshotSelectionAllowed);
    FRandomStream Random(819);
    FRandomStream ReplayRandom(819);
    FPartySnapshot Selected;
    FPartySnapshot Replayed;
    if (!TestTrue(TEXT("Matching local saved slots feed the common candidate selector"), UPartySnapshotSelectionLibrary::LoadAndSelectOpponent({FirstEntry, SecondEntry}, Fixture.Catalog, 7, Query, 4, Random, Selected, Error))) return false;
    if (!TestTrue(TEXT("The same saved slots and seed replay the same selection"), UPartySnapshotSelectionLibrary::LoadAndSelectOpponent({FirstEntry, SecondEntry}, Fixture.Catalog, 7, Query, 4, ReplayRandom, Replayed, Error))) return false;
    TestTrue(TEXT("Local slot selection preserves the full saved value"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &First, 0) || FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &Second, 0));
    TestTrue(TEXT("Local replay preserves the complete selected build"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &Replayed, 0));
    TestEqual(TEXT("Local replay advances the stream identically"), Random.GetCurrentSeed(), ReplayRandom.GetCurrentSeed());
    TestTrue(TEXT("Successful local selection clears the error"), Error.IsEmpty());
    Selected.Members[0].Stats.CurrentHP = 1.f;
    const FPartySnapshot BeforeFailure = Selected;
    const int32 SeedBeforeFailure = Random.GetCurrentSeed();
    const auto Reject = [this, &Fixture, &Query, &Random, &Selected, &Error, &BeforeFailure, SeedBeforeFailure](const TArray<FPartySnapshotSlotCandidate>& Entries, const TCHAR* Label)
    {
        TestFalse(Label, UPartySnapshotSelectionLibrary::LoadAndSelectOpponent(Entries, Fixture.Catalog, 7, Query, 4, Random, Selected, Error));
        TestTrue(TEXT("Rejected local requests preserve the previous opponent"), FPartySnapshot::StaticStruct()->CompareScriptStruct(&Selected, &BeforeFailure, 0));
        TestEqual(TEXT("Rejected local requests preserve the exact next draw"), Random.GetCurrentSeed(), SeedBeforeFailure);
        TestFalse(TEXT("Rejected local requests explain the failure"), Error.IsEmpty());
    };
    Reject({}, TEXT("An empty local pool cannot select a fallback opponent"));
    Reject({FirstEntry, FirstEntry}, TEXT("One save slot cannot be submitted twice to bias a draw"));
    FPartySnapshotSlotCandidate RejectedEntry = FirstEntry;
    RejectedEntry.SlotId = TEXT("../InvalidSlot");
    Reject({FirstEntry, RejectedEntry}, TEXT("Invalid slot syntax fails before reading the local pool"));
    RejectedEntry = FirstEntry;
    RejectedEntry.ProgressStage = INDEX_NONE;
    Reject({RejectedEntry}, TEXT("Unspecified candidate progress cannot enter the local pool"));
    RejectedEntry = FirstEntry;
    RejectedEntry.SlotId = MissingSlot.Id;
    Reject({FirstEntry, RejectedEntry}, TEXT("A later missing matching slot fails instead of drawing from a reduced pool"));
    TStrongObjectPtr<UPartySnapshotSaveGame> InvalidSave(NewObject<UPartySnapshotSaveGame>());
    InvalidSave->Snapshot = First;
    InvalidSave->Snapshot.SchemaVersion = 99;
    if (!TestTrue(TEXT("The disposable malformed snapshot fixture is saved"), UGameplayStatics::SaveGameToSlot(InvalidSave.Get(), UPartySnapshotLibrary::GetSaveSlotName(InvalidSlot.Id), 0))) return false;
    RejectedEntry.SlotId = InvalidSlot.Id;
    Reject({FirstEntry, RejectedEntry}, TEXT("A later malformed matching slot preserves the previous selection"));
    if (!TestTrue(TEXT("The disposable alternate slot can represent a duplicate stored snapshot ID"), UPartySnapshotLibrary::SaveSnapshot(InvalidSlot.Id, First, Error))) return false;
    Reject({FirstEntry, RejectedEntry}, TEXT("Distinct slot files cannot duplicate one eligible snapshot ID"));

    RejectedEntry.SlotId = MissingSlot.Id;
    RejectedEntry.ProgressStage = 8;
    if (!TestTrue(TEXT("Metadata from another progress stage is filtered before local file loading"), UPartySnapshotSelectionLibrary::LoadAndSelectOpponent({FirstEntry, RejectedEntry}, Fixture.Catalog, 7, Query, 4, Random, Selected, Error))) return false;
    RejectedEntry.ProgressStage = 7;
    RejectedEntry.Tags.Reset();
    RejectedEntry.Tags.AddTag(TAG_SnapshotSelectionExcluded);
    if (!TestTrue(TEXT("GameplayTagQuery excludes an unrelated missing slot before file loading"), UPartySnapshotSelectionLibrary::LoadAndSelectOpponent({FirstEntry, RejectedEntry}, Fixture.Catalog, 7, Query, 4, Random, Selected, Error))) return false;
    TArray<uint8> AfterBytes;
    TestTrue(TEXT("The first source slot remains readable"), UGameplayStatics::LoadDataFromSlot(AfterBytes, UPartySnapshotLibrary::GetSaveSlotName(FirstSlot.Id), 0));
    TestTrue(TEXT("Selection and caller mutations preserve first source bytes"), AfterBytes == FirstBytes);
    TestTrue(TEXT("The second source slot remains readable"), UGameplayStatics::LoadDataFromSlot(AfterBytes, UPartySnapshotLibrary::GetSaveSlotName(SecondSlot.Id), 0));
    TestTrue(TEXT("Selection and failures preserve second source bytes"), AfterBytes == SecondBytes);
    return true;
}

#endif
