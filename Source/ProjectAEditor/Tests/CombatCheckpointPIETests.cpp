#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Combat/AI/PartyAutoCombatComponent.h"
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
#include "GameFramework/PlayerState.h"
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
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
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
    AIHumanRequest,
    AIActing,
    ExtraTurn,
    ExtraOwnerRejected,
    ExtraItem,
    ExtraTurnEnded,
    Disconnected,
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

struct FPeer
{
    int32 PIEInstance = INDEX_NONE;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AGameplayPlayerController> Client;
    TWeakObjectPtr<AGameplayPlayerController> Remote;
};

class FCheckpointSessions : public IAutomationLatentCommand
{
public:
    FCheckpointSessions(FAutomationTestBase* InTest, ERunMode InMode, FString InSlot, EOpponentSourceChange InOpponentChange = EOpponentSourceChange::None, bool bInPartyAI = false, int32 InParticipantCount = 2) : Test(InTest), Mode(InMode), Slot(MoveTemp(InSlot)), OpponentChange(InOpponentChange), bPartyAI(bInPartyAI), ParticipantCount(InParticipantCount), bRestoring(InMode == ERunMode::ProcessReader), StepStarted(FPlatformTime::Seconds())
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
                    for (const TWeakObjectPtr<AUnitBase>& Unit : OldUnits) Test->TestFalse(TEXT("Every participant and opponent actor from the old session was destroyed."), Unit.IsValid());
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
            PlaySettings->SetPlayNumberOfClients(ParticipantCount);
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
        if (Step == EStep::Disconnected)
        {
            return ObserveDisconnectedSession();
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
        if (ParticipantCount > 2 && !AllPeersMatch()) return false;
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
            ExtraParticipant = 2;
            Advance(ParticipantCount > 2 ? EStep::ExtraTurn : EStep::Confirmed);
        }
        else if (Step == EStep::ExtraTurn)
        {
            AUnitBase* Unit = PartyUnits.IsValidIndex(ExtraParticipant) ? PartyUnits[ExtraParticipant].Get() : nullptr;
            AGameplayPlayerController* Owner = Peers.IsValidIndex(ExtraParticipant - 1) ? Peers[ExtraParticipant - 1].Client.Get() : nullptr;
            if (!ClientResponseArrived() || !Unit || Combat->GetCurrentUnit() != Unit || !Owner || !Owner->CanUseActiveUnitAction()) return false;
            if (!CheckAccepted()) return CloseSession();
            ExtraHPBeforeItem = Unit->GetAttributeSet()->GetHP();
            ExtraItemsBefore = Unit->HealingItemCount;
            // The first guest sends a real wrong-owner RPC for each additional participant's active unit.
            // 첫 게스트가 추가 참가자의 활성 유닛마다 실제 소유권 위반 RPC를 전송합니다.
            if (!Send(Client.Get(), ECombatActionKind::EndTurn)) return CloseSession();
            Advance(EStep::ExtraOwnerRejected);
        }
        else if (Step == EStep::ExtraOwnerRejected)
        {
            if (!ClientResponseArrived()) return false;
            if (!Test->TestTrue(TEXT("Another participant cannot end the additional owner's turn over RPC."), Client->GetLastCombatActionResponse().Result == ECombatRequestResult::NotOwner)) return CloseSession();
            AGameplayPlayerController* Owner = Peers[ExtraParticipant - 1].Client.Get();
            if (bRestoring)
            {
                if (!Send(Owner, ECombatActionKind::EndTurn)) return CloseSession();
                Advance(EStep::ExtraTurnEnded);
                return false;
            }
            ACombatManager* OwnerCombat = Owner->GetCombatManager();
            AUnitBase* OwnerUnit = OwnerCombat ? OwnerCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(PartyUnits[ExtraParticipant].Get())) : nullptr;
            if (!OwnerUnit || !Send(Owner, ECombatActionKind::HealingItem, nullptr, OwnerUnit->GetCurrentTile())) return CloseSession();
            Advance(EStep::ExtraItem);
        }
        else if (Step == EStep::ExtraItem)
        {
            if (!ClientResponseArrived()) return false;
            AUnitBase* Unit = PartyUnits[ExtraParticipant].Get();
            if (!CheckAccepted() || !Test->TestTrue(TEXT("The additional connection heals its own unit and spends its own item."), Unit->GetAttributeSet()->GetHP() > ExtraHPBeforeItem && Unit->HealingItemCount == ExtraItemsBefore - 1)) return CloseSession();
            if (!Send(Peers[ExtraParticipant - 1].Client.Get(), ECombatActionKind::EndTurn)) return CloseSession();
            Advance(EStep::ExtraTurnEnded);
        }
        else if (Step == EStep::ExtraTurnEnded)
        {
            if (!ClientResponseArrived() || Combat->GetCurrentUnit() == PartyUnits[ExtraParticipant].Get()) return false;
            if (!CheckAccepted()) return CloseSession();
            ++ExtraParticipant;
            if (bRestoring && ExtraParticipant == ParticipantCount)
            {
                ReadAndCompareCommitted();
                return CloseSession();
            }
            Advance(ExtraParticipant < ParticipantCount ? EStep::ExtraTurn : EStep::Confirmed);
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
                OldUnits.Add(Unit);
            }
            for (const FPeer& Peer : Peers) OldBindings.Add(Peer.Client->GetParticipantBindingId());
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
            if (bPartyAI && ParticipantCount == 2 && !PrepareAIRecord()) return CloseSession();
            if (ParticipantCount > 2)
            {
                UNetConnection* Connection = Peers.Last().Remote.IsValid() ? Peers.Last().Remote->GetNetConnection() : nullptr;
                if (!Test->TestNotNull(TEXT("The final participant has an actual connection to interrupt."), Connection)) return CloseSession();
                // Expect exactly the engine error caused by deliberately closing this participant's connection.
                // 이 참가자의 연결을 의도적으로 종료하여 발생하는 엔진 오류만 정확히 한 번 예상합니다.
                Test->AddExpectedErrorPlain(TEXT("UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = "), EAutomationExpectedErrorFlags::Contains, 1);
                Connection->Close();
                Connection->FlushNet(true);
                Advance(EStep::Disconnected);
                return false;
            }
            bPreserveWriterFile = Mode == ERunMode::ProcessWriter;
            return CloseSession(Mode == ERunMode::SessionRestart);
        }
        else if (Step == EStep::Restored)
        {
            if (!ViewsMatch(ClientCombat) || !UnitsMatch(ClientCombat) || (!bPartyAI && !Client->CanUseActiveUnitAction()) || !FindHUD(Host.Get()) || !FindHUD(Client.Get()))
            {
                return false;
            }
            if (!CheckRuntimeAgainstCheckpoint(Combat.Get(), true) || !CheckRuntimeAgainstCheckpoint(ClientCombat, true))
            {
                return CloseSession();
            }
            for (int32 Index = 0; Index < Peers.Num(); ++Index)
            {
                AGameplayPlayerController* PeerController = Peers[Index].Client.Get();
                if (!CheckRuntimeAgainstCheckpoint(PeerController->GetCombatManager(), true)) return CloseSession();
                if (OldBindings.IsValidIndex(Index)) Test->TestTrue(TEXT("Every returning original connection receives a fresh participant binding."), PeerController->GetParticipantBindingId().IsValid() && PeerController->GetParticipantBindingId() != OldBindings[Index]);
                Test->TestTrue(TEXT("Every restored client retains an empty authoritative Run subsystem."), Peers[Index].World->GetGameInstance()->GetSubsystem<URunStateSubsystem>()->GetPhase() == ERunPhase::None);
                Test->TestTrue(TEXT("Every restored HUD shows the same turn and permits only the active Human owner."), FindHUD(PeerController) && FindHUD(PeerController)->GetTurnInfoText().EqualTo(FindHUD(Host.Get())->GetTurnInfoText()) && PeerController->CanUseActiveUnitAction() == (Index == 0 && !bPartyAI));
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
            Test->TestTrue(TEXT("Restored HUD input respects ownership and server AI control."), HostEndTurn && ClientEndTurn && !HostEndTurn->GetIsEnabled() && ClientEndTurn->GetIsEnabled() == !bPartyAI);
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
            if (bPartyAI)
            {
                Test->TestFalse(TEXT("AI guest's original owner cannot directly control the restored unit."), Client->CanUseActiveUnitAction());
                if (!Send(Client.Get(), ECombatActionKind::EndTurn)) return CloseSession();
                Advance(EStep::AIHumanRequest);
                return false;
            }
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
            if (ParticipantCount > 2)
            {
                ExtraParticipant = 2;
                Advance(EStep::ExtraTurn);
                return false;
            }
            return CloseSession();
        }
        else if (Step == EStep::AIHumanRequest)
        {
            if (!ClientResponseArrived()) return false;
            if (!Test->TestTrue(TEXT("The owning client RPC cannot directly end an AI-controlled turn."), Client->GetLastCombatActionResponse().Result == ECombatRequestResult::NotOwner)) return CloseSession();
            APlayerUnit* AIUnit = Cast<APlayerUnit>(Guest.Get());
            UPartyAutoCombatComponent* Brain = AIUnit ? AIUnit->FindComponentByClass<UPartyAutoCombatComponent>() : nullptr;
            if (!Test->TestNotNull(TEXT("The restored player keeps its server AI component."), Brain)) return CloseSession();
            Brain->StartTurn();
            Advance(EStep::AIActing);
        }
        else if (Step == EStep::AIActing)
        {
            if (bWaitingForExtraAITurn)
            {
                if (!ClientResponseArrived()) return false;
                if (!CheckAccepted()) return CloseSession();
                bWaitingForExtraAITurn = false;
            }
            for (int32 Index = 2; Index < ParticipantCount; ++Index)
            {
                if (Combat->GetCurrentUnit() == PartyUnits[Index].Get())
                {
                    AGameplayPlayerController* Owner = Peers[Index - 1].Client.Get();
                    if (!Owner->CanUseActiveUnitAction()) return false;
                    if (!Send(Owner, ECombatActionKind::EndTurn)) return CloseSession();
                    bWaitingForExtraAITurn = true;
                    return false;
                }
            }
            const bool bViewsSynced = ViewsMatch(ClientCombat);
            const bool bUnitsSynced = UnitsMatch(ClientCombat);
            Diagnostic = FString::Printf(TEXT("AITurn=%d Current=%s HostHP=%.1f GuestHP=%.1f EnemyHP=%.1f Items=%d Skills=%d Moves=%d GuestCoord=%s ViewSync=%d UnitSync=%d Result=%d"), Combat->GetTurnSerial(), *GetNameSafe(Combat->GetCurrentUnit()), HostUnit->GetAttributeSet()->GetHP(), Guest->GetAttributeSet()->GetHP(), Enemy->GetAttributeSet()->GetHP(), AIItems, AISkills, AIMoves, Guest->GetCurrentTile() ? *Guest->GetCurrentTile()->GridCoord.ToString() : TEXT("None"), bViewsSynced, bUnitsSynced, static_cast<int32>(Combat->GetCombatResult()));
            if (LastObservedAITurn != Combat->GetTurnSerial())
            {
                LastObservedAITurn = Combat->GetTurnSerial();
                Test->AddInfo(Diagnostic);
            }
            if (!HostUnit->IsUnitAlive() || !Guest->IsUnitAlive() || Combat->GetCombatResult() != ECombatResult::None)
            {
                Test->AddError(TEXT("AI fixture ended or lost a required living participant before returning to the Host: ") + Diagnostic);
                return CloseSession();
            }
            if (bAIActionFailed)
            {
                Test->AddError(TEXT("Restored party AI failed an actual navigation, item or skill action: ") + Diagnostic);
                return CloseSession();
            }
            if (AIItems == 0 || AISkills == 0 || AIMoves == 0 || Combat->GetCurrentUnit() != HostUnit.Get() || !bViewsSynced || !bUnitsSynced) return false;
            APlayerUnit* AIUnit = Cast<APlayerUnit>(Guest.Get());
            APlayerUnit* ClientAI = Cast<APlayerUnit>(ClientCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(Guest.Get())));
            Test->TestTrue(TEXT("Actual server AI heals itself and consumes its own item."), bAIHealingObserved && AIItems == 1 && Guest->HealingItemCount == 0);
            Test->TestTrue(TEXT("Actual server AI damages an opponent and successfully moves."), AISkills > 0 && AIMoves > 0 && Enemy->GetAttributeSet()->GetHP() < EnemyHPBeforeSkill);
            Test->TestTrue(TEXT("Server and client retain AI mode and the original player team."), AIUnit && ClientAI && AIUnit->IsServerAIControlled() && ClientAI->IsServerAIControlled() && AIUnit->GetTeam() == ETeam::Player && ClientAI->GetTeam() == ETeam::Player);
            Test->TestTrue(TEXT("AI execution does not transfer original ownership."), Combat->GetCharacterId(Guest.Get()) == GuestCharacter && Combat->GetOwnerAccountId(Guest.Get()) == Expected.Identity.OriginalParticipants[1].AccountId && ClientCombat->GetOwnerAccountId(ClientAI) == Expected.Identity.OriginalParticipants[1].AccountId);
            Test->TestTrue(TEXT("AI turn and opponent turn finish without human guest input."), Combat->GetTurnSerial() > Expected.CompletedTurnSerial + 2 && Host->CanUseActiveUnitAction() && !Client->CanUseActiveUnitAction());
            const FCombatCheckpointData& NewCheckpoint = Run->GetCombatCheckpoint();
            const FCombatCheckpointUnit* SavedAI = NewCheckpoint.Units.FindByPredicate([this](const FCombatCheckpointUnit& Unit) { return Unit.CharacterId == GuestCharacter; });
            Test->TestTrue(TEXT("The next confirmed checkpoint retains AI mode and original owner."), NewCheckpoint.Revision > Expected.Revision && NewCheckpoint.SchemaVersion == 2 && SavedAI && SavedAI->PartyControlMode == EPartyControlMode::ServerAI && SavedAI->OwnerAccountId == Expected.Identity.OriginalParticipants[1].AccountId);
            Test->TestTrue(TEXT("Both HUDs show the same post-AI turn."), FindHUD(Host.Get()) && FindHUD(Client.Get()) && FindHUD(Host.Get())->GetTurnInfoText().EqualTo(FindHUD(Client.Get())->GetTurnInfoText()));
            ReadAndCompareCommitted();
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
        for (int32 Index = 0; Index < ParticipantCount; ++Index)
        {
            FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("CheckpointPIEFixture");
            Participant.AccountId.Subject = Index == 0 ? TEXT("OriginalHost") : Index == 1 ? TEXT("OriginalGuest") : FString::Printf(TEXT("OriginalGuest%d"), Index);
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
        if (ParticipantCount > 2 && !ConfigureExpandedArena(true)) return false;
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
        if (ParticipantCount > 2)
        {
            for (int32 Index = 2; Index < ParticipantCount; ++Index)
            {
                AUnitBase* Unit = PartyUnits[Index].Get();
                const TArray<TObjectPtr<USkillDefinitionDataAsset>> ExtraSkills = Unit->GetEquippedSkillDataAssets();
                if (!Test->TestTrue(TEXT("Additional owners keep authored skills and enough resources for their item RPC."), Unit->ConfigureProfession(Unit->GetAttributeSet()->GetMaxHP(), 4, 2, ExtraSkills))) return false;
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Unit->GetAttributeSet()->GetMaxHP() - 40.0f);
                Unit->HealingItemCount = 1;
                Unit->ForceNetUpdate();
            }
            // A real server death exercises corpse restoration without ending the surviving opponent encounter.
            // 생존한 상대의 전투를 끝내지 않으면서 실제 서버 사망으로 시체 복구를 검증합니다.
            Corpse->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 0.0f);
            Corpse->Die();
            if (!Test->TestTrue(TEXT("The expanded fixture includes an actual dead opponent without tile occupancy."), !Corpse->IsUnitAlive() && !Corpse->GetCurrentTile())) return false;
        }
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
        if (ParticipantCount > 2 && !ConfigureExpandedArena(false)) return false;
        if (!Test->TestTrue(TEXT("The new server loads a combat checkpoint from disk."), Run->LoadCheckpoint(Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        if (Mode == ERunMode::ProcessReader)
        {
            Expected = Run->GetCombatCheckpoint();
        }
        if (!Test->TestTrue(TEXT("No live memory state is needed to load the same confirmed body."), SameCheckpoint(Expected, Run->GetCombatCheckpoint())) || !Test->TestEqual(TEXT("The restored record contains every original participant."), Expected.Identity.OriginalParticipants.Num(), ParticipantCount))
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
        if (!ResolveFixtureUnits()) return false;
        if (bPartyAI)
        {
            APlayerUnit* AIUnit = Cast<APlayerUnit>(Guest.Get());
            UPartyAutoCombatComponent* Brain = AIUnit ? AIUnit->FindComponentByClass<UPartyAutoCombatComponent>() : nullptr;
            if (!Test->TestTrue(TEXT("The disk record restores the original guest as server AI."), Expected.SchemaVersion == 2 && AIUnit && AIUnit->IsServerAIControlled() && Brain)) return false;
            // Hold only the initial AI decision until both replicated HUDs expose the control lock.
            // 복제된 양쪽 HUD의 조작 잠금을 확인할 때까지 최초 AI 판단만 대기시킵니다.
            Brain->Stop();
            EnemyHPBeforeSkill = Enemy->GetAttributeSet()->GetHP();
            ObservePartyAI();
        }
        return true;
    }

    bool PrepareAIRecord()
    {
        FText Error;
        TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot, Error)));
        if (!Test->TestNotNull(TEXT("AI opt-in starts with an actual confirmed disk checkpoint."), Save.Get())) return false;
        FCombatCheckpointData& Checkpoint = Save->CombatCheckpoint;
        FCombatCheckpointUnit* AIUnit = Checkpoint.Units.FindByPredicate([this](const FCombatCheckpointUnit& Unit) { return Unit.CharacterId == GuestCharacter; });
        if (!Test->TestTrue(TEXT("AI fixture uses Unknown legacy consent without prior approval."), AIUnit && Checkpoint.Identity.OriginalParticipants[1].AIConsent == ERunAIConsent::Unknown && Checkpoint.Identity.OriginalParticipants[1].ConsentPolicyVersion == 0)) return false;
        // This explicit test fixture selects AI before the new session; it is not a live transition API.
        // 새 세션 전에 AI를 선택하는 명시적 테스트 데이터이며 실행 중 전환 API가 아닙니다.
        Checkpoint.SchemaVersion = 2;
        AIUnit->PartyControlMode = EPartyControlMode::ServerAI;
        AIUnit->HP = FMath::Max(1.0f, AIUnit->MaxHP - 40.0f);
        AIUnit->HealingItemCount = 1;
        AIUnit->HealingItemAmount = 40.0f;
        // Keep the passive Host and opponent alive until the complete guest AI turn can be observed.
        // 아군 AI의 전체 턴을 관찰할 때까지 수동 Host와 상대가 생존하도록 테스트 체력을 설정합니다.
        for (FCombatCheckpointUnit& Unit : Checkpoint.Units)
        {
            if (!Unit.bDead && (Unit.Team == ETeam::Enemy || Unit.CharacterId == HostCharacter))
            {
                Unit.MaxHP = 1000.0f;
                Unit.HP = 1000.0f;
            }
        }
        for (FRunPartyMember& Member : Save->Party)
        {
            const FCombatCheckpointUnit* Unit = Checkpoint.Units.FindByPredicate([&Member](const FCombatCheckpointUnit& Entry) { return Entry.CharacterId == Member.CharacterId; });
            if (Unit) Member.CurrentHP = Unit->HP;
        }
        if (!Test->TestTrue(TEXT("The writer persists AI mode before ending its process/session."), FRunCheckpointStorage::Save(Save.Get(), Slot, Error))) return false;
        Expected = Checkpoint;
        return true;
    }

    void ObservePartyAI()
    {
        RemoveActionObserver();
        ObservedUnit = Guest;
        AIItems = 0;
        AISkills = 0;
        AIMoves = 0;
        bAIHealingObserved = false;
        bAIActionFailed = false;
        LastObservedAITurn = INDEX_NONE;
        const float HPBefore = Guest->GetAttributeSet()->GetHP();
        ActionObserver = Guest->OnActionCompleted.AddLambda([this, HPBefore](AUnitBase* Unit, EUnitActionType Kind, EUnitActionResult Result)
        {
            bAIActionFailed |= Result != EUnitActionResult::Succeeded;
            if (Kind == EUnitActionType::Item)
            {
                ++AIItems;
                bAIHealingObserved |= Unit->GetAttributeSet()->GetHP() > HPBefore;
            }
            else if (Kind == EUnitActionType::Skill) ++AISkills;
            else if (Kind == EUnitActionType::Move) ++AIMoves;
            Test->AddInfo(FString::Printf(TEXT("Party AI completion: Kind=%d Result=%d Turn=%d HP=%.1f AP=%d SubAP=%d Coord=%s Items=%d Skills=%d Moves=%d"), static_cast<int32>(Kind), static_cast<int32>(Result), Combat.IsValid() ? Combat->GetTurnSerial() : INDEX_NONE, Unit->GetAttributeSet()->GetHP(), Unit->GetCurrentActionPoint(), Unit->GetCurrentSubActionPoint(), Unit->GetCurrentTile() ? *Unit->GetCurrentTile()->GridCoord.ToString() : TEXT("None"), AIItems, AISkills, AIMoves));
        });
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
        if (!Test->TestTrue(TEXT("The original roster matches all real test connections."), Identity.OriginalParticipants.Num() == ParticipantCount && Peers.Num() == ParticipantCount - 1 && GameMode->AssignRunParticipant(Host.Get(), Identity.HostAccountId))) return false;
        for (int32 Index = 0; Index < Peers.Num(); ++Index)
        {
            if (!Test->TestTrue(TEXT("Each known test connection receives its original account identity."), GameMode->AssignRunParticipant(Peers[Index].Remote.Get(), Identity.OriginalParticipants[Index + 1].AccountId))) return false;
        }
        return true;
    }

    bool ResolveFixtureUnits()
    {
        PartyUnits.SetNum(ParticipantCount);
        Enemy.Reset();
        Corpse.Reset();
        for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
        {
            if (Combat->GetCharacterId(Unit) == HostCharacter) HostUnit = Unit;
            else if (Combat->GetCharacterId(Unit) == GuestCharacter) Guest = Unit;
            else if (Unit->GetTeam() == ETeam::Enemy)
            {
                if (!Enemy.IsValid()) Enemy = Unit;
                else Corpse = Unit;
            }
            for (const FRunPartyMember& Member : Run->GetPartyMembers())
            {
                if (Member.CharacterId == Combat->GetCharacterId(Unit) && PartyUnits.IsValidIndex(Member.SlotIndex)) PartyUnits[Member.SlotIndex] = Unit;
            }
        }
        const bool bAllPartyPresent = !PartyUnits.ContainsByPredicate([](const TWeakObjectPtr<AUnitBase>& Unit) { return !Unit.IsValid(); });
        return Test->TestTrue(TEXT("The encounter creates every owned character and the expected opponent roster."), bAllPartyPresent && HostUnit.IsValid() && Guest.IsValid() && Enemy.IsValid() && (ParticipantCount == 2 || Corpse.IsValid()) && Encounter->GetSpawnedUnits().Num() == ParticipantCount + (ParticipantCount > 2 ? 2 : 1));
    }

    bool ConfigureExpandedArena(bool bConfigureOpponents)
    {
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        AGameplayGameState* GameState = Server->GetGameState<AGameplayGameState>();
        ACombatArena* Arena = GameState ? GameState->GetArena() : nullptr;
        if (!Test->TestTrue(TEXT("Expanded-party fixtures use an idle authored arena."), GameMode && Arena && Arena->PlayerCoords.Num() >= ParticipantCount && Encounter->GetSpawnedUnits().IsEmpty())) return false;
        // Preserve the established guest movement route while placing extra owners on unused player tiles.
        // 기존 게스트 이동 경로를 유지하고 추가 소유자는 사용하지 않는 아군 타일에 배치합니다.
        for (int32 Index = 2; Index < ParticipantCount; ++Index) Arena->PlayerCoords[Index] = FIntPoint(Index, 0);
        if (!bConfigureOpponents) return true;
        TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>> Definitions = GameMode->EncounterDefinitions;
        for (TPair<FName, TObjectPtr<UEncounterDefinitionDataAsset>>& Entry : Definitions)
        {
            if (!Test->TestTrue(TEXT("The expanded fixture starts from an authored PvE enemy class."), Entry.Value && Entry.Value->OpponentSnapshotSlot.IsNone() && !Entry.Value->EnemyUnitClasses.IsEmpty())) return false;
            UEncounterDefinitionDataAsset* Definition = DuplicateObject<UEncounterDefinitionDataAsset>(Entry.Value.Get(), GameMode);
            const TSubclassOf<AEnemyUnit> CorpseClass = Definition->EnemyUnitClasses[0];
            Definition->EnemyUnitClasses = {CorpseClass, CorpseClass};
            Entry.Value = Definition;
        }
        Combat->OnCombatResult.RemoveAll(Encounter.Get());
        Encounter->InitializeEncounter(Arena, Combat.Get(), GameMode->PartyDefinition, Definitions);
        return true;
    }

    bool ObserveDisconnectedSession()
    {
        if (!Server.IsValid() || !Combat.IsValid() || !Run.IsValid() || !Encounter.IsValid())
        {
            Test->AddError(TEXT("The server must survive an individual participant disconnect."));
            return CloseSession();
        }
        if (SuspensionObservedAt > 0.0 && (!SuspendedStateMatches() || !SameCheckpoint(Expected, Run->GetCombatCheckpoint())))
        {
            Test->AddError(TEXT("Disconnected combat changed its turn, action state, resources, occupancy, or confirmed checkpoint during stable observation."));
            return CloseSession();
        }
        if (Combat->IsCombatActive() || Encounter->GetFlowMessage().IsEmpty()) return false;
        for (int32 Index = 0; Index + 1 < Peers.Num(); ++Index)
        {
            AGameplayPlayerController* Controller = Peers[Index].Client.Get();
            ACombatManager* Manager = Controller ? Controller->GetCombatManager() : nullptr;
            const AGameplayGameState* State = Manager ? Manager->GetWorld()->GetGameState<AGameplayGameState>() : nullptr;
            if (!Manager || !State || !ViewsMatch(Manager) || !UnitsMatch(Manager) || Controller->CanUseActiveUnitAction() || State->GetViewState().Phase != ERunPhase::Combat || !State->GetViewState().FlowMessage.EqualTo(Encounter->GetFlowMessage())) return false;
        }
        if (SuspensionObservedAt == 0.0)
        {
            SuspendedRuntime = CaptureSuspendedState();
            SuspensionObservedAt = FPlatformTime::Seconds();
            Test->AddInfo(TEXT("All remaining clients display the suspended combat; observing unchanged state across subsequent world ticks."));
            return false;
        }
        if (FPlatformTime::Seconds() - SuspensionObservedAt < 0.75) return false;
        Test->TestTrue(TEXT("Turn, resources, occupancy, control modes, and checkpoint stay fixed after disconnect callbacks settle."), SuspendedStateMatches() && SameCheckpoint(Expected, Run->GetCombatCheckpoint()));
        Test->TestFalse(TEXT("Disconnect stops the unfinished movement without an automatic turn."), Guest->IsBusy());
        Test->TestFalse(TEXT("Disconnect prevents further Host actions."), Host->CanUseActiveUnitAction());
        Test->TestTrue(TEXT("Disconnect keeps the original Host identity, epoch, and canonical turn boundary."), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Expected.Identity, &Run->GetRunIdentity(), 0) && SameCheckpoint(Expected, Run->GetCombatCheckpoint()));
        Test->TestTrue(TEXT("Disconnect does not manufacture a combat result."), Combat->GetCombatResult() == ECombatResult::None && Run->GetPhase() == ERunPhase::Combat);
        for (const TWeakObjectPtr<AUnitBase>& Unit : PartyUnits)
        {
            const APlayerUnit* Player = Cast<APlayerUnit>(Unit.Get());
            Test->TestTrue(TEXT("A missing participant never changes any original character to AI."), Player && Player->GetPartyControlMode() == EPartyControlMode::Human);
        }
        if (!ReadAndCompareCommitted()) return CloseSession();
        Test->TestTrue(TEXT("The confirmed expanded-party record includes a real dead opponent without occupancy."), Expected.Units.ContainsByPredicate([](const FCombatCheckpointUnit& Unit) { return Unit.Team == ETeam::Enemy && Unit.bDead && Unit.HP == 0.0f && !Unit.bHasTile; }));
        if (bPartyAI && !PrepareAIRecord()) return CloseSession();
        return CloseSession(true);
    }

    FCombatCheckpointData CaptureSuspendedState() const
    {
        FCombatCheckpointData State;
        State.AttemptId = Combat->GetCombatInstanceId();
        State.CompletedTurnSerial = Combat->GetTurnSerial();
        State.NextTurnIndex = Combat->GetCurrentTurnIndex();
        for (AUnitBase* Unit : Combat->GetRegisteredUnits())
        {
            FCombatCheckpointUnit& Entry = State.Units.AddDefaulted_GetRef();
            Entry.UnitId = Combat->GetRuntimeUnitId(Unit);
            Entry.CharacterId = Combat->GetCharacterId(Unit);
            Entry.OwnerAccountId = Combat->GetOwnerAccountId(Unit);
            Entry.HP = Unit->GetAttributeSet()->GetHP();
            Entry.AP = Unit->GetCurrentActionPoint();
            Entry.SubAP = Unit->GetCurrentSubActionPoint();
            Entry.HealingItemCount = Unit->HealingItemCount;
            Entry.bDead = !Unit->IsUnitAlive();
            Entry.bHasTile = Unit->GetCurrentTile() != nullptr;
            if (Entry.bHasTile) Entry.GridCoord = Unit->GetCurrentTile()->GridCoord;
            if (const APlayerUnit* Player = Cast<APlayerUnit>(Unit)) Entry.PartyControlMode = Player->GetPartyControlMode();
        }
        return State;
    }

    bool SuspendedStateMatches() const
    {
        if (Combat->IsCombatActive() || Combat->IsAwaitingTurnCheckpoint()) return false;
        for (AUnitBase* Unit : Combat->GetRegisteredUnits())
        {
            if (!IsValid(Unit) || !Unit->GetAttributeSet() || Unit->IsBusy() || Unit->IsActiveTurn()) return false;
        }
        return SameCheckpoint(SuspendedRuntime, CaptureSuspendedState());
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
            if (const APlayerUnit* Player = Cast<APlayerUnit>(Unit)) bUnitValid &= Player->GetPartyControlMode() == Saved.PartyControlMode;
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
            if (const APlayerUnit* ServerPlayer = Cast<APlayerUnit>(ServerUnit))
            {
                const APlayerUnit* ClientPlayer = Cast<APlayerUnit>(ClientUnit);
                if (!ClientPlayer || ClientPlayer->GetPartyControlMode() != ServerPlayer->GetPartyControlMode()) return false;
            }
            ACombatGridTile* ServerTile = ServerUnit->GetCurrentTile();
            ACombatGridTile* ClientTile = ClientUnit->GetCurrentTile();
            if (ServerTile ? !ClientTile || ServerTile->GridCoord != ClientTile->GridCoord || ClientTile->GetOccupyingUnit() != ClientUnit : ClientTile != nullptr) return false;
        }
        return true;
    }

    bool ViewsMatch(ACombatManager* ClientCombat) const
    {
        const AGameplayGameState* ClientState = ClientCombat->GetWorld()->GetGameState<AGameplayGameState>();
        if (!ClientState || ClientState->GetViewState().ConfirmedCombatRevision != Run->GetCombatCheckpoint().Revision) return false;
        return ClientCombat->GetCombatInstanceId() == Combat->GetCombatInstanceId() && ClientCombat->GetRunId() == Combat->GetRunId() && ClientCombat->GetHostEpoch() == Combat->GetHostEpoch() && ClientCombat->GetTurnSerial() == Combat->GetTurnSerial() && ClientCombat->GetCombatResult() == Combat->GetCombatResult() && ClientCombat->IsCombatActive() == Combat->IsCombatActive() && ClientCombat->IsAwaitingTurnCheckpoint() == Combat->IsAwaitingTurnCheckpoint() && ClientCombat->GetRuntimeUnitId(ClientCombat->GetCurrentUnit()) == Combat->GetRuntimeUnitId(Combat->GetCurrentUnit());
    }

    bool AllPeersMatch() const
    {
        if (Peers.Num() != ParticipantCount - 1) return false;
        for (int32 Index = 0; Index < Peers.Num(); ++Index)
        {
            AGameplayPlayerController* Controller = Peers[Index].Client.Get();
            ACombatManager* Manager = Controller ? Controller->GetCombatManager() : nullptr;
            if (!Manager || !ViewsMatch(Manager) || !UnitsMatch(Manager) || !Controller->GetParticipantBindingId().IsValid() || Controller->GetBoundParticipantAccount() != Run->GetRunIdentity().OriginalParticipants[Index + 1].AccountId || !FindHUD(Controller)) return false;
        }
        return true;
    }

    bool Send(APartyPlayerController* Controller, ECombatActionKind Kind, USkillDefinitionDataAsset* Skill = nullptr, ACombatGridTile* Target = nullptr, FCombatActionRequest* OutRequest = nullptr)
    {
        FCombatActionRequest Request;
        if (!Test->TestTrue(TEXT("The fixture encodes a real controller action."), Controller->BuildCombatActionRequest(Kind, Skill, Target, Request))) return false;
        if (OutRequest) *OutRequest = Request;
        const FCombatActionResponse Response = Controller->SubmitCombatActionRequest(Request);
        if (!Controller->HasAuthority())
        {
            PendingController = Controller;
            PendingSequence = Request.RequestSequence;
            return Test->TestTrue(TEXT("Client action is sent through the owning network connection."), Response.Result == ECombatRequestResult::Pending);
        }
        return Test->TestTrue(TEXT("The Host dispatches its own action."), Response.Result == ECombatRequestResult::Accepted);
    }

    bool ClientResponseArrived() const
    {
        if (!PendingController.IsValid()) return false;
        const FCombatActionResponse& Response = PendingController->GetLastCombatActionResponse();
        return Response.RequestSequence == PendingSequence && Response.Result != ECombatRequestResult::Pending;
    }

    bool CheckAccepted()
    {
        return Test->TestTrue(TEXT("The server acknowledges the client action."), PendingController.IsValid() && PendingController->GetLastCombatActionResponse().Result == ECombatRequestResult::Accepted);
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
        TArray<FPeer> FoundPeers;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || !World->GetMapName().Contains(TEXT("Gameplay"))) continue;
            if (World->GetNetMode() == NM_ListenServer) Server = World;
            else if (World->GetNetMode() == NM_Client)
            {
                FPeer& Peer = FoundPeers.AddDefaulted_GetRef();
                Peer.PIEInstance = Context.PIEInstance;
                Peer.World = World;
                Peer.Client = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            }
        }
        if (!Server.IsValid() || FoundPeers.Num() != ParticipantCount - 1) return false;
        UNetDriver* ServerDriver = Server->GetNetDriver();
        if (!ServerDriver || ServerDriver->ClientConnections.Num() != ParticipantCount - 1) return false;
        FoundPeers.Sort([](const FPeer& Left, const FPeer& Right) { return Left.PIEInstance < Right.PIEInstance; });
        for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
        {
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(It->Get());
            if (Controller && Controller->IsLocalController()) Host = Controller;
            else if (Controller && Controller->GetNetConnection() && Controller->GetPlayerState<APlayerState>())
            {
                for (FPeer& Peer : FoundPeers)
                {
                    const APlayerState* PlayerState = Peer.Client.IsValid() ? Peer.Client->GetPlayerState<APlayerState>() : nullptr;
                    if (PlayerState && PlayerState->GetPlayerId() == Controller->GetPlayerState<APlayerState>()->GetPlayerId()) Peer.Remote = Controller;
                }
            }
        }
        TSet<AGameplayPlayerController*> UniqueRemoteControllers;
        for (const FPeer& Peer : FoundPeers)
        {
            UNetDriver* Driver = Peer.World->GetNetDriver();
            UNetConnection* RemoteConnection = Peer.Remote.IsValid() ? Peer.Remote->GetNetConnection() : nullptr;
            if (!Peer.Client.IsValid() || !Driver || Driver == ServerDriver || !Driver->ServerConnection || Driver->ServerConnection->GetConnectionState() != USOCK_Open || !RemoteConnection || RemoteConnection->GetConnectionState() != USOCK_Open || UniqueRemoteControllers.Contains(Peer.Remote.Get())) return false;
            UniqueRemoteControllers.Add(Peer.Remote.Get());
        }
        Peers = MoveTemp(FoundPeers);
        ClientWorld = Peers[0].World;
        Client = Peers[0].Client;
        Remote = Peers[0].Remote;
        AGameplayGameModeBase* GameMode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        AGameplayGameState* GameState = Server->GetGameState<AGameplayGameState>();
        if (!Client.IsValid() || !Host.IsValid() || !Remote.IsValid() || !GameMode || !GameMode->GetEncounterManager() || !GameState || !GameState->GetArena()) return false;
        Encounter = GameMode->GetEncounterManager();
        Combat = Encounter->GetCombatManager();
        Run = Server->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        Test->TestTrue(TEXT("Every original participant has a distinct connected client world and server connection."), UniqueRemoteControllers.Num() == ParticipantCount - 1 && !Client->HasAuthority());
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
        Corpse.Reset();
        PartyUnits.Reset();
        Peers.Reset();
        PendingController.Reset();
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
    bool bPartyAI = false;
    bool bWaitingForExtraAITurn = false;
    int32 ParticipantCount = 2;
    int32 ExtraParticipant = 2;
    float ExtraHPBeforeItem = 0.0f;
    int32 ExtraItemsBefore = 0;
    TArray<FPeer> Peers;
    TArray<TWeakObjectPtr<AUnitBase>> PartyUnits;
    TArray<TWeakObjectPtr<AUnitBase>> OldUnits;
    TArray<FGuid> OldBindings;
    TWeakObjectPtr<APartyPlayerController> PendingController;
    FName OpponentSlot;
    FPartySnapshot OriginalOpponent;
    FPartySnapshot ReplacementOpponent;
    FSoftObjectPath OriginalOpponentCatalog;
    EStep Step = EStep::Launch;
    bool bRestoring = false;
    bool bRestartAfterClose = false;
    bool bPreserveWriterFile = false;
    double StepStarted;
    double SuspensionObservedAt = 0.0;
    FCombatCheckpointData SuspendedRuntime;
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
    int32 AIItems = 0;
    int32 AISkills = 0;
    int32 AIMoves = 0;
    bool bAIHealingObserved = false;
    bool bAIActionFailed = false;
    int32 LastObservedAITurn = INDEX_NONE;
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
    TWeakObjectPtr<AUnitBase> Corpse;
    TWeakObjectPtr<AUnitBase> OldGuest;
    TWeakObjectPtr<AUnitBase> OldHost;
    TWeakObjectPtr<AUnitBase> ObservedUnit;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointSessionTest, "ProjectA.Coop.CheckpointSessionRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointSessionTest::RunTest(const FString& Parameters)
{
    const bool bPartyAI = FParse::Param(FCommandLine::Get(), TEXT("T14CheckpointAI"));
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
    if (bPartyAI && OpponentChange != CombatCheckpointPIETests::EOpponentSourceChange::None)
    {
        AddError(TEXT("T14CheckpointAI and T14CheckpointOpponent are separate fixture variants."));
        return false;
    }
    if (bPartyAI) AddInfo(TEXT("Testing persisted guest AI without prior consent while preserving ownership and actual two-world combat."));
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatCheckpointPIETests::FCheckpointSessions>(this, CombatCheckpointPIETests::ERunMode::SessionRestart, Slot, OpponentChange, bPartyAI));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointThreePlayersTest, "ProjectA.Coop.CheckpointSessionRestart3Players", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointThreePlayersTest::RunTest(const FString& Parameters)
{
    const bool bPartyAI = FParse::Param(FCommandLine::Get(), TEXT("T14CheckpointAI"));
    const FString Slot = TEXT("T14_CombatPIE3_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    AddInfo(TEXT("Three original participants: actual client disconnect, unchanged original Host, and a fresh fully rejoined PIE session."));
    if (bPartyAI) AddInfo(TEXT("The original guest remains connected while its persisted AI mode is restored without prior consent."));
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatCheckpointPIETests::FCheckpointSessions>(this, CombatCheckpointPIETests::ERunMode::SessionRestart, Slot, CombatCheckpointPIETests::EOpponentSourceChange::None, bPartyAI, 3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointFourPlayersTest, "ProjectA.Coop.CheckpointSessionRestart4Players", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointFourPlayersTest::RunTest(const FString& Parameters)
{
    const bool bPartyAI = FParse::Param(FCommandLine::Get(), TEXT("T14CheckpointAI"));
    const FString Slot = TEXT("T14_CombatPIE4_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    AddInfo(TEXT("Four original participants: actual client disconnect, unchanged original Host, and a fresh fully rejoined PIE session."));
    if (bPartyAI) AddInfo(TEXT("The original guest remains connected while its persisted AI mode is restored without prior consent."));
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatCheckpointPIETests::FCheckpointSessions>(this, CombatCheckpointPIETests::ERunMode::SessionRestart, Slot, CombatCheckpointPIETests::EOpponentSourceChange::None, bPartyAI, 4));
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
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<CombatCheckpointPIETests::FCheckpointSessions>(this, bWrite ? CombatCheckpointPIETests::ERunMode::ProcessWriter : CombatCheckpointPIETests::ERunMode::ProcessReader, Slot, CombatCheckpointPIETests::EOpponentSourceChange::None, FParse::Param(FCommandLine::Get(), TEXT("T14CheckpointAI"))));
    return true;
}

#endif
