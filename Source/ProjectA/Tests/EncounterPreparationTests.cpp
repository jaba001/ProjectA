#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/CombatManager.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
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
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
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
            Member.CharacterName = FText::FromString(TEXT("Preparation Hunter"));
            Member.ClassId = TEXT("Hunter");
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
    AddExpectedError(TEXT("[Encounter]"), EAutomationExpectedErrorFlags::Contains, 2);
    for (const ERunPhase FailedPhase : {ERunPhase::Preparing, ERunPhase::Combat})
    {
        EncounterPreparationTests::FFixture Fixture;
        FText Error;
        if (!TestTrue(TEXT("The isolated preparation fixture initializes and saves"), Fixture.Initialize(Error))) return false;
        AEncounterManager* Encounter = Fixture.Encounter;
        Encounter->RunState = Fixture.Run.Get();
        Encounter->CombatManager = Fixture.Combat;
        Encounter->PartyDefinition = Fixture.Run->PartyDefinition;
        const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
        if (FailedPhase == ERunPhase::Preparing)
        {
            FRunCheckpointStorage::FailNextWriteForTesting();
            TestFalse(TEXT("Missing arena setup fails through the public encounter entry point"), Encounter->RequestStartNode(TEXT("Combat_01")));
        }
        else
        {
            if (!TestTrue(TEXT("A late preparation failure starts from the combat phase"), Fixture.Run->BeginEncounter(TEXT("Combat_01")) && Fixture.Run->MarkCombatStarted())) return false;
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
        TestTrue(TEXT("The failure retains its preparation and persistence reasons"), !PreparationError.IsEmpty() && !Fixture.Run->GetSaveError().IsEmpty() && Encounter->GetFlowMessage().ToString().Contains(PreparationError.ToString()) && Encounter->GetFlowMessage().ToString().Contains(Fixture.Run->GetSaveError().ToString()));
        TestFalse(TEXT("Pending cancellation cannot start the same node again"), Encounter->RequestStartNode(TEXT("Combat_01")));

        int32 MapEvents = 0;
        bool bRetryVisibleDuringMapEvent = false;
        Fixture.Run->OnRunStateChanged.AddLambda([&]()
        {
            if (Fixture.Run->GetPhase() == ERunPhase::Map)
            {
                ++MapEvents;
                bRetryVisibleDuringMapEvent = Encounter->CanRetryCombatCheckpoint();
            }
        });
        FRunCheckpointStorage::FailNextWriteForTesting();
        TestFalse(TEXT("Another failed save keeps cancellation retryable"), Encounter->RetryCombatCheckpoint(Error));
        TestTrue(TEXT("Repeated failure reports its error without advancing phase or disk"), !Error.IsEmpty() && Encounter->CanRetryCombatCheckpoint() && Fixture.Run->GetPhase() == FailedPhase && Fixture.ReadBytes() == BeforeBytes);
        TestEqual(TEXT("Failed cancellation never publishes a map transition"), MapEvents, 0);
        TestTrue(TEXT("The same retry action commits cancellation once storage recovers"), Encounter->RetryCombatCheckpoint(Error));
        TestTrue(TEXT("Successful cancellation clears persistence and retry state"), Error.IsEmpty() && Fixture.Run->GetSaveError().IsEmpty() && !Encounter->CanRetryCombatCheckpoint());
        TestTrue(TEXT("The map permits the original node without losing the setup error"), Fixture.Run->CanStartNode(TEXT("Combat_01")) && Fixture.Run->GetCurrentNodeId().IsNone() && Fixture.Run->GetCurrentEncounterId().IsNone() && Encounter->GetFlowMessage().EqualTo(PreparationError));
        TestEqual(TEXT("Successful cancellation publishes the map once"), MapEvents, 1);
        TestFalse(TEXT("The synchronous map observer sees cancellation already completed"), bRetryVisibleDuringMapEvent);
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
        TestTrue(TEXT("The committed cancellation persists a clean map"), Saved.IsValid() && Saved->Phase == ERunPhase::Map && Saved->CurrentNode.IsNone() && Saved->CurrentEncounter.IsNone());
        TestFalse(TEXT("A duplicate retry does not repeat cancellation"), Encounter->RetryCombatCheckpoint(Error));
        TestEqual(TEXT("Duplicate retries publish no extra transition"), MapEvents, 1);
        Fixture.Run->OnRunStateChanged.Clear();
    }
    return true;
}

#endif
