#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/CombatManager.h"
#include "Engine/World.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

// Old snapshots remain readable save data, but cannot resume the retired sequential runtime.
// 이전 스냅샷은 읽을 수 있는 저장 데이터로 유지하되 사용 중단된 순차 실행을 재개하지 못합니다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRetiredCheckpointTest, "ProjectA.Combat.Checkpoint.RetiredRuntimeRejectsWithoutChangingSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRetiredCheckpointTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->CurrentNode = TEXT("SavedNode");
    Save->CurrentEncounter = TEXT("SavedEncounter");
    Save->Identity.RunId = FGuid::NewGuid();
    Save->CombatCheckpoint.AttemptId = FGuid::NewGuid();
    Save->CombatCheckpoint.Revision = 7;
    Save->CombatCheckpoint.CompletedTurnSerial = 12;
    Save->CombatCheckpoint.NextTurnIndex = 1;
    FCombatCheckpointUnit& SavedUnit = Save->CombatCheckpoint.Units.AddDefaulted_GetRef();
    SavedUnit.UnitId = FGuid::NewGuid();
    SavedUnit.CharacterId = FGuid::NewGuid();
    SavedUnit.HP = 43.0f;
    SavedUnit.GridCoord = FIntPoint(1, 0);
    const FCombatCheckpointData Before = Save->CombatCheckpoint;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Persistent Run saves still serialize"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Loaded(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Persistent Run saves still deserialize"), Loaded.Get())) return false;
    TestEqual(TEXT("Run identity survives serialization"), Loaded->Identity.RunId, Save->Identity.RunId);
    TestEqual(TEXT("Run progression survives serialization"), Loaded->CurrentNode, Save->CurrentNode);
    TestTrue(TEXT("Legacy checkpoint payload remains readable"), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Before, &Loaded->CombatCheckpoint, 0));
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
    ACombatManager* Combat = World->SpawnActor<ACombatManager>();
    TestFalse(TEXT("Old boundary cannot start a sequential combat"), Combat->RestoreCombatFromBoundary(Loaded->CombatCheckpoint.CompletedTurnSerial, Loaded->CombatCheckpoint.NextTurnIndex));
    TestFalse(TEXT("Old checkpoint retry explicitly rejects"), Combat->RetryTurnCheckpoint());
    TestFalse(TEXT("Round runtime never waits on old turn checkpoints"), Combat->IsAwaitingTurnCheckpoint());
    TestFalse(TEXT("Rejected restore cannot activate combat"), Combat->IsCombatActive());
    TestNull(TEXT("Rejected restore creates no turn manager"), Combat->GetTurnManager());
    TestTrue(TEXT("Rejected restore leaves retained save bytes represented identically"), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Before, &Loaded->CombatCheckpoint, 0));
    Combat->ResetCombat();
    World->DestroyWorld(false);
    return true;
}

#endif
