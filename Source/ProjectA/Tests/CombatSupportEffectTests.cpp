#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Engine/World.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "GAS/Effect/GE_Damage.h"
#include "GAS/Effect/GE_Heal.h"
#include "GAS/Effect/GE_Shield.h"
#include "Unit/PlayerUnit.h"
#include <limits>

namespace CombatSupportEffectTests
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
            if (!World) return nullptr;
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            APlayerUnit* Unit = World->SpawnActor<APlayerUnit>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
            if (!Unit) return nullptr;
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            return Unit;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSupportEffectLifecycleTest, "ProjectA.Combat.Effects.HealAndRoundShield", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSupportEffectLifecycleTest::RunTest(const FString& Parameters)
{
    CombatSupportEffectTests::FScopedWorld Scope;
    APlayerUnit* Source = Scope.SpawnUnit();
    APlayerUnit* Target = Scope.SpawnUnit();
    if (!TestNotNull(TEXT("Source exists"), Source) || !TestNotNull(TEXT("Target exists"), Target)) return false;
    UAS_Unit* Attributes = Target->GetAttributeSet();
    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    const FGameplayTagContainer HealTags(ProjectACombatTags::Skill_Effect_Heal);
    const FGameplayTagContainer ShieldTags(ProjectACombatTags::Skill_Effect_Shield);
    Attributes->SetHP(50.0f);
    TestTrue(TEXT("Positive heal power applies through GAS"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Heal::StaticClass(), 30.0f, HealTags));
    TestEqual(TEXT("Healing restores immediate HP"), Attributes->GetHP(), 80.0f);
    TestTrue(TEXT("An overheal is accepted and clamped"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Heal::StaticClass(), 50.0f, HealTags));
    TestEqual(TEXT("Healing never exceeds MaxHP"), Attributes->GetHP(), 100.0f);
    TestTrue(TEXT("Positive shield power applies through GAS"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Shield::StaticClass(), 40.0f, ShieldTags));
    TestEqual(TEXT("Shield is stored separately from HP"), Attributes->GetShield(), 40.0f);
    TestTrue(TEXT("A fully absorbed hit still resolves"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 25.0f));
    TestEqual(TEXT("Shield absorbs HP damage first"), Attributes->GetHP(), 100.0f);
    TestEqual(TEXT("Absorption consumes only the blocked damage"), Attributes->GetShield(), 15.0f);
    TestTrue(TEXT("A hit can exceed the remaining shield"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 20.0f));
    TestEqual(TEXT("Only unabsorbed damage reaches HP"), Attributes->GetHP(), 95.0f);
    TestEqual(TEXT("Shield cannot become negative"), Attributes->GetShield(), 0.0f);
    TestTrue(TEXT("Shield can be granted again"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Shield::StaticClass(), 30.0f, ShieldTags));
    TestTrue(TEXT("Recasting adds new shield power"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Shield::StaticClass(), 10.0f, ShieldTags));
    TestEqual(TEXT("Recasting preserves existing shield and adds new power"), Attributes->GetShield(), 40.0f);
    TestTrue(TEXT("Repeated recasting adds the same power again"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Shield::StaticClass(), 10.0f, ShieldTags));
    TestEqual(TEXT("Repeated casts accumulate shield power"), Attributes->GetShield(), 50.0f);
    TestTrue(TEXT("Damage consumes the accumulated shield"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 35.0f));
    TestEqual(TEXT("Absorption retains only the unconsumed shield"), Attributes->GetShield(), 15.0f);
    TestTrue(TEXT("Shield can be recast after partial absorption"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Shield::StaticClass(), 15.0f, ShieldTags));
    TestEqual(TEXT("Recasting after damage adds power to the remaining shield"), Attributes->GetShield(), 30.0f);
    TestEqual(TEXT("Stacked shield absorption preserves HP"), Attributes->GetHP(), 95.0f);
    TargetASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda([](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { return false; }));
    TestFalse(TEXT("GAS rejection occurs before shield consumption"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 50.0f));
    TestEqual(TEXT("Rejected damage preserves the shield"), Attributes->GetShield(), 30.0f);
    TargetASC->GameplayEffectApplicationQueries.Reset();
    TestTrue(TEXT("Round cleanup explicitly clears shield"), UCombatEffectLibrary::ClearRoundShield(Target));
    TestEqual(TEXT("Planning boundaries hold no shield state"), Attributes->GetShield(), 0.0f);
    TestEqual(TEXT("Clearing the shield does not modify HP"), Attributes->GetHP(), 95.0f);
    TestFalse(TEXT("Negative support power is rejected"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Heal::StaticClass(), -10.0f, HealTags));
    TestFalse(TEXT("Nonfinite support power is rejected"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Shield::StaticClass(), std::numeric_limits<float>::quiet_NaN(), ShieldTags));
    FGameplayTagContainer ConflictingTags = HealTags;
    ConflictingTags.AddTag(ProjectACombatTags::Skill_Effect_Shield);
    TestFalse(TEXT("Contradictory heal and shield tags are rejected"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Heal::StaticClass(), 10.0f, ConflictingTags));
    TestFalse(TEXT("A damage effect cannot silently execute as healing"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Damage::StaticClass(), 10.0f, HealTags));
    TestEqual(TEXT("Rejected effect contracts preserve HP"), Attributes->GetHP(), 95.0f);
    TestTrue(TEXT("Unshielded lethal damage applies"), UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), 200.0f));
    TestFalse(TEXT("Lethal damage resolves unit death"), Target->IsUnitAlive());
    TestFalse(TEXT("Healing cannot revive a dead unit"), UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, UGE_Heal::StaticClass(), 100.0f, HealTags));
    TestEqual(TEXT("Dead HP remains zero"), Attributes->GetHP(), 0.0f);
    return true;
}

#endif
