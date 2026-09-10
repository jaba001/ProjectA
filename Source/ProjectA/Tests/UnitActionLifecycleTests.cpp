#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Ability/GA_AreaAttack.h"
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

    UGameplayAbility* GrantAttack(AUnitBase* Unit)
    {
        UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
        const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UGA_DefaultAttack::StaticClass(), 1));
        UGameplayAbility* Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
        FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("DamageEffectClass"))->SetObjectPropertyValue_InContainer(Ability, UGE_Damage::StaticClass());
        return Ability;
    }

    void EquipSkill(AUnitBase* Unit, USkillDefinitionDataAsset* Skill)
    {
        FArrayProperty* Property = FindFProperty<FArrayProperty>(AUnitBase::StaticClass(), TEXT("EquippedSkillDataAssets"));
        FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Unit));
        const int32 Index = Helper.AddValue();
        CastFieldChecked<FObjectPropertyBase>(Property->Inner)->SetObjectPropertyValue(Helper.GetRawPtr(Index), Skill);
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

    // Reject invalid definition costs before ability activation.
    // 잘못된 정의 비용은 어빌리티 활성화 전에 거절합니다.
    Skill->ActionPointCost = 0;
    Unit->StartSkill(Skill, TargetTile);
    TestEqual(TEXT("Invalid cost rejection completes once"), Completions, 3);
    TestEqual(TEXT("Invalid cost reports failure"), LastResult, EUnitActionResult::Failed);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillDefinitionCostTest, "ProjectA.Combat.Costs.DefinitionAndInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillDefinitionCostTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Unit->SetTeam(ETeam::Player);
    Target->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Unit);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    UGameplayAbility* Ability = GrantAttack(Unit);
    // Deliberately conflict with legacy serialized costs to detect accidental reuse.
    // 과거 직렬화 비용을 일부러 다르게 설정해 잘못된 재사용을 검출합니다.
    FindFProperty<FIntProperty>(UGA_AttackBase::StaticClass(), TEXT("ActionPointCost"))->SetPropertyValue_InContainer(Ability, 99);
    ACombatManager* Combat = Scope.World->SpawnActor<ACombatManager>();
    Combat->RegisterUnits({ Unit, Target });
    Combat->StartCombat_Internal();
    APartyPlayerController* Controller = Scope.World->SpawnActor<APartyPlayerController>();
    Controller->SetCombatContext(Combat, true);
    int32 Completions = 0;
    EUnitActionResult LastResult = EUnitActionResult::Failed;
    Unit->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        LastResult = Result;
    });

    for (int32 AvailableAP : { 2, 1 })
    {
        for (int32 Cost : { 1, 2, 3, 0, -1 })
        {
            Unit->OnTurnStart();
            if (AvailableAP == 1)
            {
                Unit->ConsumeActionPoint(1);
            }
            USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_DefaultAttack::StaticClass());
            Skill->ActionPointCost = Cost;
            const bool bExpectedUsable = Cost > 0 && Cost <= AvailableAP;
            const FString Context = FString::Printf(TEXT("AP=%d Cost=%d"), AvailableAP, Cost);
            TestEqual(Context + TEXT(" unit affordability"), Unit->HasEnoughActionPoint(Cost), bExpectedUsable);
            TestEqual(Context + TEXT(" HUD affordability"), Controller->CanUseActiveUnitActionPoint(Cost), bExpectedUsable);
            if (Cost > 0)
            {
                TestEqual(Context + TEXT(" displayed cost"), Skill->GetActionPointCostText().ToString(), FString::Printf(TEXT("AP %d"), Cost));
            }
            else
            {
                TestTrue(Context + TEXT(" invalid data is visibly identified"), Skill->GetActionPointCostText().ToString().Contains(TEXT("Invalid AP")));
                TestFalse(Context + TEXT(" direct invalid consumption rejected"), Unit->ConsumeActionPoint(Cost));
                TestEqual(Context + TEXT(" invalid consumption never grants AP"), Unit->GetCurrentActionPoint(), AvailableAP);
            }
            Controller->EnterSkillMode(Skill);
            TestEqual(Context + TEXT(" selection matches affordability"), Controller->IsSkillInputMode(), bExpectedUsable);
            Controller->CancelTileInputMode();
            const float HPBefore = Target->GetAttributeSet()->GetHP();
            const int32 CompletionsBefore = Completions;
            Unit->StartSkill(Skill, TargetTile);
            TestEqual(Context + TEXT(" completes once"), Completions, CompletionsBefore + 1);
            TestFalse(Context + TEXT(" releases busy state"), Unit->IsBusy());
            TestNull(Context + TEXT(" clears skill context"), Unit->PendingSkillData.Get());
            if (bExpectedUsable)
            {
                TestEqual(Context + TEXT(" exact definition charge"), Unit->GetCurrentActionPoint(), AvailableAP - Cost);
                TestEqual(Context + TEXT(" real damage"), Target->GetAttributeSet()->GetHP(), HPBefore - 10.0f);
                TestEqual(Context + TEXT(" success result"), LastResult, EUnitActionResult::Succeeded);
            }
            else
            {
                TestEqual(Context + TEXT(" no AP charge"), Unit->GetCurrentActionPoint(), AvailableAP);
                TestEqual(Context + TEXT(" no damage"), Target->GetAttributeSet()->GetHP(), HPBefore);
                TestEqual(Context + TEXT(" failed result"), LastResult, EUnitActionResult::Failed);
            }
        }
    }
    Unit->OnActionCompleted.Clear();
    Combat->ResetCombat();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCostFailureTest, "ProjectA.Combat.Costs.ActivationFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillCostFailureTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(500.0f, 0.0f, 100.0f));
    Scope.SpawnTile(Unit);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Target);
    USkillDefinitionDataAsset* Skill = MakeSkill(Unit, UGA_DefaultAttack::StaticClass());
    Skill->ActionPointCost = 2;
    int32 Completions = 0;
    EUnitActionResult LastResult = EUnitActionResult::Succeeded;
    Unit->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        LastResult = Result;
    });
    const auto CheckFailure = [&](const FString& Context, int32 ExpectedCompletions)
    {
        TestEqual(Context + TEXT(" completes once"), Completions, ExpectedCompletions);
        TestEqual(Context + TEXT(" result"), LastResult, EUnitActionResult::Failed);
        TestEqual(Context + TEXT(" preserves AP"), Unit->GetCurrentActionPoint(), 2);
        TestEqual(Context + TEXT(" preserves movement AP"), Unit->GetCurrentSubActionPoint(), 1);
        TestEqual(Context + TEXT(" no damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
        TestFalse(Context + TEXT(" not busy"), Unit->IsBusy());
        TestFalse(Context + TEXT(" does not request turn end"), Unit->MustEndTurnAfterCurrentAction());
        TestNull(Context + TEXT(" cleared context"), Unit->PendingSkillData.Get());
    };

    Unit->StartSkill(Skill, TargetTile);
    CheckFailure(TEXT("Missing ability"), 1);
    UGameplayAbility* Ability = GrantAttack(Unit);
    FGameplayTagContainer* BlockedTags = FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), TEXT("ActivationBlockedTags"))->ContainerPtrToValuePtr<FGameplayTagContainer>(Ability);
    const FGameplayTag BlockTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Release"));
    BlockedTags->AddTag(BlockTag);
    Unit->GetAbilitySystemComponent()->AddLooseGameplayTag(BlockTag);
    Unit->StartSkill(Skill, TargetTile);
    CheckFailure(TEXT("GAS activation blocked"), 2);
    Unit->GetAbilitySystemComponent()->RemoveLooseGameplayTag(BlockTag);
    BlockedTags->Reset();

    ACombatGridTile* EmptyTile = Scope.World->SpawnActor<ACombatGridTile>();
    Skill->TargetRule = ESkillTargetRule::AnyTile;
    Unit->StartSkill(Skill, EmptyTile);
    CheckFailure(TEXT("Attack context invalid"), 3);

    Skill->TargetRule = ESkillTargetRule::EnemyUnit;
    Unit->StartSkill(Skill, TargetTile);
    TestEqual(TEXT("Valid retry completes once"), Completions, 4);
    TestEqual(TEXT("Valid retry succeeds"), LastResult, EUnitActionResult::Succeeded);
    TestEqual(TEXT("Valid retry charges the definition cost once"), Unit->GetCurrentActionPoint(), 0);
    TestEqual(TEXT("Valid retry applies damage once"), Target->GetAttributeSet()->GetHP(), 90.0f);
    Unit->OnActionCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyAffordableSkillTest, "ProjectA.Combat.Costs.EnemyAffordableSkill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyAffordableSkillTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector(0.0f, 0.0f, 100.0f));
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(50.0f, 0.0f, 100.0f));
    Enemy->SetTeam(ETeam::Enemy);
    Target->SetTeam(ETeam::Player);
    Scope.SpawnTile(Enemy);
    Scope.SpawnTile(Target);
    FindFProperty<FClassProperty>(AUnitBase::StaticClass(), TEXT("DefaultAttackAbilityClass"))->SetObjectPropertyValue_InContainer(Enemy, UGA_DefaultAttack::StaticClass());
    FArrayProperty* Classes = FindFProperty<FArrayProperty>(AUnitBase::StaticClass(), TEXT("EquippedSkillAbilityClasses"));
    FScriptArrayHelper ClassHelper(Classes, Classes->ContainerPtrToValuePtr<void>(Enemy));
    const int32 Index = ClassHelper.AddValue();
    CastFieldChecked<FClassProperty>(Classes->Inner)->SetObjectPropertyValue(ClassHelper.GetRawPtr(Index), UGA_AreaAttack::StaticClass());
    USkillDefinitionDataAsset* Expensive = MakeSkill(Enemy, UGA_DefaultAttack::StaticClass());
    USkillDefinitionDataAsset* Affordable = MakeSkill(Enemy, UGA_AreaAttack::StaticClass());
    EquipSkill(Enemy, Expensive);
    EquipSkill(Enemy, Affordable);
    Enemy->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UGA_AreaAttack::StaticClass(), 1));
    FindFProperty<FFloatProperty>(AEnemyUnit::StaticClass(), TEXT("SkillBaseScore"))->SetPropertyValue_InContainer(Enemy, 1000.0f);
    int32 Completions = 0;
    Enemy->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        TestEqual(TEXT("Affordable alternative succeeds"), Result, EUnitActionResult::Succeeded);
    });
    for (int32 RejectedCost : { 3, 0, -1 })
    {
        Expensive->ActionPointCost = RejectedCost;
        const int32 Before = Completions;
        Enemy->OnTurnStart();
        TestEqual(TEXT("AI skips unaffordable or invalid high priority attack"), Completions, Before + 1);
        TestEqual(TEXT("AI pays for affordable alternative"), Enemy->GetCurrentActionPoint(), 1);
        Enemy->OnTurnEnd();
    }
    Enemy->OnActionCompleted.Clear();
    return true;
}

#endif
