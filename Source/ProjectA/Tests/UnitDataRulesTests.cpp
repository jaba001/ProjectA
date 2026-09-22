#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
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
        Member.Stats.MaxActionPoints = Definition.ActionPoints;
        Member.Stats.MaxSubActionPoints = Definition.SubActionPoints;
        TestEqual(Label, Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error), bExpected);
        TestEqual(TEXT("Live unit agrees with catalog boundaries"), Unit->ConfigureProfession(Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, Definition.StartingSkills), bExpected);
        TestEqual(TEXT("Snapshot agrees with catalog boundaries"), UPartySnapshotLibrary::ValidateSnapshot(Snapshot, Error), bExpected);
    };
    Definition.MaxHP = UnitDataRules::MaxStatValue;
    Definition.ActionPoints = UnitDataRules::MaxActionPoints;
    Definition.SubActionPoints = UnitDataRules::MaxActionPoints;
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
    Definition.StartingSkills.Add(Skill);
    FProfessionDefinition Resolved;
    FText Error;
    TestFalse(TEXT("Catalog rejects duplicate skills"), Catalog->ResolveProfession(TEXT("Warrior"), Resolved, Error));
    TestFalse(TEXT("Live loadout rejects the same duplicate skills"), Unit->ConfigureProfession(100.0f, 2, 1, Definition.StartingSkills));
    TestEqual(TEXT("Invalid loadout leaves equipped skills intact"), Unit->GetEquippedSkillDataAssets().Num(), 1);
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
