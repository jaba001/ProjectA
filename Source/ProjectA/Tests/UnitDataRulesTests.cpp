#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Game/Run/RunTypes.h"
#include "GAS/Effect/GE_Damage.h"
#include "Unit/PlayerUnit.h"
#include "Unit/UnitDataRules.h"
#include <limits>

namespace UnitDataRulesTests
{
    struct FScopedWorld
    {
        UWorld* World = nullptr;

        FScopedWorld()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        }

        ~FScopedWorld()
        {
            if (World) World->DestroyWorld(false);
        }

        APlayerUnit* SpawnUnit()
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            APlayerUnit* Unit = World->SpawnActor<APlayerUnit>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            return Unit;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitDataValidationAgreementTest, "ProjectA.Party.SharedDataValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnitDataValidationAgreementTest::RunTest(const FString& Parameters)
{
    UnitDataRulesTests::FScopedWorld Scope;
    APlayerUnit* Unit = Scope.SpawnUnit();
    UPartyDefinitionDataAsset* Catalog = NewObject<UPartyDefinitionDataAsset>();
    USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Catalog);
    Skill->bUseRoundDefinition = true;
    FProfessionDefinition& Definition = Catalog->Professions.FindChecked(TEXT("Warrior"));
    Definition.bUseUnitClassDefaults = false;
    Definition.CombatClass = APlayerUnit::StaticClass();
    Definition.StartingSkills = {Skill};
    FPartySnapshot Snapshot;
    Snapshot.SnapshotId = TEXT("StatBoundaries");
    FPartySnapshotMember& Member = Snapshot.Members.AddDefaulted_GetRef();
    Member.MemberId = TEXT("Warrior1");
    Member.ClassId = TEXT("Warrior");
    Member.CharacterName = TEXT("Boundary Warrior");
    Member.SkillIds = {TEXT("Unarmed")};
    const auto CheckAgreement = [this, Unit, Catalog, &Definition, &Snapshot, &Member](const TCHAR* Label, bool bExpected)
    {
        FProfessionDefinition Resolved;
        FText Error;
        Member.Stats.MaxHP = Definition.MaxHP;
        Member.Stats.CurrentHP = Definition.MaxHP;
        Member.Stats.Speed = Definition.Speed;
        Member.Stats.MaxActionPoints = Definition.ActionPoints;
        Member.Stats.MaxSubActionPoints = Definition.SubActionPoints;
        TestEqual(Label, Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error), bExpected);
        TestEqual(TEXT("Live unit agrees with catalog boundaries"), Unit->ConfigureProfession(Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, Definition.StartingSkills, Definition.Speed), bExpected);
        TestEqual(TEXT("Snapshot agrees with catalog boundaries"), UPartySnapshotLibrary::ValidateSnapshot(Snapshot, Error), bExpected);
    };
    Definition.MaxHP = UnitDataRules::MaxStatValue;
    Definition.ActionPoints = UnitDataRules::MaxActionPoints;
    Definition.SubActionPoints = UnitDataRules::MaxActionPoints;
    Definition.Speed = UnitDataRules::MaxStatValue;
    CheckAgreement(TEXT("Upper boundaries are accepted at all entry points"), true);
    Definition.ActionPoints++;
    CheckAgreement(TEXT("AP beyond checkpoint capacity is rejected before spawning"), false);
    Definition.ActionPoints--;
    Definition.SubActionPoints++;
    CheckAgreement(TEXT("SubAP beyond checkpoint capacity is rejected before spawning"), false);
    Definition.SubActionPoints--;
    Definition.MaxHP++;
    CheckAgreement(TEXT("HP beyond checkpoint capacity is rejected before spawning"), false);
    Definition.MaxHP = std::numeric_limits<float>::quiet_NaN();
    CheckAgreement(TEXT("Nonfinite HP is rejected at every entry point"), false);
    TestEqual(TEXT("Rejected data preserves the previous live AP"), Unit->GetMaxActionPoint(), UnitDataRules::MaxActionPoints);
    TestEqual(TEXT("Rejected data preserves the previous live HP"), Unit->GetAttributeSet()->GetMaxHP(), UnitDataRules::MaxStatValue);
    Definition.MaxHP = 100.0f;
    for (float InvalidSpeed : {-1.0f, UnitDataRules::MaxStatValue + 1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        Definition.Speed = InvalidSpeed;
        TestFalse(TEXT("The shared speed rule rejects out-of-range and nonfinite values"), UnitDataRules::IsValidSpeed(InvalidSpeed));
        CheckAgreement(TEXT("Invalid speed is rejected at every entry point"), false);
        TestEqual(TEXT("Rejected speed preserves the previous live value"), Unit->GetAttributeSet()->GetSpeed(), UnitDataRules::MaxStatValue);
    }
    Definition.Speed = 0.0f;
    TestTrue(TEXT("Zero is within the shared speed range"), UnitDataRules::IsValidSpeed(Definition.Speed));
    CheckAgreement(TEXT("Zero speed is accepted at every entry point"), true);
    TestEqual(TEXT("Accepted zero speed reaches the live GAS attribute"), Unit->GetAttributeSet()->GetSpeed(), 0.0f);
    Definition.StartingSkills.Add(Skill);
    FProfessionDefinition Resolved;
    FText Error;
    TestFalse(TEXT("Catalog rejects duplicate skills"), Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error));
    TestFalse(TEXT("Live loadout rejects the same duplicate skills"), Unit->ConfigureProfession(100.0f, 2, 1, Definition.StartingSkills));
    TestEqual(TEXT("Invalid loadout leaves equipped skills intact"), Unit->GetEquippedSkillDataAssets().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitUnlimitedSkillLoadoutTest, "ProjectA.Party.UnlimitedOrderedSkillLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnitUnlimitedSkillLoadoutTest::RunTest(const FString& Parameters)
{
    UnitDataRulesTests::FScopedWorld Scope;
    APlayerUnit* Unit = Scope.SpawnUnit();
    UPartyDefinitionDataAsset* Catalog = NewObject<UPartyDefinitionDataAsset>();
    UOpponentSnapshotCatalogDataAsset* OpponentCatalog = NewObject<UOpponentSnapshotCatalogDataAsset>();
    FProfessionDefinition& Definition = Catalog->Professions.FindChecked(TEXT("Warrior"));
    Definition.bUseUnitClassDefaults = false;
    Definition.CombatClass = APlayerUnit::StaticClass();
    Definition.StartingSkills.Reset();
    FRunPartyMember Member;
    Member.ClassId = TEXT("Warrior");
    Member.bHasSkillLoadout = true;
    FPartySnapshotMember Opponent;
    for (int32 Index = 0; Index < 6; ++Index)
    {
        USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Catalog);
        Skill->bUseRoundDefinition = true;
        Definition.StartingSkills.Add(Skill);
        Member.Skills.Add(FSoftObjectPath(Skill));
        const FName SkillId(*FString::Printf(TEXT("UnlimitedSkill_%d"), Index));
        Opponent.SkillIds.Add(SkillId);
        OpponentCatalog->Skills.Add(SkillId, Skill);
    }
    FText Error;
    FProfessionDefinition Resolved;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> ResolvedSkills;
    TestTrue(TEXT("A profession may define more than five unique skills"), Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error));
    TestTrue(TEXT("A live unit accepts the complete ordered loadout"), Unit->ConfigureProfession(100.0f, 2, 1, Definition.StartingSkills));
    TestTrue(TEXT("A stored party member resolves more than five skills"), Catalog->ResolveMemberSkills(Member, ResolvedSkills, Error));
    TestTrue(TEXT("Stored member resolution preserves every skill in order"), ResolvedSkills == Definition.StartingSkills);
    TestTrue(TEXT("An opponent catalog resolves more than five skills"), OpponentCatalog->ResolveSkills(Opponent, ResolvedSkills, Error));
    TestTrue(TEXT("Opponent resolution preserves every skill in order"), ResolvedSkills == Definition.StartingSkills);
    TestTrue(TEXT("Skill counts have no gameplay upper limit"), UnitDataRules::IsValidSkillCount(500, true));
    TestFalse(TEXT("Negative skill counts remain invalid"), UnitDataRules::IsValidSkillCount(-1, false));
    TestFalse(TEXT("Required skill loadouts still reject zero skills"), UnitDataRules::IsValidSkillCount(0, true));
    TestTrue(TEXT("Optional skill loadouts still allow zero skills"), UnitDataRules::IsValidSkillCount(0, false));
    USkillDefinitionDataAsset* DuplicateSkill = Definition.StartingSkills[0];
    Definition.StartingSkills.Add(DuplicateSkill);
    TestFalse(TEXT("An unlimited profession still rejects duplicate skill IDs"), Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error));
    TestFalse(TEXT("An unlimited live loadout still rejects duplicate skill IDs"), Unit->ConfigureProfession(100.0f, 2, 1, Definition.StartingSkills));
    TestEqual(TEXT("Duplicate rejection preserves all six equipped skills"), Unit->GetEquippedSkillDataAssets().Num(), 6);
    Definition.StartingSkills.Last() = nullptr;
    TestFalse(TEXT("An unlimited profession still rejects missing skill assets"), Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error));
    TestFalse(TEXT("An unlimited live loadout still rejects missing skill assets"), Unit->ConfigureProfession(100.0f, 2, 1, Definition.StartingSkills));
    Definition.StartingSkills.Pop();
    const FSoftObjectPath DuplicatePath = Member.Skills[0];
    Member.Skills.Add(DuplicatePath);
    TestFalse(TEXT("An unlimited stored member still rejects duplicate skill assets"), Catalog->ResolveMemberSkills(Member, ResolvedSkills, Error));
    const FName DuplicateId = Opponent.SkillIds[0];
    Opponent.SkillIds.Add(DuplicateId);
    TestFalse(TEXT("An unlimited opponent still rejects duplicate skill assets"), OpponentCatalog->ResolveSkills(Opponent, ResolvedSkills, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstantEffectApplicationResultTest, "ProjectA.Combat.Effects.InstantApplicationResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInstantEffectApplicationResultTest::RunTest(const FString& Parameters)
{
    UnitDataRulesTests::FScopedWorld Scope;
    APlayerUnit* Source = Scope.SpawnUnit();
    APlayerUnit* Target = Scope.SpawnUnit();
    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    TargetASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda([](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { return false; }));
    TestFalse(TEXT("An Instant effect rejected by GAS is reported as rejected"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 10.0f));
    TestEqual(TEXT("Rejected Instant damage does not count as an applied target"), UCombatEffectLibrary::ApplyDamageToUnits(Source, {Target, Target}, UGE_Damage::StaticClass(), 10.0f), 0);
    TestEqual(TEXT("Rejected effects preserve HP"), Target->GetAttributeSet()->GetHP(), 100.0f);
    TargetASC->GameplayEffectApplicationQueries.Reset();
    TestTrue(TEXT("Successful Instant damage does not require an active effect handle"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 10.0f));
    TestEqual(TEXT("Successful Instant damage changes HP once"), Target->GetAttributeSet()->GetHP(), 90.0f);
    return true;
}

#endif
