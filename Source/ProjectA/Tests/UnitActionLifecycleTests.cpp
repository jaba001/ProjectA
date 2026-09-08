#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
#include "Unit/EnemyUnit.h"
#include "Unit/UnitBase.h"
#include "UObject/UnrealType.h"

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
            if (World)
            {
                World->DestroyWorld(false);
            }
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
            Unit->bIsActiveTurn = true;
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
        Skill->bMoveToTarget = false;
        Skill->ActionPointCost = 1;
        Skill->TargetRule = ESkillTargetRule::EnemyUnit;
        return Skill;
    }

    void SetMovementPhase(AUnitBase* Unit, EUnitMovePhase Phase)
    {
        // Inject only navigation phase; production action start and recovery stay intact.
        // 이동 단계만 주입하며 실제 행동 시작과 복구 코드는 그대로 실행합니다.
        FEnumProperty* Property = FindFProperty<FEnumProperty>(AUnitBase::StaticClass(), TEXT("MovePhase"));
        check(Property);
        Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Unit), static_cast<uint64>(Phase));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitInstantSkillTest, "ProjectA.Combat.Actions.InstantSkillAndActivationFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnitInstantSkillTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Scope.SpawnTile(Unit);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UGA_DefaultAttack::StaticClass(), 1));
    UGameplayAbility* Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
    FClassProperty* DamageClassProperty = FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("DamageEffectClass"));
    if (!TestNotNull(TEXT("Attack instance exists"), Ability) || !TestNotNull(TEXT("Damage class property exists"), DamageClassProperty))
    {
        return false;
    }

    DamageClassProperty->SetObjectPropertyValue_InContainer(Ability, UGE_Damage::StaticClass());
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_DefaultAttack::StaticClass());
    int32 Completions = 0;
    EUnitActionResult LastResult = EUnitActionResult::Failed;
    Unit->OnActionCompleted.AddLambda([&](AUnitBase* CompletedUnit, EUnitActionType Type, EUnitActionResult Result)
    {
        ++Completions;
        LastResult = Result;
        TestEqual(TEXT("Skill result type"), Type, EUnitActionType::Skill);
        TestFalse(TEXT("Completion sees cleared busy state"), CompletedUnit->IsBusy());
        TestNull(TEXT("Completion sees cleared context"), CompletedUnit->PendingSkillData.Get());
    });

    Unit->StartSkill(Skill, TargetTile);
    TestEqual(TEXT("Immediate completion occurs once"), Completions, 1);
    TestEqual(TEXT("No montage applies real damage"), Target->GetAttributeSet()->GetHP(), 90.0f);
    TestEqual(TEXT("AP consumed once"), Unit->GetCurrentActionPoint(), 1);
    TestEqual(TEXT("Immediate success result"), LastResult, EUnitActionResult::Succeeded);
    Unit->OnSkillFinished();
    Unit->OnSnapToTileFinished();
    TestEqual(TEXT("Stale callbacks do not duplicate completion"), Completions, 1);

    Unit->StartSkill(Skill, TargetTile);
    TestEqual(TEXT("Next action can start immediately"), Completions, 2);
    TestEqual(TEXT("Second action damage"), Target->GetAttributeSet()->GetHP(), 80.0f);

    // Retain the separate data/ability costs and test their existing rejection path.
    // 데이터와 어빌리티의 비용 분리를 유지하면서 기존 거절 경로를 검증합니다.
    Skill->ActionPointCost = 0;
    Unit->StartSkill(Skill, TargetTile);
    TestEqual(TEXT("Ability AP rejection completes once"), Completions, 3);
    TestEqual(TEXT("Ability setup rejection is failure"), LastResult, EUnitActionResult::Failed);
    TestEqual(TEXT("Rejection applies no damage"), Target->GetAttributeSet()->GetHP(), 80.0f);
    Unit->OnActionCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitHeldSkillTest, "ProjectA.Combat.Actions.BusyInputAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnitHeldSkillTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Scope.SpawnTile(Unit);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UGameplayAbility::StaticClass(), 1));
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGameplayAbility::StaticClass());
    int32 Completions = 0;
    EUnitActionResult LastResult = EUnitActionResult::Failed;
    Unit->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        LastResult = Result;
    });

    Unit->StartSkill(Skill, TargetTile);
    TestTrue(TEXT("Ability without an end remains busy"), Unit->IsBusy());
    Unit->StartMoveAction(TargetTile);
    Unit->StartSkill(Skill, TargetTile);
    Unit->OnSkillFinished();
    TestEqual(TEXT("Busy input consumes no movement resource"), Unit->GetCurrentSubActionPoint(), 1);
    TestEqual(TEXT("Busy input and legacy notify cannot complete the action"), Completions, 0);
    TestTrue(TEXT("Original ability remains active"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());

    Unit->CancelCurrentAction();
    TestEqual(TEXT("Cancellation completes once"), Completions, 1);
    TestEqual(TEXT("Cancellation result"), LastResult, EUnitActionResult::Cancelled);
    TestFalse(TEXT("Cancellation clears busy"), Unit->IsBusy());
    TestFalse(TEXT("Cancellation ends GAS ability"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    Unit->CancelCurrentAction();
    TestEqual(TEXT("Repeated cancellation is ignored"), Completions, 1);
    Unit->OnActionCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyInstantSkillTest, "ProjectA.Combat.Actions.EnemySynchronousCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyInstantSkillTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Enemy->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Enemy);
    Scope.SpawnTile(Target);
    USkillDefinitionDataAsset* Skill = MakeSkill(Enemy, UGA_DefaultAttack::StaticClass());
    FClassProperty* AttackProperty = FindFProperty<FClassProperty>(AUnitBase::StaticClass(), TEXT("DefaultAttackAbilityClass"));
    AttackProperty->SetObjectPropertyValue_InContainer(Enemy, UGA_DefaultAttack::StaticClass());
    FArrayProperty* SkillsProperty = FindFProperty<FArrayProperty>(AUnitBase::StaticClass(), TEXT("EquippedSkillDataAssets"));
    FScriptArrayHelper SkillsHelper(SkillsProperty, SkillsProperty->ContainerPtrToValuePtr<void>(Enemy));
    const int32 SkillIndex = SkillsHelper.AddValue();
    CastFieldChecked<FObjectPropertyBase>(SkillsProperty->Inner)->SetObjectPropertyValue(SkillsHelper.GetRawPtr(SkillIndex), Skill);
    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UGA_DefaultAttack::StaticClass(), 1));
    UGameplayAbility* Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
    FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("DamageEffectClass"))->SetObjectPropertyValue_InContainer(Ability, UGE_Damage::StaticClass());
    FindFProperty<FIntProperty>(AUnitBase::StaticClass(), TEXT("MaxActionPoint"))->SetPropertyValue_InContainer(Enemy, 1);
    int32 Completions = 0;
    Enemy->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        TestEqual(TEXT("Enemy synchronous result succeeds"), Result, EUnitActionResult::Succeeded);
    });

    Enemy->OnTurnStart();
    TestEqual(TEXT("Enemy instant action completes during turn start"), Completions, 1);
    TestEqual(TEXT("Enemy instant action applies real damage"), Target->GetAttributeSet()->GetHP(), 90.0f);
    TestFalse(TEXT("Enemy instant ability has ended"), Enemy->IsBusy());
    Scope.World->GetTimerManager().Tick(0.01f);
    TestEqual(TEXT("Synchronous completion reaches safe turn end"), Enemy->GetTurnState(), EEnemyTurnState::EndTurn);
    TestEqual(TEXT("Enemy turn completion is not duplicated"), Completions, 1);
    Enemy->OnTurnEnd();
    Enemy->OnActionCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyActionRecoveryTest, "ProjectA.Combat.Actions.EnemyMovementRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyActionRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    for (EUnitMovePhase Phase : { EUnitMovePhase::MovingToTarget, EUnitMovePhase::ReturningToOriginalTile })
    {
        FScopedWorld Scope;
        AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector(0.0f, 0.0f, 100.0f));
        AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
        Enemy->SetTeam(ETeam::Enemy);
        ACombatGridTile* OriginTile = Scope.SpawnTile(Enemy);
        ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
        Enemy->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UGameplayAbility::StaticClass(), 1));
        USkillDefinitionDataAsset* Skill = MakeSkill(Enemy, UGameplayAbility::StaticClass());
        Enemy->SetTurnState(EEnemyTurnState::WaitSkillComplete);
        const FVector Origin = Enemy->GetActorLocation();
        int32 Completions = 0;
        Enemy->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult)
        {
            ++Completions;
        });

        Enemy->StartSkill(Skill, TargetTile);
        SetMovementPhase(Enemy, Phase);
        Enemy->SetActorLocation(Origin + FVector(200.0f, 40.0f, 0.0f));
        Enemy->HandleMoveFailed(EUnitActionResult::Cancelled);
        TestEqual(TEXT("Movement interruption completes once"), Completions, 1);
        TestFalse(TEXT("Movement interruption clears busy"), Enemy->IsBusy());
        TestTrue(TEXT("Actor returns to reserved origin"), Enemy->GetActorLocation().Equals(Origin));
        TestEqual(TEXT("Unit tile returns to origin"), Enemy->GetCurrentTile(), OriginTile);
        TestEqual(TEXT("Origin occupancy remains consistent"), OriginTile->GetOccupyingUnit(), static_cast<AUnitBase*>(Enemy));
        TestEqual(TEXT("Target occupancy is preserved"), TargetTile->GetOccupyingUnit(), Target);

        Scope.World->GetTimerManager().Tick(0.01f);
        TestEqual(TEXT("AI exits Wait on next tick"), Enemy->GetTurnState(), EEnemyTurnState::EndTurn);
        Enemy->HandleMoveFailed();
        TestEqual(TEXT("Late failure cannot duplicate completion"), Completions, 1);
        Enemy->OnTurnEnd();
        Enemy->OnActionCompleted.Clear();
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyMoveRequestFailureTest, "ProjectA.Combat.Actions.ImmediateMoveRequestFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyMoveRequestFailureTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    ACombatGridTile* OriginTile = Scope.SpawnTile(Enemy);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    USkillDefinitionDataAsset* Skill = MakeSkill(Enemy, UGameplayAbility::StaticClass());
    Skill->bMoveToTarget = true;
    Enemy->SetTurnState(EEnemyTurnState::WaitSkillComplete);
    int32 Completions = 0;
    Enemy->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        TestEqual(TEXT("Rejected navigation request reports failure"), Result, EUnitActionResult::Failed);
    });

    Enemy->StartSkill(Skill, TargetTile);
    TestEqual(TEXT("No-navigation request resolves once"), Completions, 1);
    TestFalse(TEXT("No-navigation request does not stay busy"), Enemy->IsBusy());
    TestEqual(TEXT("Failed request retains its tile"), Enemy->GetCurrentTile(), OriginTile);
    Scope.World->GetTimerManager().Tick(0.01f);
    TestEqual(TEXT("Rejected request leaves AI Wait"), Enemy->GetTurnState(), EEnemyTurnState::EndTurn);
    Enemy->OnTurnEnd();
    Enemy->OnActionCompleted.Clear();
    return true;
}

#endif
