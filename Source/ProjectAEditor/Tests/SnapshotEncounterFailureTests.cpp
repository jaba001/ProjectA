#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatManager.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"

namespace ProjectASnapshotEncounterTests
{
// Exercise preparation failures against saved Gameplay actors without starting a combat turn.
// 전투 턴을 시작하지 않고 저장된 Gameplay 액터에서 준비 실패를 검증합니다.
class FCheckPreparationFailures : public IAutomationLatentCommand
{
public:
    explicit FCheckPreparationFailures(FAutomationTestBase* InTest) : Test(InTest), StartedAt(FPlatformTime::Seconds()), SlotId(*(TEXT("Failure_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)))
    {
    }

    virtual ~FCheckPreparationFailures() override
    {
        UGameplayStatics::DeleteGameInSlot(UPartySnapshotLibrary::GetSaveSlotName(SlotId), 0);
    }

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedAt > 60.0)
        {
            Test->AddError(TEXT("Snapshot preparation failure test timed out waiting for Gameplay PIE."));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        AGameplayGameModeBase* Mode = World ? Cast<AGameplayGameModeBase>(World->GetAuthGameMode()) : nullptr;
        AEncounterManager* Encounter = Mode ? Mode->GetEncounterManager() : nullptr;
        ACombatManager* Combat = Encounter ? Encounter->GetCombatManager() : nullptr;
        if (!Combat)
        {
            return false;
        }
        ACombatArena* Arena = nullptr;
        for (TActorIterator<ACombatArena> It(World); It; ++It)
        {
            if (It->ActorHasTag(Mode->ArenaTag))
            {
                Arena = *It;
                break;
            }
            if (!Arena)
            {
                Arena = *It;
            }
        }
        if (!Test->TestNotNull(TEXT("Gameplay contains an arena selected by tag or first-actor fallback."), Arena))
        {
            return true;
        }
        bool bSetupValid = Test->TestNotNull(TEXT("The selected Gameplay arena contains a grid."), Arena->Grid.Get());
        bSetupValid &= Test->TestTrue(TEXT("The selected Gameplay arena provides at least two enemy coordinates."), Arena->EnemyCoords.Num() >= 2);
        bSetupValid &= Test->TestNotNull(TEXT("Gameplay provides its player party catalog."), Mode->PartyDefinition.Get());
        bSetupValid &= Test->TestNotNull(TEXT("Gameplay provides its local opponent catalog."), Mode->LocalOpponentCatalog.Get());
        if (Mode->LocalOpponentCatalog)
        {
            bSetupValid &= Test->TestTrue(TEXT("The local opponent catalog contains skill entries."), !Mode->LocalOpponentCatalog->Skills.IsEmpty());
        }
        if (!bSetupValid)
        {
            return true;
        }
        if (!Test->TestTrue(TEXT("Failure scenarios begin before combat registration."), Encounter->GetSpawnedUnits().IsEmpty() && Combat->GetRegisteredUnits().IsEmpty() && !Combat->IsCombatActive()))
        {
            return true;
        }

        URunStateSubsystem* Run = World->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        Run->PartyDefinition = Mode->PartyDefinition;
        FRunPartyMember Player;
        Player.SlotIndex = 0;
        Player.CharacterName = FText::FromString(TEXT("Snapshot preparation tester"));
        Player.ClassId = TEXT("Hunter");
        Player.bCreated = true;
        FText Error;
        if (!Test->TestTrue(TEXT("A valid player party initializes the failure-test run."), Run->InitializeRun({Player}, Error)))
        {
            return true;
        }

        UEncounterDefinitionDataAsset* Definition = NewObject<UEncounterDefinitionDataAsset>(Mode);
        Definition->OpponentSnapshotSlot = SlotId;
        Definition->SnapshotCatalog = Mode->LocalOpponentCatalog;
        // Retain a valid PvE enemy to detect accidental fallback when the snapshot fails.
        // 스냅샷 실패 시 잘못된 대체 실행을 감지하도록 유효한 PvE 적을 남겨 둡니다.
        Definition->EnemyUnitClasses.Add(Mode->LocalOpponentCatalog->EnemyClasses.FindRef(TEXT("Hunter")));
        TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>> Definitions;
        Definitions.Add(Run->GetNodes()[0].EncounterId, Definition);
        Combat->OnCombatResult.RemoveAll(Encounter);
        Encounter->InitializeEncounter(Arena, Combat, Mode->PartyDefinition, Definitions);

        FPartySnapshot Snapshot;
        Test->TestFalse(TEXT("The isolated snapshot slot begins missing."), UPartySnapshotLibrary::LoadSnapshot(SlotId, Snapshot, Error));
        if (!CheckRejected(TEXT("Missing snapshot"), Error, Encounter, Combat, Arena, Run))
        {
            return true;
        }

        // Write directly through SaveGame to exercise malformed data that the public writer rejects.
        // 공개 저장 함수가 거절하는 잘못된 데이터를 검사하도록 SaveGame으로 직접 기록합니다.
        UPartySnapshotSaveGame* InvalidSave = NewObject<UPartySnapshotSaveGame>();
        InvalidSave->Snapshot.SchemaVersion = 2;
        if (!Test->TestTrue(TEXT("The unsupported-version fixture is saved."), UGameplayStatics::SaveGameToSlot(InvalidSave, UPartySnapshotLibrary::GetSaveSlotName(SlotId), 0)))
        {
            return true;
        }
        Test->TestFalse(TEXT("The fixture is rejected for its unsupported schema."), UPartySnapshotLibrary::LoadSnapshot(SlotId, Snapshot, Error));
        if (!CheckRejected(TEXT("Unsupported schema"), Error, Encounter, Combat, Arena, Run))
        {
            return true;
        }

        Snapshot.SnapshotId = TEXT("FormationFailure");
        Snapshot.ContentVersion = Mode->LocalOpponentCatalog->ContentVersion;
        FPartySnapshotMember Member;
        Member.MemberId = TEXT("FirstOpponent");
        Member.ClassId = TEXT("Hunter");
        Member.CharacterName = TEXT("First opponent");
        Member.SkillIds.Add(Mode->LocalOpponentCatalog->Skills.CreateConstIterator().Key());
        Snapshot.Members.Add(Member);
        Member.MemberId = TEXT("SecondOpponent");
        Member.CharacterName = TEXT("Second opponent");
        Member.FormationSlot = 1;
        Snapshot.Members.Add(Member);
        if (!Test->TestTrue(TEXT("Distinct formation slots pass snapshot/catalog validation."), Mode->LocalOpponentCatalog->ValidateForEncounter(Snapshot, Arena->EnemyCoords.Num(), Error)) || !Test->TestTrue(TEXT("The valid formation fixture is saved."), UPartySnapshotLibrary::SaveSnapshot(SlotId, Snapshot, Error)))
        {
            return true;
        }
        const FIntPoint OriginalSecondCoord = Arena->EnemyCoords[1];
        Arena->EnemyCoords[1] = Arena->EnemyCoords[0];
        const FText FormationError = FText::FromString(TEXT("Opponent Snapshot formation requires distinct empty enemy tiles. / 상대 스냅샷 배치에는 중복되지 않는 빈 적 타일이 필요합니다."));
        CheckRejected(TEXT("Duplicate physical formation tile"), FormationError, Encounter, Combat, Arena, Run);
        Arena->EnemyCoords[1] = OriginalSecondCoord;
        return true;
    }

private:
    bool CheckRejected(const FString& Scenario, const FText& ExpectedError, AEncounterManager* Encounter, ACombatManager* Combat, ACombatArena* Arena, URunStateSubsystem* Run)
    {
        const FName NodeId = Run->GetNodes()[0].NodeId;
        const bool bStarted = Encounter->RequestStartNode(NodeId);
        Test->TestFalse(Scenario + TEXT(": preparation rejects the request."), bStarted);
        Test->TestFalse(Scenario + TEXT(": original failure detail is available."), ExpectedError.IsEmpty());
        Test->TestEqual(Scenario + TEXT(": original failure detail reaches the UI."), Encounter->GetFlowMessage().ToString(), ExpectedError.ToString());
        Test->TestTrue(Scenario + TEXT(": the run returns to Map."), Run->GetPhase() == ERunPhase::Map);
        Test->TestTrue(Scenario + TEXT(": the node remains retryable."), Run->CanStartNode(NodeId));
        Test->TestTrue(Scenario + TEXT(": no encounter actors remain."), Encounter->GetSpawnedUnits().IsEmpty());
        Test->TestTrue(Scenario + TEXT(": no combat units remain registered."), Combat->GetRegisteredUnits().IsEmpty());
        Test->TestTrue(Scenario + TEXT(": no turn units remain registered."), !Combat->GetTurnManager() || Combat->GetTurnManager()->GetRegisteredUnitCount() == 0);
        Test->TestFalse(Scenario + TEXT(": combat is inactive."), Combat->IsCombatActive());
        for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Arena->Grid->TileMap)
        {
            Test->TestNull(Scenario + TEXT(": grid occupancy is clear."), Entry.Value->GetOccupyingUnit());
        }
        return !bStarted && Run->GetPhase() == ERunPhase::Map;
    }

    FAutomationTestBase* Test;
    double StartedAt;
    FName SlotId;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSnapshotEncounterFailureTest, "ProjectA.Snapshot.SavedGameplayPreparationFailures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSnapshotEncounterFailureTest::RunTest(const FString& Parameters)
{
    AddExpectedErrorPlain(TEXT("[Encounter]"), EAutomationExpectedErrorFlags::Contains, 3);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectASnapshotEncounterTests::FCheckPreparationFailures>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
