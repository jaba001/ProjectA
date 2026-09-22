#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/CombatManager.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Tests/RunRewardTestHelpers.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

namespace EncounterPreparationTests
{
    struct FFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        TStrongObjectPtr<UWorld> World;
        FString Slot = TEXT("PreparationRetry_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        AEncounterManager* Encounter = nullptr;
        ACombatManager* Combat = nullptr;

        FFixture()
        {
            const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
            if (!World.IsValid()) return;
            Encounter = World->SpawnActor<AEncounterManager>();
            Combat = World->SpawnActor<ACombatManager>();
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
        }

        ~FFixture()
        {
            Run->OnRunStateChanged.Clear();
            if (Encounter) Encounter->ShutdownGameplay();
            if (World.IsValid()) World->DestroyWorld(false);
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        bool Initialize(FText& OutError)
        {
            FRunPartyMember Member;
            Member.SlotIndex = 0;
            Member.CharacterName = FText::FromString(TEXT("Preparation Archer"));
            Member.ClassId = TEXT("Archer");
            Member.bCreated = true;
            return Encounter && Combat && Run->PartyDefinition && Run->InitializeRun({Member}, OutError) && Run->GetSaveError().IsEmpty();
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, Slot, 0);
            return Bytes;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterPreparationRetryTest, "ProjectA.Encounter.PreparationAbortRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterPreparationRetryTest::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("\\[Encounter\\]"), EAutomationExpectedErrorFlags::Contains, 2);
    for (const ERunPhase FailedPhase : {ERunPhase::Preparing, ERunPhase::Combat})
    {
        EncounterPreparationTests::FFixture Fixture;
        FText Error;
        if (!TestTrue(TEXT("The isolated preparation fixture initializes and saves"), Fixture.Initialize(Error))) return false;
        AEncounterManager* Encounter = Fixture.Encounter;
        Encounter->RunState = Fixture.Run.Get();
        Encounter->CombatManager = Fixture.Combat;
        Encounter->PartyDefinition = Fixture.Run->PartyDefinition;
        AGameplayPlayerController* Controller = Fixture.World->SpawnActor<AGameplayPlayerController>();
        if (!TestNotNull(TEXT("The local gameplay controller exists"), Controller)) return false;
        // Bind a transient local identity without SetPlayer's input, viewport and online initialization.
        // SetPlayer의 입력·뷰포트·온라인 초기화 없이 일시적인 로컬 플레이어 식별자만 연결합니다.
        ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
        Controller->Player = LocalPlayer;
        LocalPlayer->PlayerController = Controller;
        if (!TestTrue(TEXT("The fixture controller is local before exercising public Run requests"), Controller->IsLocalController())) return false;
        Controller->RunState = Fixture.Run.Get();
        Controller->InitializeGameplay(Encounter);
        const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
        if (FailedPhase == ERunPhase::Preparing)
        {
            FRunCheckpointStorage::FailNextWriteForTesting();
            TestFalse(TEXT("Missing arena setup fails through the public encounter entry point"), Encounter->RequestStartNode(TEXT("Combat_01")));
        }
        else
        {
            if (!TestTrue(TEXT("A late preparation failure starts from the combat phase"), Fixture.Run->BeginEncounter(TEXT("Combat_01")) && Fixture.Run->MarkCombatStarted())) return false;
            Controller->SetCombatContext(Fixture.Combat, true);
            AUnitBase* Unit = Fixture.World->SpawnActor<AUnitBase>();
            ACombatGridTile* Tile = Fixture.World->SpawnActor<ACombatGridTile>();
            ACombatArena* Arena = Fixture.World->SpawnActor<ACombatArena>();
            ACombatGridManager* Grid = Fixture.World->SpawnActor<ACombatGridManager>();
            if (!TestNotNull(TEXT("Partial preparation creates a unit"), Unit) || !TestNotNull(TEXT("Partial preparation creates a tile"), Tile) || !TestNotNull(TEXT("Partial preparation creates an arena"), Arena) || !TestNotNull(TEXT("Partial preparation creates a grid"), Grid)) return false;
            Grid->TileMap.Add(FIntPoint::ZeroValue, Tile);
            Arena->Grid = Grid;
            Encounter->Arena = Arena;
            Unit->SetCurrentTile(Tile);
            Encounter->SpawnedUnits.Add(Unit);
            Encounter->PartyActors.Add(0, Unit);
            FRunCheckpointStorage::FailNextWriteForTesting();
            TestFalse(TEXT("A failed combat start retains its preparation cancellation"), Encounter->FailPreparation(FText::FromString(TEXT("Combat start fixture failed."))));
            TestTrue(TEXT("Failed preparation destroys spawned actors and clears occupancy"), Unit->IsActorBeingDestroyed() && !Tile->GetOccupyingUnit());
        }
        const FText PreparationError = Encounter->PreparationFailureMessage;
        TestTrue(TEXT("Failed cancellation retains its exact phase and node"), Fixture.Run->GetPhase() == FailedPhase && Fixture.Run->GetCurrentNodeId() == TEXT("Combat_01") && Fixture.Run->GetCurrentEncounterId() == TEXT("DefaultEncounter"));
        TestTrue(TEXT("The previous durable checkpoint remains unchanged"), Fixture.ReadBytes() == BeforeBytes);
        TestTrue(TEXT("Cleaned preparation exposes the existing checkpoint retry action"), Encounter->GetSpawnedUnits().IsEmpty() && !Fixture.Combat->IsCombatActive() && !Encounter->bPreparing && Encounter->CanRetryCombatCheckpoint());
        TestFalse(TEXT("Flow refresh never re-enables input for a cleaned combat phase"), Controller->IsCombatInputEnabled());
        TestTrue(TEXT("The local controller exposes the pending recovery"), Controller->CanRetryGameplayRecovery());
        TestTrue(TEXT("The failure retains its preparation and persistence reasons"), !PreparationError.IsEmpty() && !Fixture.Run->GetSaveError().IsEmpty() && Encounter->GetFlowMessage().ToString().Contains(PreparationError.ToString()) && Encounter->GetFlowMessage().ToString().Contains(Fixture.Run->GetSaveError().ToString()));
        const FGameplayViewState CombinedView = FGameplayViewState::FromRun(Fixture.Run.Get(), Encounter->GetFlowMessage());
        TestTrue(TEXT("Local and replicated view data preserve both reasons without duplicate storage text"), CombinedView.FlowMessage.EqualTo(Encounter->GetFlowMessage()));
        const FGameplayViewState SeparateView = FGameplayViewState::FromRun(Fixture.Run.Get(), PreparationError);
        TestTrue(TEXT("A separate flow reason gains the storage explanation"), SeparateView.FlowMessage.EqualTo(CombinedView.FlowMessage));
        TestTrue(TEXT("An empty flow reason shows the storage error directly"), FGameplayViewState::FromRun(Fixture.Run.Get(), FText::GetEmpty()).FlowMessage.EqualTo(Fixture.Run->GetSaveError()));
        TestFalse(TEXT("Pending cancellation cannot start the same node again"), Encounter->RequestStartNode(TEXT("Combat_01")));
        Controller->SetRole(ROLE_SimulatedProxy);
        TestTrue(TEXT("The simulated client retains local ownership while server authority is absent"), Controller->IsLocalController() && !Controller->HasAuthority());
        Controller->RequestRetryCombatCheckpoint();
        TestFalse(TEXT("A client controller cannot expose the server checkpoint retry"), Controller->CanRetryGameplayRecovery());
        TestTrue(TEXT("A denied client retry preserves pending cancellation and disk"), Encounter->CanRetryCombatCheckpoint() && Fixture.Run->GetPhase() == FailedPhase && Fixture.ReadBytes() == BeforeBytes);
        Controller->SetRole(ROLE_Authority);

        int32 MapEvents = 0;
        bool bRetryVisibleDuringMapEvent = false;
        bool bAttemptedNestedStart = false;
        Fixture.Run->OnRunStateChanged.AddLambda([&]()
        {
            if (Fixture.Run->GetPhase() == ERunPhase::Map)
            {
                ++MapEvents;
                bRetryVisibleDuringMapEvent = Encounter->CanRetryCombatCheckpoint();
                if (!bAttemptedNestedStart)
                {
                    bAttemptedNestedStart = true;
                    Controller->RequestStartNode(TEXT("Combat_01"));
                    Controller->RequestRetryCombatCheckpoint();
                }
            }
        });
        FRunCheckpointStorage::FailNextWriteForTesting();
        Controller->RequestRetryCombatCheckpoint();
        TestTrue(TEXT("The public controller retry keeps repeated storage failure pending"), !Fixture.Run->GetSaveError().IsEmpty() && Controller->CanRetryGameplayRecovery() && Fixture.Run->GetPhase() == FailedPhase && Fixture.ReadBytes() == BeforeBytes);
        TestFalse(TEXT("Repeated failed recovery keeps combat input disabled"), Controller->IsCombatInputEnabled());
        TestEqual(TEXT("Failed cancellation never publishes a map transition"), MapEvents, 0);
        TestTrue(TEXT("The same retry action commits cancellation once storage recovers"), Encounter->RetryCombatCheckpoint(Error));
        TestTrue(TEXT("Successful cancellation clears persistence and retry state"), Error.IsEmpty() && Fixture.Run->GetSaveError().IsEmpty() && !Encounter->CanRetryCombatCheckpoint());
        TestTrue(TEXT("The map permits the original node without losing the setup error"), Fixture.Run->CanStartNode(TEXT("Combat_01")) && Fixture.Run->GetCurrentNodeId().IsNone() && Fixture.Run->GetCurrentEncounterId().IsNone() && Encounter->GetFlowMessage().EqualTo(PreparationError));
        TestEqual(TEXT("Successful cancellation publishes the map once"), MapEvents, 1);
        TestTrue(TEXT("The synchronous map observer attempted a public node request"), bAttemptedNestedStart);
        TestFalse(TEXT("The synchronous map observer sees cancellation already completed"), bRetryVisibleDuringMapEvent);
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
        TestTrue(TEXT("The committed cancellation persists a clean map"), Saved.IsValid() && Saved->Phase == ERunPhase::Map && Saved->CurrentNode.IsNone() && Saved->CurrentEncounter.IsNone());
        TestFalse(TEXT("A duplicate retry does not repeat cancellation"), Encounter->RetryCombatCheckpoint(Error));
        Controller->RequestRetryCombatCheckpoint();
        TestEqual(TEXT("Duplicate retries publish no extra transition"), MapEvents, 1);
        Fixture.Run->OnRunStateChanged.Clear();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterContinueRetryTest, "ProjectA.Encounter.ContinueSaveRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEncounterContinueRetryTest::RunTest(const FString& Parameters)
{
    EncounterPreparationTests::FFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("The isolated Continue fixture initializes and saves"), Fixture.Initialize(Error))) return false;
    AEncounterManager* Encounter = Fixture.Encounter;
    Encounter->RunState = Fixture.Run.Get();
    Encounter->CombatManager = Fixture.Combat;
    if (!TestTrue(TEXT("The Continue fixture enters combat before recording HP"), Fixture.Run->BeginEncounter(TEXT("Combat_01")) && Fixture.Run->MarkCombatStarted())) return false;
    Fixture.Run->UpdatePartyMemberHP(0, 100.f);
    if (!TestTrue(TEXT("The first victory commits a result checkpoint"), Fixture.Run->CompleteEncounter(ECombatResult::Victory))) return false;
    if (!TestTrue(TEXT("Continue retry starts after the reward is durable"), RunRewardTests::CollectPendingGoldRewards(Fixture.Run.Get()))) return false;
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Continue reports a failed save and remains retryable"), Encounter->ContinueRun());
    TestTrue(TEXT("Failed Continue preserves the result and its durable checkpoint"), Fixture.Run->GetPhase() == ERunPhase::Result && Fixture.ReadBytes() == BeforeBytes);
    TestTrue(TEXT("Failed Continue publishes its storage error"), !Fixture.Run->GetSaveError().IsEmpty() && Encounter->GetFlowMessage().EqualTo(Fixture.Run->GetSaveError()));

    int32 ChoiceEvents = 0;
    FText ObservedFlowMessage;
    Fixture.Run->OnRunStateChanged.AddLambda([&]()
    {
        if (Fixture.Run->GetPhase() == ERunPhase::EncounterChoice)
        {
            ++ChoiceEvents;
            ObservedFlowMessage = FGameplayViewState::FromRun(Fixture.Run.Get(), Encounter->GetFlowMessage()).FlowMessage;
        }
    });
    TestTrue(TEXT("Continue succeeds once storage recovers"), Encounter->ContinueRun());
    TestEqual(TEXT("Recovered Continue publishes exactly one encounter choice transition"), ChoiceEvents, 1);
    TestTrue(TEXT("The synchronous transition view does not retain the earlier save failure"), ObservedFlowMessage.IsEmpty());
    TestTrue(TEXT("Recovered Continue clears both storage and encounter messages"), Fixture.Run->GetSaveError().IsEmpty() && Encounter->GetFlowMessage().IsEmpty());
    TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    TestTrue(TEXT("Recovered Continue persists the encounter choice phase"), Saved.IsValid() && Saved->Phase == ERunPhase::EncounterChoice && Saved->CurrentEncounter.IsNone());
    TestFalse(TEXT("Repeated Continue cannot skip encounter selection"), Encounter->ContinueRun());
    TestEqual(TEXT("Repeated Continue publishes no extra transition"), ChoiceEvents, 1);
    Fixture.Run->OnRunStateChanged.Clear();
    return true;
}

#endif
