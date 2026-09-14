#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/SkillActor/AttackSkillActorBase.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "DataAsset/SkillPoolDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Ability/GA_AreaAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/PlayerUnit.h"
#include "Unit/UnitBase.h"
#include <limits>

namespace UnitActionLifecycleTests
{
    struct FScopedWorld
    {
        UWorld* World = nullptr;

        FScopedWorld()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        }

        ~FScopedWorld()
        {
            World->DestroyWorld(false);
        }

        template <typename T>
        T* SpawnUnit(const FVector& Location)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            T* Unit = World->SpawnActor<T>(Location, FRotator::ZeroRotator, Parameters);
            Unit->GetAbilitySystemComponent()->InitAbilityActorInfo(Unit, Unit);
            Unit->GetAbilitySystemComponent()->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            Unit->ResetActionPoint();
            Unit->ResetSubActionPoint();
            return Unit;
        }

        ACombatGridTile* SpawnTile(AUnitBase* Unit)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ACombatGridTile* Tile = World->SpawnActor<ACombatGridTile>(Unit->GetActorLocation(), FRotator::ZeroRotator, Parameters);
            Unit->SetCurrentTile(Tile);
            return Tile;
        }
    };

    USkillDefinitionDataAsset* MakeSkill(UObject* Outer, TSubclassOf<UGameplayAbility> AbilityClass)
    {
        USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Outer);
        Skill->AbilityClass = AbilityClass;
        Skill->ActionPointCost = 1;
        Skill->TargetRule = ESkillTargetRule::EnemyUnit;
        Skill->AreaType = ESkillAreaType::Single;
        return Skill;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitRetiredActionTest, "ProjectA.Combat.Actions.RetiredExecutionIsInert", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnitRetiredActionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Target->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Unit);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_DefaultAttack::StaticClass());
    const FVector Location = Unit->GetActorLocation();
    const int32 AP = Unit->GetCurrentActionPoint();
    const int32 SubAP = Unit->GetCurrentSubActionPoint();
    const int32 Stock = Unit->HealingItemCount;
    Unit->StartSkill(Skill, TargetTile);
    Unit->StartMoveAction(TargetTile);
    Unit->StartItemAction(Unit);
    Unit->OnTurnStart();
    TestFalse(TEXT("Retired turn callback cannot activate sequential actions"), Unit->IsActiveTurn());
    TestFalse(TEXT("Retired actions cannot leave a busy lifecycle"), Unit->IsBusy());
    TestFalse(TEXT("Immediate healing entry is unavailable"), Unit->CanUseHealingItem(Unit));
    TestEqual(TEXT("Retired attack does not apply tile damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
    TestEqual(TEXT("Retired entries preserve AP"), Unit->GetCurrentActionPoint(), AP);
    TestEqual(TEXT("Retired entries preserve sub AP"), Unit->GetCurrentSubActionPoint(), SubAP);
    TestEqual(TEXT("Retired entries preserve item stock"), Unit->HealingItemCount, Stock);
    TestEqual(TEXT("Retired movement cannot relocate actors"), Unit->GetActorLocation(), Location);
    TestFalse(TEXT("Default legacy ability rejects activation"), GetDefault<UGA_DefaultAttack>()->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));
    TestFalse(TEXT("Area legacy ability rejects activation"), GetDefault<UGA_AreaAttack>()->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));
    AAttackSkillActorBase* LegacyActor = Scope.World->SpawnActor<AAttackSkillActorBase>();
    int32 Resolutions = 0;
    bool bSucceeded = true;
    LegacyActor->OnSkillActorResolved.AddLambda([&](ASkillActorBase*, bool bSuccess)
    {
        ++Resolutions;
        bSucceeded = bSuccess;
    });
    FSkillActorInitData Context;
    Context.SourceUnit = Unit;
    Context.SkillData = Skill;
    Context.TargetTile = TargetTile;
    LegacyActor->InitializeAttackSkillActor(Context, UGE_Damage::StaticClass(), 90.0f);
    LegacyActor->RequestImpact();
    LegacyActor->RequestFinish();
    TestEqual(TEXT("Legacy spawned attack resolves exactly once"), Resolutions, 1);
    TestFalse(TEXT("Legacy spawned attack reports rejection"), bSucceeded);
    TestEqual(TEXT("Legacy impact callback cannot apply damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
    LegacyActor->OnSkillActorResolved.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSkillAcquisitionTest, "ProjectA.Combat.Content.SkillAcquisitionRetainsRoundData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatSkillAcquisitionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    USkillPoolDataAsset* Pool = NewObject<USkillPoolDataAsset>(Unit);
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_AreaAttack::StaticClass());
    FSkillPoolEntry Entry;
    Entry.Skill = Skill;
    Entry.Weight = 1;
    Pool->Entries.Add(Entry);
    TestEqual(TEXT("Pool reward equips the authored definition"), Unit->AcquireSkillFromPool(Pool), Skill);
    TestEqual(TEXT("Equipped lookup matches acquired data"), Unit->FindSkillDataByAbilityClass(Skill->AbilityClass), Skill);
    TestTrue(TEXT("Acquired data is available to round planning"), Unit->GetEquippedSkillDataAssets().Contains(Skill));
    TestNull(TEXT("Owned skill is excluded from pool"), Unit->AcquireSkillFromPool(Pool));
    TestFalse(TEXT("Duplicate direct acquisition is rejected"), Unit->AcquireAndEquipSkill(Skill));
    TestNull(TEXT("Data acquisition does not grant a retired executable ability"), Unit->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Skill->AbilityClass));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundSkillMigrationTest, "ProjectA.Combat.Content.RoundSkillMigrationValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundSkillMigrationTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    USkillDefinitionDataAsset* Skill = MakeSkill(GetTransientPackage(), UGA_DefaultAttack::StaticClass());
    FCombatRoundSkill Resolved;
    FText Error;
    TestTrue(TEXT("Supported old single enemy attack resolves"), Skill->ResolveRoundSkill(Resolved, Error));
    TestEqual(TEXT("Migration reads authored CDO damage only"), Resolved.Power, GetDefault<UGA_DefaultAttack>()->GetAuthoredDamageAmount());
    Skill->AreaType = ESkillAreaType::AroundSelf;
    TestFalse(TEXT("Legacy self-centered area requires explicit semantics"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->AreaType = ESkillAreaType::Single;
    Skill->TargetRule = ESkillTargetRule::EnemyTile;
    TestFalse(TEXT("Legacy tile targeting requires explicit semantics"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->bUseRoundDefinition = true;
    Skill->AbilityClass = nullptr;
    TestTrue(TEXT("Explicit round definitions do not require an ability class"), Skill->ResolveRoundSkill(Resolved, Error));
    TestFalse(TEXT("Resolved explicit definition has a stable identifier"), Resolved.SkillId.IsNone());
    Skill->RoundDefinition.Kind = ECombatRoundSkillKind::Wait;
    Skill->RoundDefinition.Approach = ECombatRoundApproach::Tile;
    TestFalse(TEXT("Wait cannot silently move to a tile"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->RoundDefinition.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill->RoundDefinition.Approach = ECombatRoundApproach::Unit;
    TestFalse(TEXT("Ground attack cannot silently track a unit"), Skill->ResolveRoundSkill(Resolved, Error));
    Skill->RoundDefinition = FCombatRoundSkill();
    Skill->RoundDefinition.Power = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite damage is rejected before simulation"), Skill->ResolveRoundSkill(Resolved, Error));
    TestFalse(TEXT("Invalid definition reports the asset error"), Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfessionLoadoutTest, "ProjectA.Party.ProfessionLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProfessionLoadoutTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestNotNull(TEXT("Shared party catalog exists"), Catalog))
    {
        return false;
    }
    FScopedWorld Scope;
    for (FName Id : { FName(TEXT("StableHand")), FName(TEXT("Scholar")), FName(TEXT("Herbalist")), FName(TEXT("Hunter")) })
    {
        FProfessionDefinition Definition;
        if (!TestTrue(TEXT("Profession resolves combat defaults"), Catalog->ResolveProfession(Id, Definition)))
        {
            return false;
        }
        TestFalse(TEXT("Profession has a readable name"), Definition.DisplayName.IsEmpty());
        TestFalse(TEXT("Profession has a description"), Definition.Description.IsEmpty());
        TestTrue(TEXT("Preview shows actual HP"), Catalog->GetProfessionDetails(Id).ToString().Contains(FString::Printf(TEXT("HP %.0f"), Definition.MaxHP)));
        APlayerUnit* Unit = Scope.SpawnUnit<APlayerUnit>(FVector::ZeroVector);
        Unit->ConfigureProfession(Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, Definition.StartingSkills);
        TestEqual(TEXT("Spawn HP equals preview"), Unit->GetAttributeSet()->GetHP(), Definition.MaxHP);
        TestEqual(TEXT("Spawn AP equals preview"), Unit->GetMaxActionPoint(), Definition.ActionPoints);
        TestEqual(TEXT("Spawn sub AP equals preview"), Unit->GetMaxSubActionPoint(), Definition.SubActionPoints);
        for (USkillDefinitionDataAsset* Skill : Definition.StartingSkills)
        {
            TestTrue(TEXT("Every displayed skill remains equipped as round data"), Unit->GetEquippedSkillDataAssets().Contains(Skill));
            FCombatRoundSkill RoundSkill;
            FText Error;
            TestTrue(TEXT("Every displayed skill has a valid round definition"), Skill->ResolveRoundSkill(RoundSkill, Error));
        }
    }
    UPartyDefinitionDataAsset* Custom = DuplicateObject<UPartyDefinitionDataAsset>(Catalog, GetTransientPackage());
    FProfessionDefinition& Override = Custom->Professions.FindChecked(TEXT("Scholar"));
    FProfessionDefinition Base;
    Catalog->ResolveProfession(TEXT("Scholar"), Base);
    Override.bUseUnitClassDefaults = false;
    Override.StartingSkills = Base.StartingSkills;
    Override.MaxHP = 137.0f;
    Override.ActionPoints = 3;
    Override.SubActionPoints = 2;
    FProfessionDefinition Resolved;
    TestTrue(TEXT("Explicit profession tuning resolves"), Custom->ResolveProfession(TEXT("Scholar"), Resolved));
    APlayerUnit* Tuned = Scope.SpawnUnit<APlayerUnit>(FVector::ZeroVector);
    Tuned->ConfigureProfession(Resolved.MaxHP, Resolved.ActionPoints, Resolved.SubActionPoints, Resolved.StartingSkills);
    TestEqual(TEXT("Override applies HP"), Tuned->GetAttributeSet()->GetHP(), 137.0f);
    TestEqual(TEXT("Override applies AP"), Tuned->GetMaxActionPoint(), 3);
    TestTrue(TEXT("Override also changes preview"), Custom->GetProfessionDetails(TEXT("Scholar")).ToString().Contains(TEXT("HP 137")));
    const TObjectPtr<USkillDefinitionDataAsset> DuplicateSkill = Override.StartingSkills[0];
    Override.StartingSkills.Add(DuplicateSkill);
    TestFalse(TEXT("Duplicate skill asset is rejected"), Custom->ResolveProfession(TEXT("Scholar"), Resolved));
    Override.StartingSkills.Pop();
    Override.MaxHP = -1.0f;
    TestFalse(TEXT("Invalid tuning is rejected"), Custom->ResolveProfession(TEXT("Scholar"), Resolved));
    TestFalse(TEXT("Unknown professions do not silently use fallback"), Custom->ResolveProfession(TEXT("Warrior"), Resolved));
    return true;
}

#endif
