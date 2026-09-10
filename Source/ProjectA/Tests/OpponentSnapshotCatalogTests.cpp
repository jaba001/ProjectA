#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "GAS/Ability/GA_AreaAttack.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "Unit/EnemyUnit.h"

namespace
{
    struct FSnapshotCatalogFixture
    {
        UOpponentSnapshotCatalogDataAsset* Catalog = NewObject<UOpponentSnapshotCatalogDataAsset>();
        USkillDefinitionDataAsset* BasicAttack = NewObject<USkillDefinitionDataAsset>(Catalog);
        USkillDefinitionDataAsset* AreaAttack = NewObject<USkillDefinitionDataAsset>(Catalog);
        FPartySnapshot Snapshot;

        FSnapshotCatalogFixture()
        {
            Catalog->EnemyClasses.Add(TEXT("Hunter"), AEnemyUnit::StaticClass());
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
            Member.MemberId = TEXT("Hunter_01");
            Member.ClassId = TEXT("Hunter");
            Member.CharacterName = TEXT("Snapshot Hunter");
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
    TestFalse(TEXT("Two identifiers cannot grant the same ability twice"), Fixture.Catalog->ValidateForEncounter(Candidate, 4, Error));

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
    Member.SkillIds = { Fixture.BasicAttack->SkillId, TEXT("MissingAfterValidSkill") };
    TestFalse(TEXT("A later unresolved identifier rejects the whole skill list"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Partial resolution preserves the caller's previous loadout"), Resolved == Previous);
    TestFalse(TEXT("Failed skill resolution explains the rejection"), Error.IsEmpty());

    Member.SkillIds.Reset();
    TestFalse(TEXT("An empty skill list cannot execute"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Empty input preserves the caller's previous loadout"), Resolved == Previous);
    Member.SkillIds.Init(Fixture.BasicAttack->SkillId, 6);
    TestFalse(TEXT("More than five skill slots cannot execute"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Excess skill slots preserve the caller's previous loadout"), Resolved == Previous);

    Member.SkillIds = { Fixture.BasicAttack->SkillId, Fixture.BasicAttack->SkillId };
    TestFalse(TEXT("Duplicate ability resolution cannot collapse two slots into one"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Duplicate ability rejection preserves the caller's previous loadout"), Resolved == Previous);
    Fixture.AreaAttack->ActionPointCost = 0;
    Member.SkillIds = { Fixture.BasicAttack->SkillId, Fixture.AreaAttack->SkillId };
    TestFalse(TEXT("Invalid trusted skill content still cannot execute"), Fixture.Catalog->ResolveSkills(Member, Resolved, Error));
    TestTrue(TEXT("Invalid trusted content preserves the caller's previous loadout"), Resolved == Previous);
    return true;
}

#endif
