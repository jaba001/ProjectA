#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Combat/CombatManager.h"
#include "Components/Button.h"
#include "Controller/GameplayPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatHUDWidget.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace CombatCheckpointPIETests
{
enum class ERunMode : uint8
{
    SessionRestart,
    ProcessWriter,
    ProcessReader
};

enum class EOpponentSourceChange : uint8
{
    None,
    Replace,
    Delete
};

enum class EStep : uint8
{
    Launch,
    Connect,
    Opening,
    GuestTurn,
    Potion,
    Move,
    Skill,
    Confirmed,
    SaveFailure,
    Retry,
    UnconfirmedMove,
    Restored,
    OldRequest,
    NewRequest,
    Closing,
    Finished
};

bool SameCheckpoint(const FCombatCheckpointData& A, const FCombatCheckpointData& B)
{
    return FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}

UCombatHUDWidget* FindHUD(APartyPlayerController* Controller)
{
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Widgets, UCombatHUDWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        UCombatHUDWidget* HUD = Cast<UCombatHUDWidget>(Widget);
        if (HUD && HUD->GetWorld() == Controller->GetWorld() && HUD->GetOwningPlayer() == Controller && HUD->IsActivated())
        {
            return HUD;
        }
    }
    return nullptr;
}

class FCheckpointSessions : public IAutomationLatentCommand
{
public:
    FCheckpointSessions(FAutomationTestBase* InTest, ERunMode InMode, FString InSlot, EOpponentSourceChange InOpponentChange = EOpponentSourceChange::None) : Test(InTest), Mode(InMode), Slot(MoveTemp(InSlot)), OpponentChange(InOpponentChange), bRestoring(InMode == ERunMode::ProcessReader), StepStarted(FPlatformTime::Seconds())
    {
        if (OpponentChange != EOpponentSourceChange::None)
        {
            OpponentSlot = FName(*(TEXT("CheckpointOpponent_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        }
    }

    virtual ~FCheckpointSessions() override
    {
        RemoveActionObserver();
        if (Step != EStep::Finished && GEditor && HasPIEWorld())
        {
            GEditor->RequestEndPlayMap();
        }
        if (!bPreserveWriterFile)
        {
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }
        if (!OpponentSlot.IsNone())
        {
            UGameplayStatics::DeleteGameInSlot(UPartySnapshotLibrary::GetSaveSlotName(OpponentSlot), 0);
        }
    }

    virtual bool Update() override
    {
        if (Step == EStep::Closing)
        {
            if (!HasPIEWorld())
            {
                if (bRestartAfterClose)
                {
                    Test->TestTrue(TEXT("The first session's combat units were destroyed."), !OldGuest.IsValid() && !OldHost.IsValid());
                    if (!ChangeOpponentSource())
                    {
                        Step = EStep::Finished;
                        return true;
                    }
                    ClearSessionReferences();
                    bRestoring = true;
                    bRestartAfterClose = false;
                    Advance(EStep::Launch);
                    return false;
                }
                Step = EStep::Finished;
                return true;
            }
            if (FPlatformTime::Seconds() - StepStarted > 30.0)
            {
                Test->AddError(TEXT("Checkpoint PIE session did not close."));
                return true;
            }
            return false;
        }
        if (FPlatformTime::Seconds() - StepStarted > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Checkpoint PIE timed out at step %d (restoring=%d): %s"), static_cast<int32>(Step), bRestoring, *Diagnostic));
            return CloseSession();
        }
        if (Step == EStep::Launch)
        {
            if (!EditorNavigationReady())
            {
                return false;
            }
            PlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
            PlaySettings->SetPlayNetMode(PIE_ListenServer);
            PlaySettings->SetPlayNumberOfClients(2);
            PlaySettings->SetRunUnderOneProcess(true);
            PlaySettings->bLaunchSeparateServer = false;
            PlaySettings->NewWindowWidth = 1280;
            PlaySettings->NewWindowHeight = 720;
            PlaySettings->SetClientWindowSize(FIntPoint(1280, 720));
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings = PlaySettings.Get();
            Params.SessionDestination = EPlaySessionDestinationType::InProcess;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            Params.bAllowOnlineSubsystem = false;
            Params.GlobalMapOverride = TEXT("/Game/User_JeHoon/LEVEL/Gameplay");
            Params.StartLocation = FVector(0.0f, 0.0f, 300.0f);
            GEditor->RequestPlaySession(Params);
            Advance(EStep::Connect);
            return false;
        }
        if (Step == EStep::Connect)
        {
            if (!FindConnectedWorlds())
            {
                return false;
            }
            if (!(bRestoring ? RestoreRun() : StartRun()))
            {
                return CloseSession();
            }
            Advance(bRestoring ? EStep::Restored : EStep::Opening);
            return false;
        }
        if (!Server.IsValid() || !ClientWorld.IsValid() || !Host.IsValid() || !Client.IsValid() || !Remote.IsValid() || !Combat.IsValid() || !Run.IsValid() || !Guest.IsValid() || !HostUnit.IsValid() || !Enemy.IsValid())
        {
            Test->AddError(TEXT("Checkpoint PIE lost a required runtime object."));
            return CloseSession();
        }
        AGameplayGameState* ClientState = ClientWorld->GetGameState<AGameplayGameState>();
        ACombatManager* ClientCombat = ClientState ? ClientState->GetCombatManager() : nullptr;
        if (!ClientCombat || Client->GetCombatManager() != ClientCombat)
        {
            return false;
        }
        if (Step == EStep::Opening)
        {
            if (!ViewsMatch(ClientCombat) || !UnitsMatch(ClientCombat) || !Host->CanUseActiveUnitAction())
            {
                return false;
            }
            Test->TestEqual(TEXT("The first inactive turn boundary is durably confirmed once."), Run->GetCombatCheckpoint().Revision, static_cast<int64>(1));
            if (!CheckFrozenOpponent())
            {
                return CloseSession();
            }
            if (!Send(Host.Get(), ECombatActionKind::EndTurn))
            {
                return CloseSession();
            }
            Advance(EStep::GuestTurn);
        }
        else if (Step == EStep::GuestTurn)
        {
            if (Combat->GetCurrentUnit() != Guest.Get() || !ViewsMatch(ClientCombat) || !UnitsMatch(ClientCombat) || !Client->CanUseActiveUnitAction())
            {
                return false;
            }
            AUnitBase* ClientGuest = ClientCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(Guest.Get()));
            if (!ClientGuest || !Send(Client.Get(), ECombatActionKind::HealingItem, nullptr, ClientGuest->GetCurrentTile()))
            {
                return CloseSession();
            }
            Advance(EStep::Potion);
        }
        else if (Step == EStep::Potion)
        {
            if (!ClientResponseArrived() || !UnitsMatch(ClientCombat))
            {
                return false;
            }
            if (Client->GetLastCombatActionResponse().Result != ECombatRequestResult::Accepted)
            {
                CheckAccepted();
                return CloseSession();
            }
            if (!ServerNavigationReady(Guest.Get(), MoveCoord))
            {
                return false;
            }
            CheckAccepted();
            Test->TestEqual(TEXT("Guest potion changes stock before capture."), Guest->HealingItemCount, 0);
            ObserveAction(Guest.Get(), EUnitActionType::Move);
            if (!Send(Client.Get(), ECombatActionKind::Move, nullptr, ClientCombat->GetTileByCoord(MoveCoord)))
            {
                return CloseSession();
            }
            Advance(EStep::Move);
        }
        else if (Step == EStep::Move)
        {
            if (ActionFailed())
            {
                return CloseSession();
            }
            if (!ClientResponseArrived() || !ActionResult.IsSet() || !UnitsMatch(ClientCombat))
            {
                return false;
            }
            if (!CheckAccepted() || !Test->TestTrue(TEXT("Guest completed the real movement before capture."), Guest->GetCurrentTile() && Guest->GetCurrentTile()->GridCoord == MoveCoord))
            {
                return CloseSession();
            }
            AUnitBase* ClientGuest = ClientCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(Guest.Get()));
            AUnitBase* ClientEnemy = ClientCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(Enemy.Get()));
            USkillDefinitionDataAsset* Skill = ClientGuest ? ClientGuest->FindSkillDataByAbilityClass(ClientGuest->GetDefaultAttackAbilityClass()) : nullptr;
            if (!Skill || !ClientEnemy || !ClientEnemy->GetCurrentTile())
            {
                return false;
            }
            EnemyHPBeforeSkill = Enemy->GetAttributeSet()->GetHP();
            ObserveAction(Guest.Get(), EUnitActionType::Skill);
            if (!Send(Client.Get(), ECombatActionKind::Skill, Skill, ClientEnemy->GetCurrentTile()))
            {
                return CloseSession();
            }
            Advance(EStep::Skill);
        }
        else if (Step == EStep::Skill)
        {
            if (ActionFailed())
            {
                return CloseSession();
            }
            if (!ClientResponseArrived() || !ActionResult.IsSet() || !UnitsMatch(ClientCombat))
            {
                return false;
            }
            if (!CheckAccepted() || !Test->TestTrue(TEXT("Confirmed combat includes actual skill damage."), Enemy->GetAttributeSet()->GetHP() < EnemyHPBeforeSkill))
            {
                return CloseSession();
            }
            RemoveActionObserver();
            if (!Send(Client.Get(), ECombatActionKind::EndTurn))
            {
                return CloseSession();
            }
            Advance(EStep::Confirmed);
        }
        else if (Step == EStep::Confirmed)
        {
            if (!ClientResponseArrived() || Combat->GetCurrentUnit() != HostUnit.Get() || Combat->GetTurnSerial() < 4 || !Run->HasCombatCheckpoint() || Combat->IsAwaitingTurnCheckpoint() || !ViewsMatch(ClientCombat) || !UnitsMatch(ClientCombat))
            {
                return false;
            }
            if (!CheckAccepted() || !ReadAndCompareCommitted())
            {
                return CloseSession();
            }
            Expected = Run->GetCombatCheckpoint();
            Test->TestTrue(TEXT("The completed guest turn retains spent AP and SubAP."), Expected.Units.ContainsByPredicate([this](const FCombatCheckpointUnit& Unit) { return Unit.CharacterId == GuestCharacter && Unit.AP < Unit.MaxAP && Unit.SubAP == 0 && Unit.HealingItemCount == 0 && Unit.GridCoord == MoveCoord; }));
            // One failed durable write must pause the same boundary without publishing its revision.
            // 영구 저장을 한 번 실패시키면 revision 게시 없이 같은 턴 경계에서 멈춰야 합니다.
            FRunCheckpointStorage::FailNextWriteForTesting();
            if (!Send(Host.Get(), ECombatActionKind::EndTurn))
            {
                return CloseSession();
            }
            Advance(EStep::SaveFailure);
        }
        else if (Step == EStep::SaveFailure)
        {
            if (!Combat->IsAwaitingTurnCheckpoint() || !ViewsMatch(ClientCombat) || Client->CanUseActiveUnitAction())
            {
                return false;
            }
            Test->TestFalse(TEXT("Save failure prevents the next host action."), Host->CanUseActiveUnitAction());
            Test->TestTrue(TEXT("Save failure exposes a reason."), !Run->GetSaveError().IsEmpty() || !Encounter->GetFlowMessage().IsEmpty());
            Test->TestTrue(TEXT("Save failure preserves the last confirmed memory checkpoint."), SameCheckpoint(Expected, Run->GetCombatCheckpoint()));
            if (!ReadAndCompareCommitted())
            {
                return CloseSession();
            }
            FText Error;
            if (!Test->TestTrue(TEXT("The pending boundary can retry its durable write."), Encounter->RetryCombatCheckpoint(Error)))
            {
                Test->AddError(Error.ToString());
                return CloseSession();
            }
            Advance(EStep::Retry);
        }
        else if (Step == EStep::Retry)
        {
            if (Combat->IsAwaitingTurnCheckpoint() || Combat->GetCurrentUnit() != Guest.Get() || !ViewsMatch(ClientCombat) || !UnitsMatch(ClientCombat) || !Client->CanUseActiveUnitAction() || !ServerNavigationReady(Guest.Get(), UnconfirmedCoord))
            {
                return false;
            }
            Test->TestEqual(TEXT("Retry publishes exactly one newer checkpoint."), Run->GetCombatCheckpoint().Revision, Expected.Revision + 1);
            Expected = Run->GetCombatCheckpoint();
            if (!ReadAndCompareCommitted() || !CheckRuntimeAgainstCheckpoint(Combat.Get(), false))
            {
                return CloseSession();
            }
            OldCombatId = Combat->GetCombatInstanceId();
            OldBindingId = Client->GetParticipantBindingId();
            for (AUnitBase* Unit : Combat->GetRegisteredUnits())
            {
                OldRuntimeIds.Add(Combat->GetRuntimeUnitId(Unit));
            }
            // Slow only this fixture action so the test can observe interruption before arrival.
            // 도착 전에 중단 상태를 관찰할 수 있도록 이번 테스트 행동의 이동 속도만 낮춥니다.
            Guest->GetCharacterMovement()->MaxWalkSpeed = 50.0f;
            if (!Send(Client.Get(), ECombatActionKind::Move, nullptr, ClientCombat->GetTileByCoord(UnconfirmedCoord), &OldRequest))
            {
                return CloseSession();
            }
            Advance(EStep::UnconfirmedMove);
        }
        else if (Step == EStep::UnconfirmedMove)
        {
            if (!ClientResponseArrived() || !Guest->IsBusy())
            {
                return false;
            }
            if (!CheckAccepted())
            {
                return CloseSession();
            }
            FCombatCheckpointUnit RejectedCapture;
            FText Error;
            Test->TestFalse(TEXT("An actual in-flight movement cannot be captured as a boundary."), Guest->CaptureCheckpointState(RejectedCapture, Error));
            Test->TestFalse(TEXT("Rejected in-flight capture supplies an empty error."), Error.IsEmpty());
            Test->TestFalse(TEXT("An in-flight action cannot request an unrelated checkpoint retry."), Encounter->RetryCombatCheckpoint(Error));
            Test->TestFalse(TEXT("Rejected in-flight retry supplies an empty error."), Error.IsEmpty());
            Test->TestTrue(TEXT("In-flight action leaves the last confirmed checkpoint unchanged."), SameCheckpoint(Expected, Run->GetCombatCheckpoint()));
            if (!ReadAndCompareCommitted())
            {
                return CloseSession();
            }
            OldGuest = Guest;
            OldHost = HostUnit;
            bPreserveWriterFile = Mode == ERunMode::ProcessWriter;
            return CloseSession(Mode == ERunMode::SessionRestart);
        }
        else if (Step == EStep::Restored)
        {
            if (!ViewsMatch(ClientCombat) || !UnitsMatch(ClientCombat) || !Client->CanUseActiveUnitAction() || !FindHUD(Host.Get()) || !FindHUD(Client.Get()))
            {
                return false;
            }
            if (!CheckRuntimeAgainstCheckpoint(Combat.Get(), true) || !CheckRuntimeAgainstCheckpoint(ClientCombat, true))
            {
                return CloseSession();
            }
            Test->TestTrue(TEXT("The original Host identity and epoch survive session restoration."), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Expected.Identity, &Run->GetRunIdentity(), 0));
            Test->TestTrue(TEXT("Restoration preserves the confirmed body and attempt."), SameCheckpoint(Expected, Run->GetCombatCheckpoint()));
            if (!CheckFrozenOpponent() || !CheckChangedOpponentSource())
            {
                return CloseSession();
            }
            Test->TestTrue(TEXT("The new session does not reuse the old combat request nonce."), Combat->GetCombatInstanceId().IsValid() && Combat->GetCombatInstanceId() != OldCombatId);
            Test->TestTrue(TEXT("The new client connection receives a fresh binding."), Client->GetParticipantBindingId().IsValid() && Client->GetParticipantBindingId() != OldBindingId);
            Test->TestTrue(TEXT("Both HUDs show the restored turn."), FindHUD(Host.Get())->GetTurnInfoText().ToString() == FindHUD(Client.Get())->GetTurnInfoText().ToString());
            UButton* HostEndTurn = Cast<UButton>(FindHUD(Host.Get())->GetWidgetFromName(TEXT("Button_EndTurn")));
            UButton* ClientEndTurn = Cast<UButton>(FindHUD(Client.Get())->GetWidgetFromName(TEXT("Button_EndTurn")));
            Test->TestTrue(TEXT("Restored HUD input is available only to the current owner."), HostEndTurn && ClientEndTurn && !HostEndTurn->GetIsEnabled() && ClientEndTurn->GetIsEnabled());
            Test->TestTrue(TEXT("Client RunState remains non-authoritative after restore."), ClientWorld->GetGameInstance()->GetSubsystem<URunStateSubsystem>()->GetPhase() == ERunPhase::None);
            if (!Test->TestFalse(TEXT("The Host cannot take over the guest's restored turn."), Host->CanUseActiveUnitAction()))
            {
                return CloseSession();
            }
            FCombatActionRequest WrongOwner;
            if (!Host->BuildCombatActionRequest(ECombatActionKind::EndTurn, nullptr, nullptr, WrongOwner))
            {
                Test->AddError(TEXT("The Host could not encode an ownership rejection fixture."));
                return CloseSession();
            }
            Test->TestTrue(TEXT("Restored ownership is enforced by the server."), Host->SubmitCombatActionRequest(WrongOwner).Result == ECombatRequestResult::NotOwner);
            if (Mode == ERunMode::ProcessReader)
            {
                if (!Send(Client.Get(), ECombatActionKind::EndTurn))
                {
                    return CloseSession();
                }
                Advance(EStep::NewRequest);
            }
            else
            {
                // Send the exact old packet through the new connection; old feedback is intentionally ignored by the client.
                // 새 연결로 이전 패킷 그대로 보내며 클라이언트는 이전 전투의 응답 표시를 의도적으로 무시합니다.
                Client->ServerRequestCombatAction(OldRequest);
                Advance(EStep::OldRequest);
            }
        }
        else if (Step == EStep::OldRequest)
        {
            const FCombatActionResponse& Response = Remote->GetLastCombatActionResponse();
            if (Response.CombatInstanceId != OldRequest.CombatInstanceId || Response.RequestSequence != OldRequest.RequestSequence)
            {
                return false;
            }
            Test->TestTrue(TEXT("The new server rejects the prior session's command."), Response.Result == ECombatRequestResult::InvalidContext);
            if (!CheckRuntimeAgainstCheckpoint(Combat.Get(), true) || !Send(Client.Get(), ECombatActionKind::EndTurn))
            {
                return CloseSession();
            }
            Advance(EStep::NewRequest);
        }
        else if (Step == EStep::NewRequest)
        {
            if (!ClientResponseArrived() || Combat->GetTurnSerial() <= Expected.CompletedTurnSerial + 1)
            {
                return false;
            }
            CheckAccepted();
            Test->TestTrue(TEXT("Restored combat makes a newer durable turn boundary."), Run->GetCombatCheckpoint().Revision > Expected.Revision);
            CheckFrozenOpponent();
            return CloseSession();
        }
        return false;
    }

private:
    bool StartRun()
    {
        URunStateSubsystem* State = Run.Get();
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        State->PartyDefinition = GameMode->PartyDefinition;
        State->EnableCheckpointSaving(Slot);
        FRunIdentityData Identity;
        Identity.Origin = ERunIdentityOrigin::AccountProvider;
        Identity.RunId = FGuid::NewGuid();
        Identity.HostEpoch = 1;
        TArray<FRunPartyMember> Party;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("CheckpointPIEFixture");
            Participant.AccountId.Subject = Index == 0 ? TEXT("OriginalHost") : TEXT("OriginalGuest");
            FRunPartyMember& Member = Party.AddDefaulted_GetRef();
            Member.SlotIndex = Index;
            Member.bCreated = true;
            Member.ClassId = TEXT("Hunter");
            Member.CharacterName = FText::FromString(Participant.AccountId.Subject);
            Member.CharacterId = FGuid::NewGuid();
            Member.OwnerAccountId = Participant.AccountId;
        }
        Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
        FText Error;
        if (!Test->TestTrue(TEXT("The server initializes an identified Run with an authored catalog."), State->InitializeRunWithIdentity(Party, Identity, Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        HostCharacter = Party[0].CharacterId;
        GuestCharacter = Party[1].CharacterId;
        if (OpponentChange != EOpponentSourceChange::None && !CreateOpponentFixture())
        {
            return false;
        }
        if (!BindOriginalParticipants() || !Test->TestTrue(TEXT("The server starts the checkpoint encounter."), Encounter->RequestStartNode(State->GetNodes()[0].NodeId)) || !ResolveFixtureUnits())
        {
            Test->AddError(Encounter->GetFlowMessage().ToString());
            return false;
        }
        // Keep authored asset paths stable; only these server runtime values differ for the capture fixture.
        // 작성된 에셋 경로는 유지하고 캡처 테스트를 위한 서버 실행 값만 변경합니다.
        const TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills = Guest->GetEquippedSkillDataAssets();
        if (!Test->TestTrue(TEXT("Runtime fixture stats preserve actual authored skills."), Guest->ConfigureProfession(Guest->GetAttributeSet()->GetMaxHP(), 4, 2, Skills)))
        {
            return false;
        }
        Guest->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Guest->GetAttributeSet()->GetMaxHP() - 40.0f);
        Guest->ForceNetUpdate();
        return true;
    }

    bool RestoreRun()
    {
        FText Error;
        if (OpponentChange != EOpponentSourceChange::None && (!CheckChangedOpponentSource() || !ConfigureOpponentDefinition()))
        {
            return false;
        }
        Run->EnableCheckpointSaving(Slot);
        if (!Test->TestTrue(TEXT("The new server loads a combat checkpoint from disk."), Run->LoadCheckpoint(Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        if (Mode == ERunMode::ProcessReader)
        {
            Expected = Run->GetCombatCheckpoint();
        }
        if (!Test->TestTrue(TEXT("No live memory state is needed to load the same confirmed body."), SameCheckpoint(Expected, Run->GetCombatCheckpoint())) || !Test->TestEqual(TEXT("The restored record contains both original participants."), Expected.Identity.OriginalParticipants.Num(), 2))
        {
            return false;
        }
        for (const FCombatCheckpointUnit& Unit : Expected.Units)
        {
            if (Unit.PartySlot == 0) HostCharacter = Unit.CharacterId;
            if (Unit.PartySlot == 1) GuestCharacter = Unit.CharacterId;
        }
        if (!BindOriginalParticipants())
        {
            return false;
        }
        if (!RejectMisplacedSavedTransform())
        {
            return false;
        }
        if (!Test->TestFalse(TEXT("The other original participant cannot restore as Host."), Encounter->RestoreSavedCombat(Expected.Identity.OriginalParticipants[1].AccountId, Error)) || !Test->TestTrue(TEXT("Wrong-Host rejection spawns no combat actors."), Encounter->GetSpawnedUnits().IsEmpty()))
        {
            return false;
        }
        if (!Test->TestTrue(TEXT("The original Host restores the saved combat boundary."), Encounter->RestoreSavedCombat(Expected.Identity.HostAccountId, Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        return ResolveFixtureUnits();
    }

    bool RejectMisplacedSavedTransform()
    {
        FText Error;
        TStrongObjectPtr<URunSaveGame> Original(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
        if (!Test->TestNotNull(TEXT("A valid committed record is retained for the arena mismatch regression."), Original.Get())) return false;
        TStrongObjectPtr<URunSaveGame> Misplaced(DuplicateObject<URunSaveGame>(Original.Get(), GetTransientPackage()));
        FCombatCheckpointUnit* Unit = Misplaced->CombatCheckpoint.Units.FindByPredicate([](const FCombatCheckpointUnit& Entry) { return !Entry.bDead && Entry.bHasTile; });
        if (!Test->TestNotNull(TEXT("The regression changes a living unit with saved grid occupancy."), Unit)) return false;
        // Preserve the grid coordinate and checksum-valid format while moving only the saved actor position.
        // 그리드 좌표와 정상 체크섬 형식을 유지한 채 저장된 액터 위치만 변경합니다.
        Unit->Transform.AddToTranslation(FVector(1000.0f, 0.0f, 0.0f));
        bool bValid = Test->TestTrue(TEXT("The misplaced-transform fixture is written with a valid storage envelope."), FRunCheckpointStorage::Save(Misplaced.Get(), Slot, Error));
        if (bValid)
        {
            bValid &= Test->TestTrue(TEXT("The misplaced record loads before arena-specific validation."), Run->LoadCheckpoint(Error));
            if (bValid)
            {
                bValid &= Test->TestFalse(TEXT("The original Host cannot restore a unit away from its saved grid tile."), Encounter->RestoreSavedCombat(Expected.Identity.HostAccountId, Error));
                bValid &= Test->TestTrue(TEXT("Arena mismatch exposes its reason without spawning or registering combat units."), !Error.IsEmpty() && !Encounter->GetFlowMessage().IsEmpty() && Encounter->GetSpawnedUnits().IsEmpty() && Combat->GetRegisteredUnits().IsEmpty());
                const URunSaveGame* AfterRejection = Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error));
                bValid &= Test->TestTrue(TEXT("Rejected restoration leaves the durable record unchanged."), AfterRejection && SameCheckpoint(AfterRejection->CombatCheckpoint, Misplaced->CombatCheckpoint));
            }
        }
        const bool bRestoredFile = Test->TestTrue(TEXT("The original valid checkpoint is restored after the rejection fixture."), FRunCheckpointStorage::Save(Original.Get(), Slot, Error));
        const bool bReloaded = bRestoredFile && Test->TestTrue(TEXT("Normal restoration reloads the original confirmed checkpoint."), Run->LoadCheckpoint(Error));
        return bValid && bReloaded;
    }

    bool ConfigureOpponentDefinition()
    {
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        const AGameplayGameState* GameState = Server->GetGameState<AGameplayGameState>();
        ACombatArena* Arena = GameState ? GameState->GetArena() : nullptr;
        if (!Test->TestTrue(TEXT("The isolated Snapshot fixture reuses authored catalog and idle Gameplay actors."), GameMode && GameMode->LocalOpponentCatalog && Arena && Encounter->GetSpawnedUnits().IsEmpty() && !Combat->IsCombatActive())) return false;
        UEncounterDefinitionDataAsset* Definition = NewObject<UEncounterDefinitionDataAsset>(GameMode);
        Definition->OpponentSnapshotSlot = OpponentSlot;
        Definition->SnapshotCatalog = GameMode->LocalOpponentCatalog;
        TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>> Definitions = GameMode->EncounterDefinitions;
        for (TPair<FName, TObjectPtr<UEncounterDefinitionDataAsset>>& Entry : Definitions)
        {
            Entry.Value = Definition;
        }
        Combat->OnCombatResult.RemoveAll(Encounter.Get());
        Encounter->InitializeEncounter(Arena, Combat.Get(), GameMode->PartyDefinition, Definitions);
        return true;
    }

    bool CreateOpponentFixture()
    {
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        if (!ConfigureOpponentDefinition()) return false;
        // Match the existing sample's authored build without reading or modifying its user-owned slot.
        // 사용자 샘플 슬롯을 읽거나 수정하지 않고 기존 샘플과 같은 작성된 빌드를 사용합니다.
        OriginalOpponent.SnapshotId = TEXT("CheckpointOriginalOpponent");
        OriginalOpponent.ContentVersion = GameMode->LocalOpponentCatalog->ContentVersion;
        FPartySnapshotMember& Member = OriginalOpponent.Members.AddDefaulted_GetRef();
        Member.MemberId = TEXT("CheckpointHunter");
        Member.ClassId = TEXT("Hunter");
        Member.CharacterName = TEXT("Frozen checkpoint Hunter");
        Member.Stats.MaxHP = 140.0f;
        Member.Stats.CurrentHP = 120.0f;
        Member.SkillIds = {TEXT("DefaultAttack"), TEXT("SweepingStrike")};
        OriginalOpponentCatalog = FSoftObjectPath(GameMode->LocalOpponentCatalog.Get());
        FText Error;
        if (!Test->TestTrue(TEXT("An isolated original opponent Snapshot is saved before the first encounter."), UPartySnapshotLibrary::SaveSnapshot(OpponentSlot, OriginalOpponent, Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        return true;
    }

    bool ChangeOpponentSource()
    {
        if (OpponentChange == EOpponentSourceChange::None) return true;
        // Change only the isolated source after the original PIE actors have been destroyed.
        // 원래 PIE 액터를 파괴한 뒤 격리된 원본 슬롯만 변경합니다.
        if (OpponentChange == EOpponentSourceChange::Delete)
        {
            if (!Test->TestTrue(TEXT("The isolated source Snapshot is deleted before restoring combat."), UGameplayStatics::DeleteGameInSlot(UPartySnapshotLibrary::GetSaveSlotName(OpponentSlot), 0))) return false;
        }
        else
        {
            ReplacementOpponent = OriginalOpponent;
            ReplacementOpponent.SnapshotId = TEXT("CheckpointReplacementOpponent");
            ReplacementOpponent.Members[0].CharacterName = TEXT("Replacement must not enter restored combat");
            ReplacementOpponent.Members[0].Stats.MaxHP = 500.0f;
            ReplacementOpponent.Members[0].Stats.CurrentHP = 499.0f;
            ReplacementOpponent.Members[0].SkillIds = {TEXT("SweepingStrike"), TEXT("DefaultAttack")};
            FText Error;
            if (!Test->TestTrue(TEXT("The source is replaced by a different valid opponent before restoring combat."), UPartySnapshotLibrary::SaveSnapshot(OpponentSlot, ReplacementOpponent, Error)))
            {
                Test->AddError(Error.ToString());
                return false;
            }
        }
        return CheckChangedOpponentSource();
    }

    bool CheckChangedOpponentSource()
    {
        if (OpponentChange == EOpponentSourceChange::None) return true;
        FPartySnapshot Current;
        FText Error;
        const bool bLoaded = UPartySnapshotLibrary::LoadSnapshot(OpponentSlot, Current, Error);
        if (OpponentChange == EOpponentSourceChange::Delete)
        {
            return Test->TestTrue(TEXT("The original opponent source remains absent throughout actual restoration."), !bLoaded && !UGameplayStatics::DoesSaveGameExist(UPartySnapshotLibrary::GetSaveSlotName(OpponentSlot), 0));
        }
        return Test->TestTrue(TEXT("The source contains the replacement instead of the original frozen opponent."), bLoaded && FPartySnapshot::StaticStruct()->CompareScriptStruct(&Current, &ReplacementOpponent, 0));
    }

    bool CheckFrozenOpponent()
    {
        if (OpponentChange == EOpponentSourceChange::None) return true;
        const FCombatCheckpointData& Saved = Run->GetCombatCheckpoint();
        const bool bFrozen = Saved.bHasOpponentSnapshot && Saved.OpponentCatalog == OriginalOpponentCatalog && FPartySnapshot::StaticStruct()->CompareScriptStruct(&Saved.OpponentSnapshot, &OriginalOpponent, 0);
        const bool bOriginalActor = Enemy.IsValid() && Enemy->RuntimeCharacterName.ToString() == OriginalOpponent.Members[0].CharacterName && FMath::IsNearlyEqual(Enemy->GetAttributeSet()->GetMaxHP(), OriginalOpponent.Members[0].Stats.MaxHP);
        return Test->TestTrue(TEXT("Actual combat and confirmed metadata retain the original frozen Snapshot and trusted catalog."), bFrozen && bOriginalActor);
    }

    bool BindOriginalParticipants()
    {
        const FRunIdentityData& Identity = Run->GetRunIdentity();
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        return Test->TestTrue(TEXT("Known test connections receive the original account identities."), Identity.OriginalParticipants.Num() == 2 && GameMode->AssignRunParticipant(Host.Get(), Identity.HostAccountId) && GameMode->AssignRunParticipant(Remote.Get(), Identity.OriginalParticipants[1].AccountId));
    }

    bool ResolveFixtureUnits()
    {
        for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
        {
            if (Combat->GetCharacterId(Unit) == HostCharacter) HostUnit = Unit;
            else if (Combat->GetCharacterId(Unit) == GuestCharacter) Guest = Unit;
            else if (Unit->GetTeam() == ETeam::Enemy) Enemy = Unit;
        }
        return Test->TestTrue(TEXT("The encounter creates two owned characters and one opponent."), HostUnit.IsValid() && Guest.IsValid() && Enemy.IsValid() && Encounter->GetSpawnedUnits().Num() == 3);
    }

    bool ReadAndCompareCommitted()
    {
        FText Error;
        const URunSaveGame* Saved = Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error));
        const bool bValid = Saved && Saved->Version == 3 && SameCheckpoint(Saved->CombatCheckpoint, Run->GetCombatCheckpoint());
        if (!Test->TestTrue(TEXT("The durable v3 record equals the last confirmed checkpoint."), bValid))
        {
            Test->AddError(Error.ToString());
        }
        return bValid;
    }

    bool CheckRuntimeAgainstCheckpoint(ACombatManager* Manager, bool bRequireFreshIds)
    {
        bool bValid = Test->TestEqual(TEXT("Restore keeps the complete turn order."), Manager->GetRegisteredUnits().Num(), Expected.Units.Num());
        bValid &= Test->TestEqual(TEXT("Restore activates exactly the next turn once."), Manager->GetTurnSerial(), Expected.CompletedTurnSerial + 1);
        for (int32 Index = 0; Index < Expected.Units.Num(); ++Index)
        {
            if (!Manager->GetRegisteredUnits().IsValidIndex(Index)) return false;
            AUnitBase* Unit = Manager->GetRegisteredUnits()[Index];
            const FCombatCheckpointUnit& Saved = Expected.Units[Index];
            const bool bActive = Index == Expected.NextTurnIndex;
            const FText UnitName = Unit && !Unit->RuntimeCharacterName.IsEmpty() ? Unit->RuntimeCharacterName : FText::FromString(GetNameSafe(Unit));
            bool bUnitValid = Unit && Unit->GetAttributeSet() && Unit->IsActiveTurn() == bActive && Unit->IsUnitAlive() != Saved.bDead && !Unit->IsBusy() && Unit->GetTeam() == Saved.Team && Unit->GetClass()->GetPathName() == Saved.UnitClass.ToString() && UnitName.EqualTo(Saved.CharacterName);
            if (!bUnitValid)
            {
                bValid &= Test->TestTrue(FString::Printf(TEXT("Restored unit %d has its saved type, team, life and turn state."), Index), false);
                continue;
            }
            bUnitValid &= FMath::IsNearlyEqual(Unit->GetAttributeSet()->GetHP(), Saved.HP) && FMath::IsNearlyEqual(Unit->GetAttributeSet()->GetMaxHP(), Saved.MaxHP);
            bUnitValid &= Unit->GetCurrentActionPoint() == (bActive ? Saved.MaxAP : Saved.AP) && Unit->GetCurrentSubActionPoint() == (bActive ? Saved.MaxSubAP : Saved.SubAP);
            bUnitValid &= Unit->GetMaxActionPoint() == Saved.MaxAP && Unit->GetMaxSubActionPoint() == Saved.MaxSubAP && Unit->HealingItemCount == Saved.HealingItemCount && Unit->HealingItemAmount == Saved.HealingItemAmount && Unit->GetMoveRange() == Saved.MoveRange;
            bUnitValid &= Manager->GetCharacterId(Unit) == Saved.CharacterId && Manager->GetOwnerAccountId(Unit) == Saved.OwnerAccountId;
            bUnitValid &= Saved.bHasTile ? Unit->GetCurrentTile() && Unit->GetCurrentTile()->GridCoord == Saved.GridCoord && Unit->GetCurrentTile()->GetOccupyingUnit() == Unit : !Unit->GetCurrentTile();
            bUnitValid &= FVector::DistSquared2D(Unit->GetActorLocation(), Saved.Transform.GetLocation()) < 1.0f;
            bUnitValid &= Unit->GetEquippedSkillDataAssets().Num() == Saved.Skills.Num() && FSoftObjectPath(Unit->GetDefaultAttackAbilityClass().Get()) == Saved.DefaultAttackAbility;
            for (int32 SkillIndex = 0; SkillIndex < Saved.Skills.Num() && Unit->GetEquippedSkillDataAssets().IsValidIndex(SkillIndex); ++SkillIndex)
            {
                bUnitValid &= FSoftObjectPath(Unit->GetEquippedSkillDataAssets()[SkillIndex].Get()) == Saved.Skills[SkillIndex];
            }
            if (bRequireFreshIds)
            {
                bUnitValid &= Manager->GetRuntimeUnitId(Unit).IsValid() && !OldRuntimeIds.Contains(Manager->GetRuntimeUnitId(Unit));
            }
            bValid &= Test->TestTrue(FString::Printf(TEXT("Restored unit %d retains HP, resources, placement, ownership and ordered loadout."), Index), bUnitValid);
        }
        return bValid;
    }

    bool UnitsMatch(ACombatManager* ClientCombat) const
    {
        if (ClientCombat->GetRegisteredUnits().Num() != Combat->GetRegisteredUnits().Num()) return false;
        for (AUnitBase* ServerUnit : Combat->GetRegisteredUnits())
        {
            AUnitBase* ClientUnit = ClientCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(ServerUnit));
            if (!ClientUnit || !ClientUnit->GetAttributeSet() || ClientUnit->IsBusy() != ServerUnit->IsBusy() || ClientUnit->IsUnitAlive() != ServerUnit->IsUnitAlive() || ClientUnit->GetCurrentActionPoint() != ServerUnit->GetCurrentActionPoint() || ClientUnit->GetCurrentSubActionPoint() != ServerUnit->GetCurrentSubActionPoint() || ClientUnit->HealingItemCount != ServerUnit->HealingItemCount || !FMath::IsNearlyEqual(ClientUnit->GetAttributeSet()->GetHP(), ServerUnit->GetAttributeSet()->GetHP()) || ClientCombat->GetOwnerAccountId(ClientUnit) != Combat->GetOwnerAccountId(ServerUnit)) return false;
            if (ClientUnit->GetMaxActionPoint() != ServerUnit->GetMaxActionPoint() || ClientUnit->GetMaxSubActionPoint() != ServerUnit->GetMaxSubActionPoint() || ClientUnit->GetMoveRange() != ServerUnit->GetMoveRange() || ClientUnit->HealingItemAmount != ServerUnit->HealingItemAmount || !FMath::IsNearlyEqual(ClientUnit->GetAttributeSet()->GetMaxHP(), ServerUnit->GetAttributeSet()->GetMaxHP()) || ClientUnit->GetEquippedSkillDataAssets() != ServerUnit->GetEquippedSkillDataAssets() || ClientUnit->GetDefaultAttackAbilityClass() != ServerUnit->GetDefaultAttackAbilityClass() || ClientUnit->IsActiveTurn() != ServerUnit->IsActiveTurn() || ClientUnit->GetTeam() != ServerUnit->GetTeam() || !ClientUnit->RuntimeCharacterName.EqualTo(ServerUnit->RuntimeCharacterName)) return false;
            ACombatGridTile* ServerTile = ServerUnit->GetCurrentTile();
            ACombatGridTile* ClientTile = ClientUnit->GetCurrentTile();
            if (ServerTile ? !ClientTile || ServerTile->GridCoord != ClientTile->GridCoord || ClientTile->GetOccupyingUnit() != ClientUnit : ClientTile != nullptr) return false;
        }
        return true;
    }

    bool ViewsMatch(ACombatManager* ClientCombat) const
    {
        const AGameplayGameState* ClientState = ClientWorld->GetGameState<AGameplayGameState>();
        if (!ClientState || ClientState->GetViewState().ConfirmedCombatRevision != Run->GetCombatCheckpoint().Revision) return false;
        return ClientCombat->GetCombatInstanceId() == Combat->GetCombatInstanceId() && ClientCombat->GetRunId() == Combat->GetRunId() && ClientCombat->GetHostEpoch() == Combat->GetHostEpoch() && ClientCombat->GetTurnSerial() == Combat->GetTurnSerial() && ClientCombat->GetRuntimeUnitId(ClientCombat->GetCurrentUnit()) == Combat->GetRuntimeUnitId(Combat->GetCurrentUnit());
    }

    bool Send(APartyPlayerController* Controller, ECombatActionKind Kind, USkillDefinitionDataAsset* Skill = nullptr, ACombatGridTile* Target = nullptr, FCombatActionRequest* OutRequest = nullptr)
    {
        FCombatActionRequest Request;
        if (!Test->TestTrue(TEXT("The fixture encodes a real controller action."), Controller->BuildCombatActionRequest(Kind, Skill, Target, Request))) return false;
        if (OutRequest) *OutRequest = Request;
        const FCombatActionResponse Response = Controller->SubmitCombatActionRequest(Request);
        if (Controller == Client.Get())
        {
            PendingSequence = Request.RequestSequence;
            return Test->TestTrue(TEXT("Client action is sent through the owning network connection."), Response.Result == ECombatRequestResult::Pending);
        }
        return Test->TestTrue(TEXT("The Host dispatches its own action."), Response.Result == ECombatRequestResult::Accepted);
    }

    bool ClientResponseArrived() const
    {
        const FCombatActionResponse& Response = Client->GetLastCombatActionResponse();
        return Response.RequestSequence == PendingSequence && Response.Result != ECombatRequestResult::Pending;
    }

    bool CheckAccepted()
    {
        return Test->TestTrue(TEXT("The server acknowledges the client action."), Client->GetLastCombatActionResponse().Result == ECombatRequestResult::Accepted);
    }

    void ObserveAction(AUnitBase* Unit, EUnitActionType Kind)
    {
        RemoveActionObserver();
        ActionResult.Reset();
        ObservedUnit = Unit;
        ActionObserver = Unit->OnActionCompleted.AddLambda([this, Kind](AUnitBase*, EUnitActionType Action, EUnitActionResult Result)
        {
            if (Action == Kind) ActionResult = Result;
        });
    }

    bool ActionFailed()
    {
        if (ActionResult.IsSet() && ActionResult.GetValue() != EUnitActionResult::Succeeded)
        {
            Test->AddError(FString::Printf(TEXT("The actual fixture action failed: %d. %s"), static_cast<int32>(ActionResult.GetValue()), *Diagnostic));
            return true;
        }
        return false;
    }

    void RemoveActionObserver()
    {
        if (ObservedUnit.IsValid() && ActionObserver.IsValid()) ObservedUnit->OnActionCompleted.Remove(ActionObserver);
        ActionObserver.Reset();
        ObservedUnit.Reset();
    }

    bool HasPIEWorld() const
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World()) return true;
        }
        return false;
    }

    bool FindConnectedWorlds()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || !World->GetMapName().Contains(TEXT("Gameplay"))) continue;
            if (World->GetNetMode() == NM_ListenServer) Server = World;
            else if (World->GetNetMode() == NM_Client) ClientWorld = World;
        }
        if (!Server.IsValid() || !ClientWorld.IsValid()) return false;
        UNetDriver* ServerDriver = Server->GetNetDriver();
        UNetDriver* ClientDriver = ClientWorld->GetNetDriver();
        if (!ServerDriver || !ClientDriver || !ClientDriver->ServerConnection || ClientDriver->ServerConnection->GetConnectionState() != USOCK_Open || ServerDriver->ClientConnections.Num() != 1 || ServerDriver->ClientConnections[0]->GetConnectionState() != USOCK_Open) return false;
        Client = Cast<AGameplayPlayerController>(ClientWorld->GetFirstPlayerController());
        for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
        {
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(It->Get());
            if (Controller && Controller->IsLocalController()) Host = Controller;
            else if (Controller && Controller->GetNetConnection()) Remote = Controller;
        }
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        AGameplayGameState* GameState = Server->GetGameState<AGameplayGameState>();
        if (!Client.IsValid() || !Host.IsValid() || !Remote.IsValid() || !GameMode || !GameMode->GetEncounterManager() || !GameState || !GameState->GetArena()) return false;
        Encounter = GameMode->GetEncounterManager();
        Combat = Encounter->GetCombatManager();
        Run = Server->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        Test->TestTrue(TEXT("Separate Listen Server and client worlds have real connected NetDrivers."), ServerDriver != ClientDriver && !Client->HasAuthority());
        return Combat.IsValid() && Run.IsValid();
    }

    bool NavigationReady(UWorld* World, const FVector& Start, const FVector& Goal, AActor* Context)
    {
        UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
        ANavigationData* NavData = Navigation ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        const bool bBuildingOrLocked = UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);
        const int32 Remaining = Navigation ? Navigation->GetNumRemainingBuildTasks() : -1;
        FNavLocation ProjectedStart;
        FNavLocation ProjectedGoal;
        const bool bStart = NavData && !bBuildingOrLocked && Navigation->ProjectPointToNavigation(Start, ProjectedStart, NavData->GetDefaultQueryExtent(), NavData);
        const bool bGoal = NavData && !bBuildingOrLocked && Navigation->ProjectPointToNavigation(Goal, ProjectedGoal, NavData->GetDefaultQueryExtent(), NavData);
        UNavigationPath* Path = bStart && bGoal ? UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, Goal, Context ? Context : NavData) : nullptr;
        const bool bPath = Path && Path->IsValid() && !Path->IsPartial();
        Diagnostic = FString::Printf(TEXT("World=%s Nav=%s BuildingOrLocked=%d Remaining=%d Start=%s Goal=%s Projected=%d/%d Path=%d"), *GetNameSafe(World), *GetNameSafe(NavData), bBuildingOrLocked, Remaining, *Start.ToString(), *Goal.ToString(), bStart, bGoal, bPath);
        return NavData && !bBuildingOrLocked && Remaining == 0 && bPath;
    }

    bool EditorNavigationReady()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (!World || !World->GetMapName().Contains(TEXT("Gameplay"))) return false;
        for (TActorIterator<ACombatArena> It(World); It; ++It)
        {
            ACombatGridManager* Grid = It->Grid;
            if (!Grid || !It->PlayerCoords.IsValidIndex(1)) continue;
            const FFloatProperty* SpacingProperty = FindFProperty<FFloatProperty>(Grid->GetClass(), TEXT("Spacing"));
            const FFloatProperty* GapProperty = FindFProperty<FFloatProperty>(Grid->GetClass(), TEXT("GapSpacing"));
            const FIntProperty* GapStartProperty = FindFProperty<FIntProperty>(Grid->GetClass(), TEXT("GapStartIndex"));
            if (!SpacingProperty || !GapProperty || !GapStartProperty) return false;
            const float Spacing = SpacingProperty->GetPropertyValue_InContainer(Grid);
            const float Gap = GapProperty->GetPropertyValue_InContainer(Grid);
            const int32 GapStart = GapStartProperty->GetPropertyValue_InContainer(Grid);
            const auto Position = [Grid, Spacing, Gap, GapStart](FIntPoint Coord)
            {
                return Grid->GetActorLocation() + FVector(-Coord.X * Spacing, Coord.Y * Spacing + (Coord.Y >= GapStart ? Gap : 0.0f), 100.0f);
            };
            // PIE pauses the editor's automatic navigation rebuild before world duplication.
            // PIE가 월드 복제 전에 에디터의 자동 내비게이션 재생성을 멈추므로 먼저 완료를 기다립니다.
            return NavigationReady(World, Position(It->PlayerCoords[1]), Position(UnconfirmedCoord), nullptr);
        }
        Diagnostic = TEXT("Saved Gameplay has no usable arena/grid.");
        return false;
    }

    bool ServerNavigationReady(AUnitBase* Unit, FIntPoint Coord)
    {
        ACombatGridTile* Tile = Combat->GetTileByCoord(Coord);
        if (!Tile || !Unit->GetCharacterMovement() || Unit->GetCharacterMovement()->MovementMode != MOVE_Walking) return false;
        FVector Goal = Tile->GetActorLocation();
        Goal.Z = Unit->GetActorLocation().Z;
        return NavigationReady(Server.Get(), Unit->GetActorLocation(), Goal, Unit);
    }

    void ClearSessionReferences()
    {
        Server.Reset();
        ClientWorld.Reset();
        Host.Reset();
        Remote.Reset();
        Client.Reset();
        Combat.Reset();
        Encounter.Reset();
        Run.Reset();
        HostUnit.Reset();
        Guest.Reset();
        Enemy.Reset();
    }

    void Advance(EStep Next)
    {
        Step = Next;
        StepStarted = FPlatformTime::Seconds();
        Test->AddInfo(FString::Printf(TEXT("Checkpoint PIE step %d, restoring=%d."), static_cast<int32>(Step), bRestoring));
    }

    bool CloseSession(bool bRestart = false)
    {
        RemoveActionObserver();
        bRestartAfterClose = bRestart;
        Advance(EStep::Closing);
        GEditor->RequestEndPlayMap();
        return false;
    }

    FAutomationTestBase* Test;
    ERunMode Mode;
    FString Slot;
    EOpponentSourceChange OpponentChange;
    FName OpponentSlot;
    FPartySnapshot OriginalOpponent;
    FPartySnapshot ReplacementOpponent;
    FSoftObjectPath OriginalOpponentCatalog;
    EStep Step = EStep::Launch;
    bool bRestoring = false;
    bool bRestartAfterClose = false;
    bool bPreserveWriterFile = false;
    double StepStarted;
    FString Diagnostic;
    FIntPoint MoveCoord = FIntPoint(2, 1);
    FIntPoint UnconfirmedCoord = FIntPoint(3, 1);
    FGuid HostCharacter;
    FGuid GuestCharacter;
    FGuid OldCombatId;
    FGuid OldBindingId;
    TSet<FGuid> OldRuntimeIds;
    FCombatActionRequest OldRequest;
    int64 PendingSequence = 0;
    float EnemyHPBeforeSkill = 0.0f;
    FCombatCheckpointData Expected;
    FDelegateHandle ActionObserver;
    TOptional<EUnitActionResult> ActionResult;
    TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
    TWeakObjectPtr<UWorld> Server;
    TWeakObjectPtr<UWorld> ClientWorld;
    TWeakObjectPtr<AGameplayPlayerController> Host;
    TWeakObjectPtr<AGameplayPlayerController> Remote;
    TWeakObjectPtr<AGameplayPlayerController> Client;
    TWeakObjectPtr<ACombatManager> Combat;
    TWeakObjectPtr<AEncounterManager> Encounter;
    TWeakObjectPtr<URunStateSubsystem> Run;
    TWeakObjectPtr<AUnitBase> HostUnit;
    TWeakObjectPtr<AUnitBase> Guest;
    TWeakObjectPtr<AUnitBase> Enemy;
    TWeakObjectPtr<AUnitBase> OldGuest;
    TWeakObjectPtr<AUnitBase> OldHost;
    TWeakObjectPtr<AUnitBase> ObservedUnit;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointSessionTest, "ProjectA.Coop.CheckpointSessionRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointSessionTest::RunTest(const FString& Parameters)
{
    CombatCheckpointPIETests::EOpponentSourceChange OpponentChange = CombatCheckpointPIETests::EOpponentSourceChange::None;
    FString OpponentOption;
    if (FParse::Value(FCommandLine::Get(), TEXT("T14CheckpointOpponent="), OpponentOption))
    {
        if (OpponentOption.Equals(TEXT("Replace"), ESearchCase::IgnoreCase)) OpponentChange = CombatCheckpointPIETests::EOpponentSourceChange::Replace;
        else if (OpponentOption.Equals(TEXT("Delete"), ESearchCase::IgnoreCase)) OpponentChange = CombatCheckpointPIETests::EOpponentSourceChange::Delete;
        else
        {
            AddError(TEXT("T14CheckpointOpponent must be Replace or Delete."));
            return false;
        }
        AddInfo(TEXT("Testing actual frozen-opponent restoration after source Snapshot change: ") + OpponentOption);
    }
    const FString Slot = TEXT("T14_CombatPIE_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatCheckpointPIETests::FCheckpointSessions>(this, CombatCheckpointPIETests::ERunMode::SessionRestart, Slot, OpponentChange));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointProcessTest, "ProjectA.Persistence.CombatProcessRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointProcessTest::RunTest(const FString& Parameters)
{
    const bool bWrite = FParse::Param(FCommandLine::Get(), TEXT("T14WriteCombatCheckpoint"));
    const bool bRead = FParse::Param(FCommandLine::Get(), TEXT("T14ReadCombatCheckpoint"));
    if (!bWrite && !bRead)
    {
        AddInfo(TEXT("Independent combat process restart is opt-in; CheckpointSessionRestart covers two complete PIE sessions."));
        return true;
    }
    FString Slot;
    if (!TestTrue(TEXT("Process restart requires one mode and an isolated explicit slot."), bWrite != bRead && FParse::Value(FCommandLine::Get(), TEXT("T14CheckpointSlot="), Slot) && Slot.StartsWith(TEXT("T14_CombatProcess_")) && FRunCheckpointStorage::IsSafeSlotName(Slot))) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatCheckpointPIETests::FCheckpointSessions>(this, bWrite ? CombatCheckpointPIETests::ERunMode::ProcessWriter : CombatCheckpointPIETests::ERunMode::ProcessReader, Slot));
    return true;
}

#endif
