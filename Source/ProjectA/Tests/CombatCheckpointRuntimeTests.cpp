#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Turn/TurnManager.h"
#include "GameplayEffect.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace CombatCheckpointRuntimeTests
{
    struct FFixture
    {
        UWorld* World = nullptr;
        TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog;
        FProfessionDefinition Profession;

        FFixture()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
            Catalog.Reset(LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty")));
        }

        ~FFixture()
        {
            if (World)
            {
                World->DestroyWorld(false);
            }
        }

        bool Initialize()
        {
            return World && Catalog.IsValid() && Catalog->ResolveProfession(TEXT("Hunter"), Profession) && Profession.CombatClass && !Profession.StartingSkills.IsEmpty();
        }

        AUnitBase* Spawn(bool bConfigure = true)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnitBase* Unit = World->SpawnActor<AUnitBase>(Profession.CombatClass, FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Params);
            if (!Unit)
            {
                return nullptr;
            }
            UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
            ASC->InitAbilityActorInfo(Unit, Unit);
            ASC->AddAttributeSetSubobject(Unit->GetAttributeSet());
            Unit->GetAttributeSet()->InitMaxHP(100.0f);
            Unit->GetAttributeSet()->InitHP(100.0f);
            if (bConfigure && !Unit->ConfigureProfession(100.0f, 4, 2, Profession.StartingSkills))
            {
                return nullptr;
            }
            return Unit;
        }
    };

    bool SameUnitState(const FCombatCheckpointUnit& Left, const FCombatCheckpointUnit& Right)
    {
        return FCombatCheckpointUnit::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointDeadRuntimeTest, "ProjectA.Checkpoint.Runtime.DeadRestoreWithoutCallbacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointDeadRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace CombatCheckpointRuntimeTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Authored profession and skill paths resolve"), Fixture.Initialize()))
    {
        return false;
    }
    AUnitBase* Source = Fixture.Spawn();
    AUnitBase* Restored = Fixture.Spawn(false);
    if (!TestNotNull(TEXT("Source unit exists"), Source) || !TestNotNull(TEXT("Fresh restore unit exists"), Restored))
    {
        return false;
    }
    Source->RuntimeCharacterName = FText::FromString(TEXT("Fallen Hunter"));
    Source->HealingItemCount = 2;
    Source->HealingItemAmount = 31.0f;
    Source->ConsumeActionPoint(3);
    Source->ConsumeSubActionPoint(2);
    Source->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 0.0f);
    Source->Die();
    FCombatCheckpointUnit State;
    State.UnitId = FGuid::NewGuid();
    State.CharacterId = FGuid::NewGuid();
    State.OwnerAccountId.Provider = TEXT("Development");
    State.OwnerAccountId.Subject = TEXT("owner");
    State.PartySlot = 0;
    const FGuid UnitId = State.UnitId;
    FText Error;
    if (!TestTrue(TEXT("Dead inactive unit captures"), Source->CaptureCheckpointState(State, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestEqual(TEXT("Capture preserves persistent unit identity"), State.UnitId, UnitId);
    TestTrue(TEXT("Death is captured"), State.bDead);
    TestFalse(TEXT("Dead unit has no occupancy"), State.bHasTile);
    int32 DeathEvents = 0;
    int32 ActionEvents = 0;
    Restored->OnUnitDied.AddLambda([&DeathEvents](AUnitBase*) { ++DeathEvents; });
    Restored->OnActionCompleted.AddLambda([&ActionEvents](AUnitBase*, EUnitActionType, EUnitActionResult) { ++ActionEvents; });
    TestTrue(TEXT("Dead state restores to a fresh authored actor"), Restored->RestoreCheckpointState(State, Error));
    TestFalse(TEXT("Restored unit remains dead"), Restored->IsUnitAlive());
    TestEqual(TEXT("Restored HP remains zero"), Restored->GetAttributeSet()->GetHP(), 0.0f);
    TestEqual(TEXT("Death gameplay callback is not replayed"), DeathEvents, 0);
    TestEqual(TEXT("Action callback is not replayed"), ActionEvents, 0);
    TestFalse(TEXT("Restored dead unit is inactive"), Restored->IsActiveTurn());
    TestFalse(TEXT("Restored dead unit is idle"), Restored->IsBusy());
    TestEqual(TEXT("AP is restored without a turn reset"), Restored->GetCurrentActionPoint(), 1);
    TestEqual(TEXT("Item count is restored without using an item"), Restored->HealingItemCount, 2);
    FCombatCheckpointUnit CapturedAgain = State;
    TestTrue(TEXT("Restored dead state captures again"), Restored->CaptureCheckpointState(CapturedAgain, Error));
    TestTrue(TEXT("All represented unit values round trip"), SameUnitState(State, CapturedAgain));
    TestFalse(TEXT("The same actor cannot replay restoration"), Restored->RestoreCheckpointState(State, Error));
    Restored->OnUnitDied.Clear();
    Restored->OnActionCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointGASRuntimeTest, "ProjectA.Checkpoint.Runtime.UnsupportedGASRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointGASRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace CombatCheckpointRuntimeTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Authored fixture initializes"), Fixture.Initialize()))
    {
        return false;
    }
    AUnitBase* Unit = Fixture.Spawn();
    if (!TestNotNull(TEXT("Unit exists"), Unit))
    {
        return false;
    }
    FCombatCheckpointUnit State;
    FText Error;
    TestTrue(TEXT("Ordinary authored loadout captures"), Unit->CaptureCheckpointState(State, Error));
    const FCombatCheckpointUnit Original = State;
    const auto RejectCapture = [this, Unit, &State, &Original, &Error](const TCHAR* Label)
    {
        TestFalse(Label, Unit->CaptureCheckpointState(State, Error));
        TestFalse(FString(Label) + TEXT(" explains rejection"), Error.IsEmpty());
        TestTrue(FString(Label) + TEXT(" preserves output"), SameUnitState(State, Original));
    };
    Unit->bIsActiveTurn = true;
    RejectCapture(TEXT("Active turn is not an inactive checkpoint boundary"));
    Unit->bIsActiveTurn = false;

    UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
    UGameplayEffect* DurationEffect = NewObject<UGameplayEffect>(Unit);
    DurationEffect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationEffect->DurationMagnitude = FScalableFloat(30.0f);
    const FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectToSelf(DurationEffect, 1.0f, ASC->MakeEffectContext());
    TestTrue(TEXT("Actual duration effect becomes active"), EffectHandle.IsValid() && !ASC->GetActiveEffects(FGameplayEffectQuery()).IsEmpty());
    RejectCapture(TEXT("Active duration effect cannot be silently discarded"));
    ASC->RemoveActiveGameplayEffect(EffectHandle);
    TestTrue(TEXT("Capture resumes after duration effect removal"), Unit->CaptureCheckpointState(State, Error));

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Fixture.Profession.StartingSkills[0]->AbilityClass);
    if (!TestNotNull(TEXT("Authored ability spec is granted"), Spec))
    {
        return false;
    }
    Spec->ActiveCount = 1;
    RejectCapture(TEXT("Active GAS ability cannot be captured"));
    Spec->ActiveCount = 0;

    // Substitute a per-test ability object so the authored class default is never modified.
    // 작성된 클래스 기본값을 수정하지 않도록 테스트 전용 어빌리티 객체를 대입합니다.
    UGameplayAbility* OriginalAbility = Spec->Ability;
    UGameplayAbility* CooldownAbility = DuplicateObject<UGameplayAbility>(OriginalAbility, Unit);
    FClassProperty* CooldownProperty = FindFProperty<FClassProperty>(UGameplayAbility::StaticClass(), TEXT("CooldownGameplayEffectClass"));
    if (!TestNotNull(TEXT("Cooldown configuration is available"), CooldownProperty))
    {
        return false;
    }
    CooldownProperty->SetObjectPropertyValue_InContainer(CooldownAbility, UGameplayEffect::StaticClass());
    Spec->Ability = CooldownAbility;
    RejectCapture(TEXT("Configured cooldown is explicitly unsupported"));
    Spec->Ability = OriginalAbility;
    TestTrue(TEXT("Ordinary GAS state remains capturable after rejection"), Unit->CaptureCheckpointState(State, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointTurnRuntimeTest, "ProjectA.Checkpoint.Runtime.TurnGateRetryAndResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointTurnRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace CombatCheckpointRuntimeTests;
    FFixture Fixture;
    if (!TestTrue(TEXT("Authored fixture initializes"), Fixture.Initialize()))
    {
        return false;
    }
    AUnitBase* First = Fixture.Spawn();
    AUnitBase* Second = Fixture.Spawn();
    if (!TestNotNull(TEXT("First unit exists"), First) || !TestNotNull(TEXT("Second unit exists"), Second))
    {
        return false;
    }
    // A player implementation on the enemy team isolates turn gating from autonomous AI decisions.
    // 적 팀의 플레이어 구현으로 자동 AI 판단과 분리해 턴 경계 자체를 검증합니다.
    Second->SetTeam(ETeam::Enemy);
    First->ConsumeActionPoint(4);
    Second->ConsumeActionPoint(4);
    UTurnManager* Turns = NewObject<UTurnManager>(Fixture.World);
    bool bSaveSucceeds = false;
    int32 SaveCalls = 0;
    int32 LastCompletedSerial = INDEX_NONE;
    int32 LastNextIndex = INDEX_NONE;
    Turns->CommitTurnBoundary.BindLambda([&](int32 CompletedSerial, int32 NextIndex)
    {
        ++SaveCalls;
        LastCompletedSerial = CompletedSerial;
        LastNextIndex = NextIndex;
        TestFalse(TEXT("First unit is inactive during commit"), First->IsActiveTurn());
        TestFalse(TEXT("Second unit is inactive during commit"), Second->IsActiveTurn());
        return bSaveSucceeds;
    });
    Turns->InitializeTurnOrder({ First, Second });
    TestTrue(TEXT("Initial failed write blocks the first turn"), Turns->IsAwaitingTurnCheckpoint());
    TestFalse(TEXT("Blocked combat cannot accept actions"), Turns->IsCombatActive());
    TestEqual(TEXT("First turn has not incremented"), Turns->GetTurnCounter(), 0);
    TestEqual(TEXT("AP has not reset before commit"), First->GetCurrentActionPoint(), 0);
    Turns->EndTurn();
    Turns->NextTurn();
    Turns->StartTurn();
    TestEqual(TEXT("Turn commands cannot bypass the save gate"), SaveCalls, 1);
    TestFalse(TEXT("Another failed write stays blocked"), Turns->RetryTurnCheckpoint());
    TestEqual(TEXT("Failed retry keeps the same completed serial"), LastCompletedSerial, 0);
    TestEqual(TEXT("Failed retry keeps the same next index"), LastNextIndex, 0);
    bSaveSucceeds = true;
    TestTrue(TEXT("Successful retry activates the first turn"), Turns->RetryTurnCheckpoint());
    TestEqual(TEXT("Successful retry increments once"), Turns->GetTurnCounter(), 1);
    TestEqual(TEXT("Successful retry resets AP once"), First->GetCurrentActionPoint(), 4);
    First->ConsumeActionPoint(2);
    TestFalse(TEXT("Duplicate successful retry is rejected"), Turns->RetryTurnCheckpoint());
    Turns->StartTurn();
    TestEqual(TEXT("Duplicate start cannot replenish spent AP"), First->GetCurrentActionPoint(), 2);
    bSaveSucceeds = false;
    Turns->EndTurn();
    TestEqual(TEXT("Next gate saves the completed first turn"), LastCompletedSerial, 1);
    TestEqual(TEXT("Next gate identifies the second unit"), LastNextIndex, 1);
    TestEqual(TEXT("Second unit AP remains unchanged while blocked"), Second->GetCurrentActionPoint(), 0);
    bSaveSucceeds = true;
    TestTrue(TEXT("Second boundary retry succeeds"), Turns->RetryTurnCheckpoint());
    TestEqual(TEXT("Second turn starts exactly once"), Turns->GetTurnCounter(), 2);
    Turns->SuspendForRecovery();
    TestFalse(TEXT("Disconnect suspension is not a storage retry"), Turns->RetryTurnCheckpoint());
    TestEqual(TEXT("Suspension does not publish a combat result"), Turns->GetCombatResult(), ECombatResult::None);

    First->ConsumeActionPoint(2);
    Second->ConsumeActionPoint(4);
    UTurnManager* Resumed = NewObject<UTurnManager>(Fixture.World);
    int32 ResumeWrites = 0;
    Resumed->CommitTurnBoundary.BindLambda([&](int32 CompletedSerial, int32 NextIndex)
    {
        ++ResumeWrites;
        TestEqual(TEXT("Resumed boundary retains the restored serial"), CompletedSerial, 6);
        TestEqual(TEXT("Resumed boundary advances to the next unit"), NextIndex, 1);
        return false;
    });
    TestTrue(TEXT("Committed boundary resumes"), Resumed->RestoreFromBoundary({ First, Second }, 5, 0));
    TestEqual(TEXT("Restore skips rewriting the imported boundary"), ResumeWrites, 0);
    TestEqual(TEXT("Restore activates exactly the next turn"), Resumed->GetTurnCounter(), 6);
    TestEqual(TEXT("Resumed active unit receives AP once"), First->GetCurrentActionPoint(), 4);
    First->ConsumeActionPoint(1);
    TestFalse(TEXT("Active restore cannot run again"), Resumed->RestoreFromBoundary({ First, Second }, 5, 0));
    Resumed->StartTurn();
    TestEqual(TEXT("Repeated restore/start cannot replenish AP"), First->GetCurrentActionPoint(), 3);
    Resumed->EndTurn();
    TestEqual(TEXT("Normal saving resumes at the following boundary"), ResumeWrites, 1);
    TestTrue(TEXT("Following failed write remains blocked"), Resumed->IsAwaitingTurnCheckpoint());
    Resumed->SuspendForRecovery();
    Turns->CommitTurnBoundary.Unbind();
    Resumed->CommitTurnBoundary.Unbind();
    return true;
}

#endif
