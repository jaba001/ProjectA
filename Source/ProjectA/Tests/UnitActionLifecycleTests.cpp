#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/SkillActor/AttackSkillActorBase.h"
#include "Grid/Combat/CombatGridManager.h"
#include "EngineUtils.h"
#include "Misc/DataValidation.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "GAS/Ability/GA_DefaultAttack.h"
#include "GAS/Ability/GA_AreaAttack.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Unit/UnitBase.h"
#include "UObject/UnrealType.h"

namespace UnitActionLifecycleTests
{
    struct FScopedWorld
    {
        UWorld* World = nullptr;
        uint64 TimerFrame = 0;

        // Advance isolated timer frames without changing the surrounding editor frame.
        // 외부 에디터 프레임을 바꾸지 않고 격리된 타이머 프레임을 진행합니다.
        void TickTimers(float DeltaSeconds)
        {
            TimerFrame = FMath::Max(TimerFrame, GFrameCounter) + 1;
            TGuardValue<uint64> FrameGuard(GFrameCounter, TimerFrame);
            World->GetTimerManager().Tick(DeltaSeconds);
        }

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
            if (Unit->IsA<AEnemyUnit>())
            {
                Unit->SetTeam(ETeam::Enemy);
            }
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
    Target->SetTeam(ETeam::Enemy);
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
    Target->SetTeam(ETeam::Enemy);
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
    Target->SetTeam(ETeam::Enemy);
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
    EmptyTile->SetTerritory(ETileTerritory::Enemy);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillTargetRuleTest, "ProjectA.Combat.Targeting.RulesAndPlayerSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillTargetRuleTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    for (ETeam SourceTeam : { ETeam::Player, ETeam::Enemy })
    {
        FScopedWorld Scope;
        AUnitBase* Source = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
        AUnitBase* Ally = Scope.SpawnUnit<AUnitBase>(FVector(100.0f, 0.0f, 0.0f));
        AUnitBase* Opponent = Scope.SpawnUnit<AUnitBase>(FVector(200.0f, 0.0f, 0.0f));
        Source->SetTeam(SourceTeam);
        Ally->SetTeam(SourceTeam);
        ETeam OpposingTeam = ETeam::Enemy;
        ETileTerritory AllyTerritory = ETileTerritory::Player;
        ETileTerritory EnemyTerritory = ETileTerritory::Enemy;
        if (SourceTeam == ETeam::Enemy)
        {
            OpposingTeam = ETeam::Player;
            AllyTerritory = ETileTerritory::Enemy;
            EnemyTerritory = ETileTerritory::Player;
        }
        Opponent->SetTeam(OpposingTeam);
        TArray<ACombatGridTile*> Tiles = { Scope.SpawnTile(Source), Scope.SpawnTile(Ally), Scope.SpawnTile(Opponent), Scope.World->SpawnActor<ACombatGridTile>(), Scope.World->SpawnActor<ACombatGridTile>(), Scope.World->SpawnActor<ACombatGridTile>() };
        Tiles[0]->SetTerritory(AllyTerritory);
        Tiles[1]->SetTerritory(AllyTerritory);
        Tiles[2]->SetTerritory(EnemyTerritory);
        Tiles[3]->SetTerritory(AllyTerritory);
        Tiles[4]->SetTerritory(EnemyTerritory);
        ACombatManager* Combat = Scope.World->SpawnActor<ACombatManager>();
        Combat->RegisterUnits({ Source, Opponent });
        Combat->StartCombat_Internal();
        APartyPlayerController* Controller = Scope.World->SpawnActor<APartyPlayerController>();
        Controller->SetCombatContext(Combat, true);
        USkillDefinitionDataAsset* Skill = MakeSkill(Source, UGA_DefaultAttack::StaticClass());
        // Exercise target filtering for both teams without bypassing the separate turn-input guard.
        // 턴 입력 권한 검사를 변경하지 않고 양 진영의 대상 필터를 검증합니다.
        FindFProperty<FObjectPropertyBase>(APartyPlayerController::StaticClass(), TEXT("PendingSkillData"))->SetObjectPropertyValue_InContainer(Controller, Skill);
        const bool Expected[6][6] =
        {
            { false, false, true, false, false, false },
            { true, true, false, false, false, false },
            { true, true, true, false, false, false },
            { false, false, true, false, true, false },
            { true, true, false, true, false, false },
            { true, true, true, true, true, false }
        };
        for (int32 RuleIndex = 0; RuleIndex < 6; ++RuleIndex)
        {
            Skill->TargetRule = static_cast<ESkillTargetRule>(RuleIndex);
            for (bool bProtected : { false, true })
            {
                Tiles[2]->SetProtectedByFront(bProtected);
                for (bool bIgnoreFront : { false, true })
                {
                    Skill->bIgnoreFront = bIgnoreFront;
                    for (bool bApproach : { false, true })
                    {
                        Skill->bMoveToTarget = bApproach;
                        for (int32 TileIndex = 0; TileIndex < Tiles.Num(); ++TileIndex)
                        {
                            bool bAllowed = Expected[RuleIndex][TileIndex];
                            if (RuleIndex == 0 && TileIndex == 2 && bProtected && !bIgnoreFront)
                            {
                                bAllowed = false;
                            }
                            if (bApproach && (TileIndex == 0 || TileIndex >= 3))
                            {
                                bAllowed = false;
                            }
                            const FString Context = FString::Printf(TEXT("Team=%d Rule=%d Tile=%d Protected=%d Ignore=%d Approach=%d"), static_cast<int32>(SourceTeam), RuleIndex, TileIndex, bProtected, bIgnoreFront, bApproach);
                            TestEqual(Context + TEXT(" shared rule"), UCombatTargetingLibrary::IsValidSkillTarget(Source, Skill, Tiles[TileIndex]), bAllowed);
                            TestEqual(Context + TEXT(" player filter"), Controller->IsValidTileForPendingSkill(Tiles[TileIndex]), bAllowed);
                        }
                    }
                }
            }
        }
        Skill->TargetRule = ESkillTargetRule::AnyUnit;
        Skill->bMoveToTarget = false;
        FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Opponent, true);
        TestFalse(TEXT("Dead occupant is rejected"), Controller->IsValidTileForPendingSkill(Tiles[2]));
        FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Opponent, false);
        Tiles[4]->SetOccupyingUnit(Opponent);
        TestFalse(TEXT("Stale tile occupancy is rejected"), Controller->IsValidTileForPendingSkill(Tiles[4]));
        Skill->TargetRule = static_cast<ESkillTargetRule>(255);
        TestFalse(TEXT("Unknown target rule is rejected"), Controller->IsValidTileForPendingSkill(Tiles[2]));
        TestFalse(TEXT("Null source is rejected"), UCombatTargetingLibrary::IsValidSkillTarget(nullptr, Skill, Tiles[0]));
        TestFalse(TEXT("Null skill is rejected"), UCombatTargetingLibrary::IsValidSkillTarget(Source, nullptr, Tiles[0]));
        TestFalse(TEXT("Null tile is rejected"), UCombatTargetingLibrary::IsValidSkillTarget(Source, Skill, nullptr));
        Combat->ResetCombat();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyTargetRuleTest, "ProjectA.Combat.Targeting.EnemySelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyTargetRuleTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector::ZeroVector);
    AUnitBase* Protected = Scope.SpawnUnit<AUnitBase>(FVector(10.0f, 0.0f, 0.0f));
    AUnitBase* Exposed = Scope.SpawnUnit<AUnitBase>(FVector(100.0f, 0.0f, 0.0f));
    ACombatGridTile* SelfTile = Scope.SpawnTile(Enemy);
    ACombatGridTile* ProtectedTile = Scope.SpawnTile(Protected);
    ACombatGridTile* ExposedTile = Scope.SpawnTile(Exposed);
    ACombatGridTile* EmptyTile = Scope.World->SpawnActor<ACombatGridTile>();
    SelfTile->SetTerritory(ETileTerritory::Enemy);
    EmptyTile->SetTerritory(ETileTerritory::Player);
    ProtectedTile->SetProtectedByFront(true);
    USkillDefinitionDataAsset* Skill = MakeSkill(Enemy, UGameplayAbility::StaticClass());
    EquipSkill(Enemy, Skill);
    FindFProperty<FClassProperty>(AUnitBase::StaticClass(), TEXT("DefaultAttackAbilityClass"))->SetObjectPropertyValue_InContainer(Enemy, UGameplayAbility::StaticClass());
    Enemy->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UGameplayAbility::StaticClass(), 1));
    const auto CheckSelection = [&](ESkillTargetRule Rule, bool bIgnoreFront, ACombatGridTile* ExpectedTile)
    {
        Skill->TargetRule = Rule;
        Skill->bIgnoreFront = bIgnoreFront;
        Enemy->OnTurnStart();
        TestTrue(TEXT("AI begins the selected action"), Enemy->IsBusy());
        TestEqual(TEXT("AI chooses a rule-valid tile"), Enemy->PendingSkillTargetTile, ExpectedTile);
        TestTrue(TEXT("AI choice passes shared validation"), UCombatTargetingLibrary::IsValidSkillTarget(Enemy, Skill, Enemy->PendingSkillTargetTile));
        Enemy->CancelCurrentAction();
        Enemy->OnTurnEnd();
    };
    CheckSelection(ESkillTargetRule::EnemyUnit, false, ExposedTile);
    CheckSelection(ESkillTargetRule::EnemyUnit, true, ProtectedTile);
    CheckSelection(ESkillTargetRule::AllyUnit, false, SelfTile);
    CheckSelection(ESkillTargetRule::AnyUnit, false, SelfTile);
    CheckSelection(ESkillTargetRule::EnemyTile, false, EmptyTile);
    CheckSelection(ESkillTargetRule::AllyTile, false, SelfTile);
    ProtectedTile->SetProtectedByFront(false);
    CheckSelection(ESkillTargetRule::EnemyUnit, false, ProtectedTile);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillTargetExecutionTest, "ProjectA.Combat.Targeting.ExecutionRevalidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillTargetExecutionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Source = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(100.0f, 0.0f, 0.0f));
    Target->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Source);
    ACombatGridTile* Tile = Scope.SpawnTile(Target);
    GrantAttack(Source);
    USkillDefinitionDataAsset* Skill = MakeSkill(Source, UGA_DefaultAttack::StaticClass());
    int32 Completions = 0;
    EUnitActionResult LastResult = EUnitActionResult::Succeeded;
    Source->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completions;
        LastResult = Result;
    });
    Tile->SetProtectedByFront(true);
    Source->StartSkill(Skill, Tile);
    TestEqual(TEXT("Direct protected-target request fails"), LastResult, EUnitActionResult::Failed);
    TestEqual(TEXT("Protected-target rejection preserves AP"), Source->GetCurrentActionPoint(), 2);
    TestEqual(TEXT("Protected-target rejection causes no damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
    Tile->SetProtectedByFront(false);
    Target->SetTeam(ETeam::Player);
    Source->StartSkill(Skill, Tile);
    TestEqual(TEXT("Direct friendly-target request fails"), LastResult, EUnitActionResult::Failed);
    TestEqual(TEXT("Direct rejections complete exactly once each"), Completions, 2);
    Target->SetTeam(ETeam::Enemy);

    // Inject the waiting phase to model changes during approach without depending on navigation.
    // 내비게이션에 의존하지 않고 접근 중 상태 변경을 재현하도록 실행 대기 단계를 주입합니다.
    for (int32 Scenario = 0; Scenario < 4; ++Scenario)
    {
        FEnumProperty* ActionProperty = FindFProperty<FEnumProperty>(AUnitBase::StaticClass(), TEXT("CurrentActionType"));
        ActionProperty->GetUnderlyingProperty()->SetIntPropertyValue(ActionProperty->ContainerPtrToValuePtr<void>(Source), static_cast<uint64>(EUnitActionType::Skill));
        SetMovementPhase(Source, EUnitMovePhase::WaitingForSkill);
        Source->PendingSkillData = Skill;
        Source->PendingSkillAbilityClass = Skill->AbilityClass;
        Source->PendingSkillTargetTile = Tile;
        Source->PendingTargetUnit = Target;
        if (Scenario == 0)
        {
            Tile->SetProtectedByFront(true);
        }
        else if (Scenario == 1)
        {
            Target->SetTeam(ETeam::Player);
        }
        else if (Scenario == 2)
        {
            FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Target, true);
        }
        else
        {
            ACombatGridTile* OtherTile = Scope.World->SpawnActor<ACombatGridTile>();
            Target->SetCurrentTile(OtherTile);
            AUnitBase* Replacement = Scope.SpawnUnit<AUnitBase>(FVector(100.0f, 0.0f, 0.0f));
            Replacement->SetTeam(ETeam::Enemy);
            Replacement->SetCurrentTile(Tile);
        }
        Source->ExecuteSkillAtTarget();
        TestEqual(TEXT("Changed target completes once"), Completions, Scenario + 3);
        TestEqual(TEXT("Changed target reports failure"), LastResult, EUnitActionResult::Failed);
        TestFalse(TEXT("Changed target releases busy state"), Source->IsBusy());
        TestEqual(TEXT("Changed target preserves AP"), Source->GetCurrentActionPoint(), 2);
        TestEqual(TEXT("Changed target causes no damage"), Target->GetAttributeSet()->GetHP(), 100.0f);
        Tile->SetProtectedByFront(false);
        Target->SetTeam(ETeam::Enemy);
        FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Target, false);
    }
    Target->SetCurrentTile(Tile);
    Source->StartSkill(Skill, Tile);
    TestEqual(TEXT("Valid retry succeeds"), LastResult, EUnitActionResult::Succeeded);
    TestEqual(TEXT("Valid retry charges AP once"), Source->GetCurrentActionPoint(), 1);
    TestEqual(TEXT("Valid retry applies real damage"), Target->GetAttributeSet()->GetHP(), 90.0f);
    Source->OnActionCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerEnemyInputIsolationTest, "ProjectA.Combat.Input.PlayerAndEnemyTurnIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerEnemyInputIsolationTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Player = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    AEnemyUnit* Enemy = Scope.SpawnUnit<AEnemyUnit>(FVector(100.0f, 0.0f, 0.0f));
    ACombatGridTile* PlayerTile = Scope.SpawnTile(Player);
    ACombatGridTile* EnemyTile = Scope.SpawnTile(Enemy);
    USkillDefinitionDataAsset* EnemySkill = MakeSkill(Enemy, UGameplayAbility::StaticClass());
    EquipSkill(Enemy, EnemySkill);
    Enemy->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UGameplayAbility::StaticClass(), 1));
    FindFProperty<FClassProperty>(AUnitBase::StaticClass(), TEXT("DefaultAttackAbilityClass"))->SetObjectPropertyValue_InContainer(Enemy, UGameplayAbility::StaticClass());
    ACombatManager* Combat = Scope.World->SpawnActor<ACombatManager>();
    Combat->RegisterUnits({ Player, Enemy });
    Combat->StartCombat_Internal();
    APartyPlayerController* Controller = Scope.World->SpawnActor<APartyPlayerController>();
    Scope.World->AddController(Controller);
    TestEqual(TEXT("Tile input resolves the registered player controller"), Scope.World->GetFirstPlayerController(), static_cast<APlayerController*>(Controller));
    Controller->SetCombatContext(Combat, true);
    USkillDefinitionDataAsset* PlayerSkill = MakeSkill(Player, UGA_DefaultAttack::StaticClass());
    Controller->EnterSkillMode(PlayerSkill);
    TestTrue(TEXT("Player can select a skill on its turn"), Controller->IsSkillInputMode());
    TestTrue(TEXT("Internal transition accepts the active player"), Combat->RequestEndTurnForUnit(Player));
    TestFalse(TEXT("Turn transition clears pending input"), Controller->IsSkillInputMode());
    TestNull(TEXT("Turn transition clears pending skill"), Controller->GetPendingSkillData());
    TestTrue(TEXT("Enemy starts its own held ability"), Enemy->IsBusy());
    Enemy->CancelCurrentAction();
    TestTrue(TEXT("Enemy stays active until its scheduled continuation"), Enemy->IsActiveTurn());
    TestFalse(TEXT("Enemy continuation gap is not busy"), Enemy->IsBusy());
    const int32 TurnBefore = Combat->GetTurnManager()->GetTurnCounter();
    const int32 APBefore = Enemy->GetCurrentActionPoint();
    const int32 MoveAPBefore = Enemy->GetCurrentSubActionPoint();
    const FVector PositionBefore = Enemy->GetActorLocation();
    Controller->EnterMoveMode();
    TestFalse(TEXT("Player cannot select enemy movement"), Controller->IsMoveInputMode());
    Controller->EnterSkillMode(PlayerSkill);
    TestFalse(TEXT("Player cannot select an enemy skill"), Controller->IsSkillInputMode());
    // Even a stale externally set mode cannot bypass the player command guard.
    // 외부에서 남긴 입력 모드도 플레이어 명령 검사를 우회할 수 없습니다.
    Controller->SetTileInputMode(ETileInputMode::Move);
    PlayerTile->NotifyActorOnClicked(EKeys::LeftMouseButton);
    Controller->HandleTileClicked(EnemyTile);
    Controller->RequestEndTurn();
    Combat->RequestEndTurn();
    TestEqual(TEXT("Player cannot end the idle enemy turn"), Combat->GetTurnManager()->GetTurnCounter(), TurnBefore);
    TestEqual(TEXT("Enemy AP is unchanged by player input"), Enemy->GetCurrentActionPoint(), APBefore);
    TestEqual(TEXT("Enemy movement AP is unchanged"), Enemy->GetCurrentSubActionPoint(), MoveAPBefore);
    TestTrue(TEXT("Player input does not move the enemy"), Enemy->GetActorLocation().Equals(PositionBefore));
    TestEqual(TEXT("Player input does not deal damage"), Player->GetAttributeSet()->GetHP(), 100.0f);
    TestNull(TEXT("Enemy-turn clicks do not select a tile"), Controller->GetSelectedTile());
    // Disabling UI must not disable the enemy's internal completion path.
    // UI를 잠가도 적의 내부 완료 경로는 막히지 않아야 합니다.
    Controller->SetCombatContext(Combat, false);
    Scope.World->GetTimerManager().Tick(0.01f);
    TestEqual(TEXT("Actual AI completion returns the turn to the player"), Combat->GetCurrentUnit(), Player);
    TestEqual(TEXT("AI advances exactly one turn"), Combat->GetTurnManager()->GetTurnCounter(), TurnBefore + 1);
    TestEqual(TEXT("Previous input mode is cleared"), Controller->GetTileInputMode(), ETileInputMode::None);
    Controller->RequestEndTurn();
    Combat->RequestEndTurn();
    TestEqual(TEXT("Disabled UI cannot end even a player turn"), Combat->GetCurrentUnit(), Player);
    TestFalse(TEXT("Stale enemy completion cannot end the player's turn"), Combat->RequestEndTurnForUnit(Enemy));
    Controller->SetCombatContext(Combat, true);
    TestTrue(TEXT("Player input resumes when enabled"), Controller->CanUseActiveUnitAction());
    Combat->ResetCombat();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerCommandGuardTest, "ProjectA.Combat.Input.TileCommandsAndTurnGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerCommandGuardTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Player = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    AUnitBase* Enemy = Scope.SpawnUnit<AUnitBase>(FVector(100.0f, 0.0f, 0.0f));
    Enemy->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Player);
    ACombatGridTile* TargetTile = Scope.SpawnTile(Enemy);
    GrantAttack(Player);
    ACombatManager* Combat = Scope.World->SpawnActor<ACombatManager>();
    Combat->RegisterUnits({ Player, Enemy });
    Combat->StartCombat_Internal();
    APartyPlayerController* Controller = Scope.World->SpawnActor<APartyPlayerController>();
    Scope.World->AddController(Controller);
    TestEqual(TEXT("Tile input resolves the registered player controller"), Scope.World->GetFirstPlayerController(), static_cast<APlayerController*>(Controller));
    Controller->SetCombatContext(Combat, true);
    USkillDefinitionDataAsset* Skill = MakeSkill(Player, UGA_DefaultAttack::StaticClass());
    TestFalse(TEXT("Null turn requester is rejected"), Combat->RequestEndTurnForUnit(nullptr));
    TestFalse(TEXT("Inactive unit cannot end another turn"), Combat->RequestEndTurnForUnit(Enemy));
    Player->bIsActiveTurn = false;
    TestFalse(TEXT("Inactive flag prevents turn completion"), Combat->RequestEndTurnForUnit(Player));
    Player->bIsActiveTurn = true;
    SetMovementPhase(Player, EUnitMovePhase::WaitingForSkill);
    Controller->RequestEndTurn();
    Combat->RequestEndTurn();
    TestFalse(TEXT("Busy unit cannot end its turn internally"), Combat->RequestEndTurnForUnit(Player));
    TestEqual(TEXT("Busy end-turn requests do not advance"), Combat->GetCurrentUnit(), Player);
    SetMovementPhase(Player, EUnitMovePhase::None);
    FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Player, true);
    TestFalse(TEXT("Dead unit cannot request normal turn completion"), Combat->RequestEndTurnForUnit(Player));
    FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Player, false);

    Controller->EnterSkillMode(Skill);
    Player->ConsumeActionPoint(2);
    TargetTile->NotifyActorOnClicked(EKeys::LeftMouseButton);
    TestEqual(TEXT("Tile command rechecks AP after selection"), Enemy->GetAttributeSet()->GetHP(), 100.0f);
    TestTrue(TEXT("Rejected tile command retains the selected skill"), Controller->IsSkillInputMode());
    Player->OnTurnStart();
    TargetTile->NotifyActorOnClicked(EKeys::LeftMouseButton);
    TestEqual(TEXT("Tile delegates a valid attack to the controller"), Enemy->GetAttributeSet()->GetHP(), 90.0f);
    TestEqual(TEXT("Valid tile command charges AP once"), Player->GetCurrentActionPoint(), 1);
    TestEqual(TEXT("Valid tile command consumes selection"), Controller->GetTileInputMode(), ETileInputMode::None);
    Controller->EnterSkillMode(Skill);
    Combat->EndCombat();
    TestEqual(TEXT("Combat end clears input mode"), Controller->GetTileInputMode(), ETileInputMode::None);
    TestNull(TEXT("Combat end clears selected skill"), Controller->GetPendingSkillData());
    Controller->HandleTileClicked(TargetTile);
    Controller->RequestEndTurn();
    TestFalse(TEXT("Ended combat rejects unit completion"), Combat->RequestEndTurnForUnit(Player));
    TestEqual(TEXT("Ended combat rejects damage input"), Enemy->GetAttributeSet()->GetHP(), 90.0f);
    Combat->ResetCombat();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillAreaParityTest, "ProjectA.Combat.Area.DirectAndSpawnedParity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillAreaParityTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    ACombatGridManager* Grid = Scope.World->SpawnActor<ACombatGridManager>();
    const TArray<FIntPoint> Coords = { {0, 0}, {4, 0}, {5, 1}, {1, 1}, {-1, 0}, {8, 8}, {4, 1} };
    TArray<AUnitBase*> Units;
    TArray<ACombatGridTile*> Tiles;
    for (const FIntPoint& Coord : Coords)
    {
        AUnitBase* Unit = Scope.SpawnUnit<AUnitBase>(FVector(Coord.X * 200.0f, Coord.Y * 200.0f, 100.0f));
        ACombatGridTile* Tile = Scope.SpawnTile(Unit);
        Tile->GridCoord = Coord;
        Grid->TileMap.Add(Coord, Tile);
        Units.Add(Unit);
        Tiles.Add(Tile);
    }
    AUnitBase* Source = Units[0];
    UAbilitySystemComponent* ASC = Source->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UGA_AreaAttack::StaticClass(), 1));
    UGameplayAbility* Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
    FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("DamageEffectClass"))->SetObjectPropertyValue_InContainer(Ability, UGE_Damage::StaticClass());
    FClassProperty* SpawnClass = FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("SpawnedAttackActorClass"));
    USkillDefinitionDataAsset* Skill = MakeSkill(Source, UGA_AreaAttack::StaticClass());
    Skill->bIgnoreFront = true;
    FindFProperty<FBoolProperty>(AUnitBase::StaticClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(Units[6], true);
    for (ETeam Team : { ETeam::Player, ETeam::Enemy })
    {
        const ETeam Other = Team == ETeam::Player ? ETeam::Enemy : ETeam::Player;
        for (ESkillTargetRule Rule : { ESkillTargetRule::EnemyUnit, ESkillTargetRule::AllyUnit, ESkillTargetRule::AnyUnit, ESkillTargetRule::EnemyTile, ESkillTargetRule::AllyTile, ESkillTargetRule::AnyTile })
        {
            const bool bAllyRule = Rule == ESkillTargetRule::AllyUnit || Rule == ESkillTargetRule::AllyTile;
            const bool bEnemyRule = Rule == ESkillTargetRule::EnemyUnit || Rule == ESkillTargetRule::EnemyTile;
            Source->SetTeam(Team);
            Units[1]->SetTeam(bAllyRule ? Team : Other);
            Units[2]->SetTeam(Team);
            Units[3]->SetTeam(Other);
            Units[4]->SetTeam(Team);
            Units[5]->SetTeam(Other);
            Units[6]->SetTeam(Other);
            for (int32 Index = 0; Index < Units.Num(); ++Index)
            {
                Tiles[Index]->SetTerritory(Units[Index]->GetTeam() == ETeam::Player ? ETileTerritory::Player : ETileTerritory::Enemy);
            }
            Skill->TargetRule = Rule;
            for (ESkillAreaType Shape : { ESkillAreaType::Single, ESkillAreaType::AroundTarget, ESkillAreaType::AroundSelf })
            {
                Skill->AreaType = Shape;
                for (int32 Radius : { 0, 1 })
                {
                    Skill->AreaRadius = Radius;
                    // Explicit fixture coverage includes diagonal neighbors and excludes the caster/dead unit.
                    // 명시적 배치 기준은 대각선 이웃을 포함하고 시전자와 사망 유닛을 제외합니다.
                    TArray<int32> Covered;
                    if (Shape == ESkillAreaType::Single || Shape == ESkillAreaType::AroundTarget)
                    {
                        Covered.Add(1);
                        if (Shape == ESkillAreaType::AroundTarget && Radius == 1)
                        {
                            Covered.Add(2);
                        }
                    }
                    else if (Radius == 1)
                    {
                        Covered = { 3, 4 };
                    }
                    for (bool bSpawn : { false, true })
                    {
                        for (AUnitBase* Unit : Units)
                        {
                            Unit->GetAttributeSet()->InitHP(100.0f);
                        }
                        Source->OnTurnStart();
                        SpawnClass->SetObjectPropertyValue_InContainer(Ability, bSpawn ? AAttackSkillActorBase::StaticClass() : nullptr);
                        Source->StartSkill(Skill, Tiles[1]);
                        if (bSpawn)
                        {
                            int32 ActorCount = 0;
                            for (TActorIterator<AAttackSkillActorBase> It(Scope.World); It; ++It)
                            {
                                ++ActorCount;
                                It->RequestImpact();
                                It->RequestImpact();
                                It->Destroy();
                            }
                            TestEqual(TEXT("Ability spawns exactly one impact actor"), ActorCount, 1);
                        }
                        for (int32 Index = 0; Index < Units.Num(); ++Index)
                        {
                            const bool bSameTeam = Units[Index]->GetTeam() == Team;
                            const bool bAllowed = (!bAllyRule || bSameTeam) && (!bEnemyRule || !bSameTeam);
                            const float ExpectedHP = Covered.Contains(Index) && bAllowed ? 90.0f : 100.0f;
                            TestEqual(FString::Printf(TEXT("Team=%d Rule=%d Shape=%d Radius=%d Spawn=%d Unit=%d"), int32(Team), int32(Rule), int32(Shape), Radius, bSpawn, Index), Units[Index]->GetAttributeSet()->GetHP(), ExpectedHP);
                        }
                        TestEqual(TEXT("Area action consumes AP once"), Source->GetCurrentActionPoint(), 1);
                        TestFalse(TEXT("Area action completes"), Source->IsBusy());
                    }
                }
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillAreaRejectionTest, "ProjectA.Combat.Area.UnsupportedAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillAreaRejectionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    AUnitBase* Source = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(200.0f, 0.0f, 0.0f));
    Target->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Source);
    ACombatGridTile* Tile = Scope.SpawnTile(Target);
    GrantAttack(Source);
    USkillDefinitionDataAsset* Skill = MakeSkill(Source, UGA_DefaultAttack::StaticClass());
    for (ESkillAreaType Shape : { ESkillAreaType::Row, ESkillAreaType::Column, ESkillAreaType::LeftAndTarget, ESkillAreaType::RightAndTarget, ESkillAreaType::DiagonalTarget, ESkillAreaType::AllEnemies, static_cast<ESkillAreaType>(255) })
    {
        Skill->AreaType = Shape;
#if WITH_EDITOR
        FDataValidationContext ValidationContext;
        TestEqual(TEXT("Editor rejects unsupported area"), Skill->IsDataValid(ValidationContext), EDataValidationResult::Invalid);
#endif
        TestFalse(TEXT("Unsupported area is not selectable"), UCombatTargetingLibrary::IsValidSkillTarget(Source, Skill, Tile));
        Source->StartSkill(Skill, Tile);
        TestFalse(TEXT("Invalid area leaves unit idle"), Source->IsBusy());
        TestNull(TEXT("Invalid area clears context"), Source->PendingSkillData.Get());
        TestEqual(TEXT("Invalid area does not consume AP"), Source->GetCurrentActionPoint(), 2);
        FSkillActorInitData Init;
        Init.SourceUnit = Source;
        Init.SkillData = Skill;
        Init.TargetTile = Tile;
        AAttackSkillActorBase* Actor = Scope.World->SpawnActor<AAttackSkillActorBase>();
        Actor->InitializeAttackSkillActor(Init, UGE_Damage::StaticClass(), 10.0f);
        Actor->RequestImpact();
        TestEqual(TEXT("Invalid actor area does not fall back to splash"), Target->GetAttributeSet()->GetHP(), 100.0f);
        Actor->Destroy();
    }
    Skill->AreaType = ESkillAreaType::AroundTarget;
    Skill->AreaRadius = -1;
    Source->StartSkill(Skill, Tile);
    TestEqual(TEXT("Negative radius does not consume AP"), Source->GetCurrentActionPoint(), 2);
    Skill->AreaType = ESkillAreaType::Single;
    Skill->AreaRadius = 0;
    TestEqual(TEXT("Single resolution needs no grid manager"), UCombatTargetingLibrary::ResolveSkillAreaTargets(Source, Skill, Tile).Num(), 1);
    Source->StartSkill(Skill, Tile);
    TestEqual(TEXT("Valid retry deals damage"), Target->GetAttributeSet()->GetHP(), 90.0f);
    TestEqual(TEXT("Valid retry consumes AP"), Source->GetCurrentActionPoint(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpawnedAttackCompletionTest, "ProjectA.Combat.Completion.ProjectileBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSpawnedAttackCompletionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    FScopedWorld Scope;
    Scope.TickTimers(0.0f);
    AUnitBase* Source = Scope.SpawnUnit<AUnitBase>(FVector::ZeroVector);
    AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(200.0f, 0.0f, 0.0f));
    Target->SetTeam(ETeam::Enemy);
    Scope.SpawnTile(Source);
    ACombatGridTile* Tile = Scope.SpawnTile(Target);
    UGameplayAbility* Ability = GrantAttack(Source);
    FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("SpawnedAttackActorClass"))->SetObjectPropertyValue_InContainer(Ability, AAttackSkillActorBase::StaticClass());
    FindFProperty<FFloatProperty>(UGA_AttackBase::StaticClass(), TEXT("SpawnedActorTimeout"))->SetPropertyValue_InContainer(Ability, 0.2f);
    USkillDefinitionDataAsset* Skill = MakeSkill(Source, UGA_DefaultAttack::StaticClass());
    int32 Completed = 0;
    EUnitActionResult LastResult = EUnitActionResult::Succeeded;
    Source->OnActionCompleted.AddLambda([&](AUnitBase*, EUnitActionType, EUnitActionResult Result)
    {
        ++Completed;
        LastResult = Result;
    });
    for (int32 Scenario = 0; Scenario < 6; ++Scenario)
    {
        Source->OnTurnStart();
        const int32 Before = Completed;
        const float HPBefore = Target->GetAttributeSet()->GetHP();
        Source->StartSkill(Skill, Tile);
        AAttackSkillActorBase* Actor = nullptr;
        for (TActorIterator<AAttackSkillActorBase> It(Scope.World); It; ++It)
        {
            Actor = *It;
        }
        if (!TestNotNull(TEXT("Projectile exists"), Actor))
        {
            return false;
        }
        TestTrue(TEXT("No-montage release waits for projectile"), Source->IsBusy());
        TestEqual(TEXT("No early completion"), Completed, Before);
        TestEqual(TEXT("AP charged only once before impact"), Source->GetCurrentActionPoint(), 1);
        if (Scenario == 0)
        {
            // Inject an unfinished animation to exercise impact-before-montage ordering.
            // 미완료 애니메이션 상태를 주입해 몽타주 이전 임팩트 순서를 검증합니다.
            FindFProperty<FBoolProperty>(UGA_AttackBase::StaticClass(), TEXT("bAnimationFinished"))->SetPropertyValue_InContainer(Ability, false);
            Actor->RequestImpact();
            TestTrue(TEXT("Impact still waits for animation"), Source->IsBusy());
            Ability->ProcessEvent(Ability->FindFunctionChecked(TEXT("OnAttackMontageCompleted")), nullptr);
        }
        else if (Scenario == 1)
        {
            Actor->RequestFinish();
        }
        else if (Scenario == 2)
        {
            Actor->Destroy();
        }
        else if (Scenario == 3)
        {
            Scope.TickTimers(0.0f);
            Scope.TickTimers(0.3f);
        }
        else if (Scenario == 4)
        {
            Source->CancelCurrentAction();
        }
        else
        {
            Source->Die();
        }
        TestFalse(TEXT("Terminal path clears busy"), Source->IsBusy());
        TestEqual(TEXT("Terminal path completes exactly once"), Completed, Before + 1);
        const EUnitActionResult ExpectedResult = Scenario == 0 ? EUnitActionResult::Succeeded : (Scenario >= 4 ? EUnitActionResult::Cancelled : EUnitActionResult::Failed);
        TestEqual(TEXT("Terminal result is preserved"), LastResult, ExpectedResult);
        Actor->RequestImpact();
        Actor->RequestFinish();
        Actor->Destroy();
        TestEqual(TEXT("Late callbacks cannot complete again"), Completed, Before + 1);
        TestEqual(TEXT("Late or failed projectile cannot damage"), Target->GetAttributeSet()->GetHP(), HPBefore - (Scenario == 0 ? 10.0f : 0.0f));
        TestEqual(TEXT("Consumed AP is not refunded"), Source->GetCurrentActionPoint(), 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerExhaustionTest, "ProjectA.Combat.Completion.PlayerResourceExhaustion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerExhaustionTest::RunTest(const FString& Parameters)
{
    using namespace UnitActionLifecycleTests;
    for (int32 RemainingAP : { 0, 1 })
    {
        for (int32 RemainingSubAP : { 0, 1 })
        {
            FScopedWorld Scope;
            Scope.TickTimers(0.0f);
            APlayerUnit* Source = Scope.SpawnUnit<APlayerUnit>(FVector::ZeroVector);
            AUnitBase* Target = Scope.SpawnUnit<AUnitBase>(FVector(200.0f, 0.0f, 0.0f));
            Target->SetTeam(ETeam::Enemy);
            Scope.SpawnTile(Source);
            ACombatGridTile* Tile = Scope.SpawnTile(Target);
            UGameplayAbility* Ability = GrantAttack(Source);
            FindFProperty<FClassProperty>(UGA_AttackBase::StaticClass(), TEXT("SpawnedAttackActorClass"))->SetObjectPropertyValue_InContainer(Ability, AAttackSkillActorBase::StaticClass());
            USkillDefinitionDataAsset* Skill = MakeSkill(Source, UGA_DefaultAttack::StaticClass());
            Skill->ActionPointCost = 2 - RemainingAP;
            ACombatManager* Combat = Scope.World->SpawnActor<ACombatManager>();
            Combat->RegisterUnits({ Source, Target });
            Combat->StartCombat_Internal();
            if (RemainingSubAP == 0)
            {
                Source->ConsumeSubActionPoint(1);
            }
            Source->StartSkill(Skill, Tile);
            Scope.TickTimers(0.01f);
            TestEqual(TEXT("Exhaustion cannot end a projectile in flight"), Combat->GetCurrentUnit(), static_cast<AUnitBase*>(Source));
            for (TActorIterator<AAttackSkillActorBase> It(Scope.World); It; ++It)
            {
                It->RequestImpact();
            }
            TestEqual(TEXT("Completion notification precedes turn transition"), Combat->GetCurrentUnit(), static_cast<AUnitBase*>(Source));
            Scope.TickTimers(0.01f);
            AUnitBase* Expected = RemainingAP == 0 && RemainingSubAP == 0 ? Target : Source;
            TestEqual(TEXT("Only exhaustion of both pools ends turn"), Combat->GetCurrentUnit(), Expected);
            TestFalse(TEXT("Player action is complete"), Source->IsBusy());
            if (RemainingAP == 0 && RemainingSubAP == 1)
            {
                TestFalse(TEXT("Legacy end-turn flag preserves remaining movement"), Source->MustEndTurnAfterCurrentAction());
                Source->StartItemAction(Source);
                Scope.TickTimers(0.01f);
                TestEqual(TEXT("Last sub-action also triggers exhausted turn end"), Combat->GetCurrentUnit(), Target);
            }
            Combat->ResetCombat();
        }
    }
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
        Unit->bIsActiveTurn = false;
        Unit->ConfigureProfession(Definition.MaxHP, Definition.ActionPoints, Definition.SubActionPoints, Definition.StartingSkills);
        TestEqual(TEXT("Spawn HP equals preview"), Unit->GetAttributeSet()->GetHP(), Definition.MaxHP);
        TestEqual(TEXT("Spawn AP equals preview"), Unit->GetMaxActionPoint(), Definition.ActionPoints);
        TestEqual(TEXT("Spawn sub AP equals preview"), Unit->GetMaxSubActionPoint(), Definition.SubActionPoints);
        for (USkillDefinitionDataAsset* Skill : Definition.StartingSkills)
        {
            TestNotNull(TEXT("Every displayed skill is granted"), Unit->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Skill->AbilityClass));
            TestEqual(TEXT("Granted skill uses catalog definition"), Unit->FindSkillDataByAbilityClass(Skill->AbilityClass), Skill);
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
    Tuned->bIsActiveTurn = false;
    Tuned->ConfigureProfession(Resolved.MaxHP, Resolved.ActionPoints, Resolved.SubActionPoints, Resolved.StartingSkills);
    TestEqual(TEXT("Override applies HP"), Tuned->GetAttributeSet()->GetHP(), 137.0f);
    TestEqual(TEXT("Override applies AP"), Tuned->GetMaxActionPoint(), 3);
    TestTrue(TEXT("Override also changes preview"), Custom->GetProfessionDetails(TEXT("Scholar")).ToString().Contains(TEXT("HP 137")));
    const TObjectPtr<USkillDefinitionDataAsset> DuplicateSkill = Override.StartingSkills[0];
    Override.StartingSkills.Add(DuplicateSkill);
    TestFalse(TEXT("Duplicate skill ability is rejected"), Custom->ResolveProfession(TEXT("Scholar"), Resolved));
    Override.StartingSkills.Pop();
    Override.MaxHP = -1.0f;
    TestFalse(TEXT("Invalid tuning is rejected"), Custom->ResolveProfession(TEXT("Scholar"), Resolved));
    TestFalse(TEXT("Unknown professions do not silently use fallback"), Custom->ResolveProfession(TEXT("Warrior"), Resolved));
    return true;
}

#endif
