#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Controller/GameplayPlayerController.h"
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
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatHUDWidget.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace ProjectACoopNetworkTests
{
template <typename T>
T* FindScreen(APartyPlayerController* Controller)
{
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Widgets, T::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        T* Screen = Cast<T>(Widget);
        if (Screen && Screen->GetWorld() == Controller->GetWorld() && Screen->GetOwningPlayer() == Controller && Screen->IsActivated())
        {
            return Screen;
        }
    }
    return nullptr;
}

// Each in-process PIE world exchanges commands and replicated actors through a separate NetDriver.
// 한 프로세스의 각 PIE 월드는 서로 다른 NetDriver로 명령과 복제 액터를 교환합니다.
class FPlayCoopNetwork : public IAutomationLatentCommand
{
public:
    explicit FPlayCoopNetwork(FAutomationTestBase* InTest, int32 InParticipantCount = 2) : Test(InTest), ParticipantCount(InParticipantCount), StageStarted(FPlatformTime::Seconds())
    {
        MoveCoord = GetMoveCoord(1);
    }

    virtual ~FPlayCoopNetwork() override
    {
        RemoveMoveObserver();
        if (GEditor && Stage != Finished && HasPIEWorld())
        {
            GEditor->RequestEndPlayMap();
        }
    }

    virtual bool Update() override
    {
        if (Stage == Ending)
        {
            if (!HasPIEWorld())
            {
                Stage = Finished;
                return true;
            }
            if (FPlatformTime::Seconds() - StageStarted > 30.0)
            {
                Test->AddError(TEXT("Co-op PIE worlds did not close after the test."));
                return true;
            }
            return false;
        }
        if (FPlatformTime::Seconds() - StageStarted > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Co-op network PIE (%d players, owner %d) timed out at stage %d; last client response=%d sequence=%lld."), ParticipantCount, ActiveRemoteIndex + 1, Stage, Client.IsValid() ? static_cast<int32>(Client->GetLastCombatActionResponse().Result) : -1, Client.IsValid() ? Client->GetLastCombatActionResponse().RequestSequence : -1));
            Test->AddError(FString::Printf(TEXT("Last %s navigation readiness: %s"), Stage == 0 ? TEXT("editor before PIE") : TEXT("server"), Stage == 0 ? *EditorNavigationStatus : *NavigationStatus));
            return EndSession();
        }
        if (Stage == 0)
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
            Advance();
            return false;
        }
        if (Stage == 1)
        {
            if (!FindConnectedWorlds())
            {
                return false;
            }
            if (!InitializeRun())
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (!bClientBindingsReady)
        {
            if (!MatchClientBindings())
            {
                return false;
            }
            bClientBindingsReady = true;
            SelectRemote(0);
        }
        if (!ServerWorld.IsValid() || !ClientWorld.IsValid() || !Host.IsValid() || !RemoteServerController.IsValid() || !Client.IsValid() || !ServerCombat.IsValid())
        {
            Test->AddError(TEXT("A required co-op world or controller disappeared during the test."));
            return EndSession();
        }
        AGameplayGameState* ServerState = ServerWorld->GetGameState<AGameplayGameState>();
        AGameplayGameState* ClientState = ClientWorld->GetGameState<AGameplayGameState>();
        ACombatManager* ClientCombat = ClientState ? ClientState->GetCombatManager() : nullptr;
        if (!ServerState || !ClientState || !ClientCombat || Client->GetCombatManager() != ClientCombat)
        {
            return false;
        }
        if (Stage == 2)
        {
            if (!AllClientsMatch() || !HUDOwnershipReady(true))
            {
                return false;
            }
            AUnitBase* ClientActive = ClientCombat->GetCurrentUnit();
            AUnitBase* ClientEnemy = ClientCombat->ResolveRuntimeUnit(EnemyUnitId);
            USkillDefinitionDataAsset* ClientSkill = ClientActive ? ClientActive->FindSkillDataByAbilityClass(ClientActive->GetDefaultAttackAbilityClass()) : nullptr;
            if (!ClientSkill || !ClientEnemy || !ClientEnemy->GetCurrentTile())
            {
                return false;
            }
            Test->TestTrue(TEXT("The client observes the original Run identity."), ClientCombat->GetRunId() == Identity.RunId);
            Test->TestTrue(TEXT("The host can act only on its own opening character."), Host->CanUseActiveUnitAction());
            Test->TestFalse(TEXT("The client cannot act on the host's character."), Client->CanUseActiveUnitAction());
            Test->TestTrue(TEXT("The client GameInstance does not become the authoritative Run store."), ClientWorld->GetGameInstance()->GetSubsystem<URunStateSubsystem>()->GetPhase() == ERunPhase::None);
            CheckHUDOwnership(true);
            const int32 ServerAP = HostUnit->GetCurrentActionPoint();
            const int32 ServerSubAP = HostUnit->GetCurrentSubActionPoint();
            const float ServerEnemyHP = EnemyUnit->GetAttributeSet()->GetHP();
            ClientActive->StartSkill(ClientSkill, ClientEnemy->GetCurrentTile());
            ClientActive->StartItemAction(ClientActive);
            ClientActive->StartMoveAction(ClientCombat->GetTileByCoord(MoveCoord));
            Test->TestFalse(TEXT("Direct client Unit calls cannot start a local combat action."), ClientActive->IsBusy());
            Test->TestEqual(TEXT("Direct client Unit calls cannot consume server AP."), HostUnit->GetCurrentActionPoint(), ServerAP);
            Test->TestEqual(TEXT("Direct client Unit calls cannot consume server SubAP."), HostUnit->GetCurrentSubActionPoint(), ServerSubAP);
            Test->TestEqual(TEXT("Direct client Unit calls cannot apply server damage."), EnemyUnit->GetAttributeSet()->GetHP(), ServerEnemyHP);
            FCombatActionRequest NotOwner;
            if (!Test->TestTrue(TEXT("The remote client can encode a request for a server-side ownership rejection."), Client->BuildCombatActionRequest(ECombatActionKind::EndTurn, nullptr, nullptr, NotOwner)))
            {
                return EndSession();
            }
            TurnBefore = ServerCombat->GetTurnSerial();
            QueueRequest(NotOwner);
            Advance();
            return false;
        }
        if (Stage == 3)
        {
            if (!ResponseArrived())
            {
                return false;
            }
            Test->TestTrue(TEXT("The server rejects the guest's real RPC for the host character."), Client->GetLastCombatActionResponse().Result == ECombatRequestResult::NotOwner);
            Test->TestEqual(TEXT("Rejected ownership leaves the server turn unchanged."), ServerCombat->GetTurnSerial(), TurnBefore);
            if (ActiveRemoteIndex + 1 < ParticipantCount - 1)
            {
                SelectRemote(ActiveRemoteIndex + 1);
                SetStage(2);
                return false;
            }
            SelectRemote(0);
            if (!ClickHUD(Host.Get(), TEXT("Button_EndTurn")))
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (Stage == 4)
        {
            if (ServerCombat->GetCurrentUnit() != GuestUnit.Get() || !AllClientsMatch() || !Client->CanUseActiveUnitAction() || !HUDOwnershipReady(false))
            {
                return false;
            }
            CheckHUDOwnership(false);
            Test->TestFalse(TEXT("Listen host has no human-control exception for the guest character."), Host->CanUseActiveUnitAction());
            if (!ClickHUD(Client.Get(), TEXT("Button_Item")) || !CapturePendingResponse())
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (Stage == 5)
        {
            if (!ResponseArrived() || !AllClientsMatch())
            {
                return false;
            }
            if (Client->GetLastCombatActionResponse().Result == ECombatRequestResult::Accepted && !ServerNavigationReady())
            {
                return false;
            }
            if (!CheckAccepted(TEXT("Client HUD potion")))
            {
                return EndSession();
            }
            Test->TestEqual(TEXT("Client potion heals the server unit."), GuestUnit->GetAttributeSet()->GetHP(), GuestUnit->GetAttributeSet()->GetMaxHP());
            Test->TestEqual(TEXT("Client potion consumes exactly one server item."), GuestUnit->HealingItemCount, 0);
            Test->TestEqual(TEXT("Client potion consumes one server SubAP."), GuestUnit->GetCurrentSubActionPoint(), 1);
            ACombatGridTile* MoveTile = ClientCombat->GetTileByCoord(MoveCoord);
            if (!Test->TestNotNull(TEXT("The client resolves the replicated move tile."), MoveTile) || !ClickHUD(Client.Get(), TEXT("Button_Move")))
            {
                return EndSession();
            }
            MoveResult.Reset();
            MoveObserver = GuestUnit->OnActionCompleted.AddLambda([this](AUnitBase*, EUnitActionType Action, EUnitActionResult Result)
            {
                if (Action == EUnitActionType::Move)
                {
                    MoveResult = Result;
                }
            });
            Client->HandleTileClicked(MoveTile);
            if (!CapturePendingResponse())
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (Stage == 6)
        {
            if (MoveResult.IsSet() && MoveResult.GetValue() != EUnitActionResult::Succeeded)
            {
                Test->AddError(FString::Printf(TEXT("Server movement completed unsuccessfully: result=%d; %s"), static_cast<int32>(MoveResult.GetValue()), *NavigationStatus));
                return EndSession();
            }
            if (ResponseArrived() && Client->GetLastCombatActionResponse().Result != ECombatRequestResult::Accepted)
            {
                CheckAccepted(TEXT("Client movement"));
                return EndSession();
            }
            if (!ResponseArrived() || !MoveResult.IsSet() || GuestUnit->IsBusy() || !GuestUnit->GetCurrentTile() || GuestUnit->GetCurrentTile()->GridCoord != MoveCoord || !AllClientsMatch())
            {
                return false;
            }
            if (!CheckAccepted(TEXT("Client movement")))
            {
                return EndSession();
            }
            RemoveMoveObserver();
            Test->TestEqual(TEXT("Client movement consumes the remaining server SubAP."), GuestUnit->GetCurrentSubActionPoint(), 0);
            AUnitBase* ClientGuest = ClientCombat->ResolveRuntimeUnit(GuestUnitId);
            AUnitBase* ClientEnemy = ClientCombat->ResolveRuntimeUnit(EnemyUnitId);
            UCombatHUDWidget* HUD = FindScreen<UCombatHUDWidget>(Client.Get());
            UHorizontalBox* SkillList = HUD ? Cast<UHorizontalBox>(HUD->GetWidgetFromName(TEXT("SkillList"))) : nullptr;
            UButton* SkillButton = SkillList && SkillList->GetChildrenCount() > 0 ? Cast<UButton>(SkillList->GetChildAt(0)) : nullptr;
            if (!Test->TestTrue(TEXT("Replicated equipped skills populate the client HUD."), ClientGuest && ClientEnemy && SkillButton && SkillButton->GetIsEnabled()))
            {
                return EndSession();
            }
            SkillButton->OnClicked.Broadcast();
            if (!Test->TestTrue(TEXT("The client HUD selects its actual equipped skill."), Client->IsSkillInputMode() && Client->GetPendingSkillData()))
            {
                return EndSession();
            }
            HPBeforeSkill = EnemyUnit->GetAttributeSet()->GetHP();
            APBeforeSkill = GuestUnit->GetCurrentActionPoint();
            SkillCost = Client->GetPendingSkillData()->ActionPointCost;
            if (!Client->BuildCombatActionRequest(ECombatActionKind::Skill, Client->GetPendingSkillData(), ClientEnemy->GetCurrentTile(), SkillRequest))
            {
                Test->AddError(TEXT("The client could not encode its equipped skill command."));
                return EndSession();
            }
            QueueRequest(SkillRequest);
            Advance();
            return false;
        }
        if (Stage == 7)
        {
            if (!ResponseArrived() || GuestUnit->IsBusy() || !AllClientsMatch())
            {
                return false;
            }
            if (!CheckAccepted(TEXT("Client equipped skill")))
            {
                return EndSession();
            }
            Test->TestTrue(TEXT("The remote skill causes actual server GAS damage."), EnemyUnit->GetAttributeSet()->GetHP() < HPBeforeSkill);
            Test->TestEqual(TEXT("The remote skill charges server AP once."), GuestUnit->GetCurrentActionPoint(), APBeforeSkill - SkillCost);
            HPAfterSkill = EnemyUnit->GetAttributeSet()->GetHP();
            APAfterSkill = GuestUnit->GetCurrentActionPoint();
            Client->SubmitCombatActionRequest(SkillRequest);
            Test->TestTrue(TEXT("Retransmitting an acknowledged request preserves settled client feedback."), Client->GetLastCombatActionResponse().Result == ECombatRequestResult::Accepted);
            Advance();
            return false;
        }
        if (Stage == 8)
        {
            const FCombatActionResponse& ServerResponse = RemoteServerController->GetLastCombatActionResponse();
            if (ServerResponse.RequestSequence != SkillRequest.RequestSequence || ServerResponse.Result != ECombatRequestResult::DuplicateRequest)
            {
                return false;
            }
            Test->TestEqual(TEXT("A duplicate real client RPC cannot damage again."), EnemyUnit->GetAttributeSet()->GetHP(), HPAfterSkill);
            Test->TestEqual(TEXT("A duplicate real client RPC cannot charge AP again."), GuestUnit->GetCurrentActionPoint(), APAfterSkill);
            TurnBefore = ServerCombat->GetTurnSerial();
            if (!ClickHUD(Client.Get(), TEXT("Button_EndTurn")) || !CapturePendingResponse())
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (Stage == 9)
        {
            const bool bMoreRemoteTurns = ActiveRemoteIndex + 1 < ParticipantCount - 1;
            AUnitBase* ExpectedNextUnit = bMoreRemoteTurns ? PartyUnits[ActiveRemoteIndex + 2].Get() : HostUnit.Get();
            if (!ResponseArrived() || ServerCombat->GetTurnSerial() <= TurnBefore || ServerCombat->GetCurrentUnit() != ExpectedNextUnit || !AllClientsMatch())
            {
                return false;
            }
            if (!CheckAccepted(TEXT("Client end turn")))
            {
                return EndSession();
            }
            Test->AddInfo(FString::Printf(TEXT("Owner %d completed real potion, movement, skill, replay rejection and end-turn RPCs; all %d clients match."), ActiveRemoteIndex + 1, ParticipantCount - 1));
            if (bMoreRemoteTurns)
            {
                SelectRemote(ActiveRemoteIndex + 1);
                SetStage(4);
                return false;
            }
            // Lethal fixture damage verifies replicated death independently of balance and AI targeting.
            // 치명적인 테스트 피해로 밸런스와 AI 대상 선택에 의존하지 않고 사망 복제를 검증합니다.
            for (int32 OwnerIndex = 1; OwnerIndex < ParticipantCount; ++OwnerIndex)
            {
                if (!Test->TestTrue(FString::Printf(TEXT("Server fixture applies lethal damage to owner %d's character."), OwnerIndex), UCombatEffectLibrary::ApplyDamageToUnit(EnemyUnit.Get(), PartyUnits[OwnerIndex].Get(), UGE_Damage::StaticClass(), 100000.0f)))
                {
                    return EndSession();
                }
            }
            Advance();
            return false;
        }
        if (Stage == 10)
        {
            if (!AllClientsMatch())
            {
                return false;
            }
            for (int32 OwnerIndex = 1; OwnerIndex < ParticipantCount; ++OwnerIndex)
            {
                Test->TestFalse(FString::Printf(TEXT("Every world observes owner %d's character death."), OwnerIndex), PartyUnits[OwnerIndex]->IsUnitAlive());
                for (const TWeakObjectPtr<AGameplayPlayerController>& RemoteClient : RemoteClients)
                {
                    ACombatGridTile* PreviousTile = RemoteClient->GetCombatManager()->GetTileByCoord(GetMoveCoord(OwnerIndex));
                    Test->TestTrue(TEXT("Each client death clears the previous grid occupant."), PreviousTile && !PreviousTile->GetOccupyingUnit());
                }
            }
            if (!Test->TestTrue(TEXT("Server fixture applies lethal damage to the last enemy."), UCombatEffectLibrary::ApplyDamageToUnit(HostUnit.Get(), EnemyUnit.Get(), UGE_Damage::StaticClass(), 100000.0f)))
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (Stage == 11)
        {
            if (ServerState->GetViewState().Phase != ERunPhase::Result || !AllClientsMatch() || !AllClientsShowResult() || !FindScreen<UEncounterResultWidget>(Host.Get()))
            {
                return false;
            }
            Test->TestTrue(TEXT("The server confirms Victory."), ServerState->GetViewState().LastResult == ECombatResult::Victory);
            Test->TestFalse(TEXT("The result locks host combat input."), Host->CanUseActiveUnitAction());
            UTextBlock* HostResult = Cast<UTextBlock>(FindScreen<UEncounterResultWidget>(Host.Get())->GetWidgetFromName(TEXT("Text_Result")));
            for (const TWeakObjectPtr<AGameplayPlayerController>& RemoteClient : RemoteClients)
            {
                Test->TestTrue(TEXT("Every client receives Victory in both gameplay and combat state."), RemoteClient->GetWorld()->GetGameState<AGameplayGameState>()->GetViewState().LastResult == ECombatResult::Victory && RemoteClient->GetCombatManager()->GetCombatResult() == ECombatResult::Victory);
                Test->TestFalse(TEXT("The result locks each client's combat input."), RemoteClient->CanUseActiveUnitAction());
                UTextBlock* ClientResult = Cast<UTextBlock>(FindScreen<UEncounterResultWidget>(RemoteClient.Get())->GetWidgetFromName(TEXT("Text_Result")));
                Test->TestTrue(TEXT("Every result HUD displays the same final text."), HostResult && ClientResult && HostResult->GetText().ToString() == ClientResult->GetText().ToString());
            }
            // The fixture invokes the server transition without defining a multiplayer node-voting policy.
            // 멀티플레이 노드 선택 정책을 정하지 않고 테스트가 서버 전환을 직접 실행합니다.
            if (!Test->TestTrue(TEXT("The server fixture continues after result inspection."), Encounter->ContinueRun()))
            {
                return EndSession();
            }
            Advance();
            return false;
        }
        if (Stage == 12)
        {
            if (ServerState->GetViewState().Phase != ERunPhase::Map || !ServerCombat->GetRegisteredUnits().IsEmpty() || !AllClientsCleanedUp())
            {
                return false;
            }
            Test->TestTrue(TEXT("Server cleanup releases encounter actors."), Encounter->GetSpawnedUnits().IsEmpty());
            for (const TWeakObjectPtr<AGameplayPlayerController>& RemoteClient : RemoteClients)
            {
                ACombatArena* Arena = RemoteClient->GetWorld()->GetGameState<AGameplayGameState>()->GetArena();
                for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Arena->Grid->TileMap)
                {
                    Test->TestNull(TEXT("Every client cleanup leaves no grid occupancy."), Entry.Value->GetOccupyingUnit());
                }
            }
            return EndSession();
        }
        return false;
    }

private:
    bool EditorNavigationReady()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (!World || !World->GetMapName().Contains(TEXT("Gameplay")))
        {
            EditorNavigationStatus = TEXT("The saved Gameplay editor world is not loaded.");
            return false;
        }
        ACombatArena* Arena = nullptr;
        for (TActorIterator<ACombatArena> It(World); It; ++It)
        {
            if (It->Grid && It->PlayerCoords.IsValidIndex(1))
            {
                Arena = *It;
                break;
            }
        }
        if (!Arena)
        {
            EditorNavigationStatus = TEXT("Saved Gameplay has no arena/grid with the guest formation slot.");
            return false;
        }
        ACombatGridManager* Grid = Arena->Grid;
        const FFloatProperty* SpacingProperty = FindFProperty<FFloatProperty>(Grid->GetClass(), TEXT("Spacing"));
        const FFloatProperty* GapProperty = FindFProperty<FFloatProperty>(Grid->GetClass(), TEXT("GapSpacing"));
        const FIntProperty* GapStartProperty = FindFProperty<FIntProperty>(Grid->GetClass(), TEXT("GapStartIndex"));
        if (!SpacingProperty || !GapProperty || !GapStartProperty)
        {
            EditorNavigationStatus = TEXT("Saved grid spacing properties could not be read.");
            return false;
        }
        const float Spacing = SpacingProperty->GetPropertyValue_InContainer(Grid);
        const float Gap = GapProperty->GetPropertyValue_InContainer(Grid);
        const int32 GapStart = GapStartProperty->GetPropertyValue_InContainer(Grid);
        const auto SpawnPosition = [Grid, Spacing, Gap, GapStart](FIntPoint Coord)
        {
            return Grid->GetActorLocation() + FVector(-Coord.X * Spacing, Coord.Y * Spacing + (Coord.Y >= GapStart ? Gap : 0.0f), 100.0f);
        };
        const FVector Start = SpawnPosition(Arena->PlayerCoords[1]);
        const FVector Goal = SpawnPosition(MoveCoord);
        UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
        ANavigationData* NavData = Navigation ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        const bool bBuildingOrLocked = UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);
        const int32 RemainingTasks = Navigation ? Navigation->GetNumRemainingBuildTasks() : -1;
        FNavLocation ProjectedStart;
        FNavLocation ProjectedGoal;
        const bool bStartProjected = NavData && !bBuildingOrLocked && Navigation->ProjectPointToNavigation(Start, ProjectedStart, NavData->GetDefaultQueryExtent(), NavData);
        const bool bGoalProjected = NavData && !bBuildingOrLocked && Navigation->ProjectPointToNavigation(Goal, ProjectedGoal, NavData->GetDefaultQueryExtent(), NavData);
        UNavigationPath* Path = bStartProjected && bGoalProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, Goal, NavData) : nullptr;
        const bool bPathValid = Path && Path->IsValid() && !Path->IsPartial();
        EditorNavigationStatus = FString::Printf(TEXT("World=%s DefaultData=%s BuildingOrLocked=%d RemainingTasks=%d Start=%s Goal=%s StartProjected=%d GoalProjected=%d CompletePath=%d"), *GetNameSafe(World), *GetNameSafe(NavData), bBuildingOrLocked, RemainingTasks, *Start.ToString(), *Goal.ToString(), bStartProjected, bGoalProjected, bPathValid);
        // PIE pauses editor navigation generation, so duplicate only after the normal map-load rebuild finishes.
        // PIE가 에디터 내비게이션 생성을 멈추므로 맵 로드의 자동 재생성이 끝난 뒤에만 복제합니다.
        const bool bReady = NavData && !bBuildingOrLocked && RemainingTasks == 0 && bPathValid;
        if (bReady)
        {
            UE_LOG(LogTemp, Display, TEXT("[CoopPIEEditorNavigation] %s"), *EditorNavigationStatus);
        }
        return bReady;
    }

    bool HasPIEWorld() const
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
            {
                return true;
            }
        }
        return false;
    }

    bool FindConnectedWorlds()
    {
        ObservedClients.Reset();
        ServerRemoteControllers.Reset();
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || !World->GetMapName().Contains(TEXT("Gameplay")))
            {
                continue;
            }
            if (World->GetNetMode() == NM_ListenServer)
            {
                ServerWorld = World;
            }
            else if (World->GetNetMode() == NM_Client)
            {
                if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(World->GetFirstPlayerController()))
                {
                    ObservedClients.Add(Controller);
                }
            }
        }
        if (!ServerWorld.IsValid() || ObservedClients.Num() != ParticipantCount - 1)
        {
            return false;
        }
        UNetDriver* ServerDriver = ServerWorld->GetNetDriver();
        if (!ServerDriver || ServerDriver->ClientConnections.Num() != ParticipantCount - 1)
        {
            return false;
        }
        for (UNetConnection* Connection : ServerDriver->ClientConnections)
        {
            AGameplayPlayerController* Controller = Connection ? Cast<AGameplayPlayerController>(Connection->OwningActor) : nullptr;
            if (!Controller || Connection->GetConnectionState() != USOCK_Open)
            {
                return false;
            }
            ServerRemoteControllers.Add(Controller);
        }
        for (const TWeakObjectPtr<AGameplayPlayerController>& RemoteClient : ObservedClients)
        {
            UNetDriver* Driver = RemoteClient->GetWorld()->GetNetDriver();
            if (!Driver || !Driver->ServerConnection || Driver->ServerConnection->GetConnectionState() != USOCK_Open)
            {
                return false;
            }
        }
        for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
        {
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(It->Get());
            if (Controller && Controller->IsLocalController())
            {
                Host = Controller;
            }
        }
        AGameplayGameModeBase* Mode = ServerWorld->GetAuthGameMode<AGameplayGameModeBase>();
        AGameplayGameState* State = ServerWorld->GetGameState<AGameplayGameState>();
        if (!Host.IsValid() || !Mode || !Mode->GetEncounterManager() || !State || !State->GetArena() || Mode->GetNumPlayers() != ParticipantCount)
        {
            return false;
        }
        Test->TestEqual(TEXT("The server counts every requested player connection."), Mode->GetNumPlayers(), ParticipantCount);
        if (!Test->TestNotNull(TEXT("Gameplay creates the official GameSession admission controller."), Mode->GameSession.Get()))
        {
            return false;
        }
        Test->TestEqual(TEXT("The configured GameSession supports at most four players."), Mode->GameSession->MaxPlayers, 4);
        // Exercise Unreal's admission rule without launching an additional fifth PIE instance.
        // 다섯 번째 PIE 인스턴스를 추가로 실행하지 않고 언리얼의 입장 규칙을 호출합니다.
        Test->TestTrue(TEXT("GameSession admits a remaining player only below its four-player capacity."), Mode->GameSession->ApproveLogin(TEXT("")).IsEmpty() == (ParticipantCount < 4));
        for (const TWeakObjectPtr<AGameplayPlayerController>& RemoteClient : ObservedClients)
        {
            Test->TestTrue(TEXT("Every client world owns a separate connected NetDriver."), ServerWorld.Get() != RemoteClient->GetWorld() && ServerDriver != RemoteClient->GetWorld()->GetNetDriver());
            Test->TestFalse(TEXT("Commands originate from actual non-authoritative client controllers."), RemoteClient->HasAuthority());
        }
        return true;
    }

    bool MatchClientBindings()
    {
        RemoteClients.Reset();
        TSet<AGameplayPlayerController*> MatchedClients;
        for (int32 Index = 0; Index < ParticipantCount - 1; ++Index)
        {
            AGameplayPlayerController* ServerController = ServerRemoteControllers[Index].Get();
            if (!ServerController)
            {
                return false;
            }
            AGameplayPlayerController* MatchingClient = nullptr;
            for (const TWeakObjectPtr<AGameplayPlayerController>& Candidate : ObservedClients)
            {
                if (Candidate.IsValid() && Candidate->GetBoundParticipantAccount() == Identity.OriginalParticipants[Index + 1].AccountId && Candidate->GetParticipantBindingId().IsValid() && Candidate->GetParticipantBindingId() == ServerController->GetParticipantBindingId())
                {
                    MatchingClient = Candidate.Get();
                    break;
                }
            }
            if (!MatchingClient || MatchedClients.Contains(MatchingClient))
            {
                return false;
            }
            MatchedClients.Add(MatchingClient);
            RemoteClients.Add(MatchingClient);
        }
        Test->TestEqual(TEXT("Every explicitly assigned account resolves to a unique replicated client binding."), MatchedClients.Num(), ParticipantCount - 1);
        return true;
    }

    FIntPoint GetMoveCoord(int32 OwnerIndex) const
    {
        // Keep the two-player route; larger parties move to their own empty rear tile.
        // 2인 이동 경로는 유지하며 더 큰 파티는 각자 비어 있는 후열 타일로 이동합니다.
        return ParticipantCount == 2 ? FIntPoint(2, 1) : FIntPoint(OwnerIndex, 0);
    }

    void SelectRemote(int32 Index)
    {
        RemoveMoveObserver();
        ActiveRemoteIndex = Index;
        RemoteServerController = ServerRemoteControllers[Index];
        Client = RemoteClients[Index];
        ClientWorld = Client->GetWorld();
        GuestUnit = PartyUnits[Index + 1];
        GuestUnitId = ServerCombat->GetRuntimeUnitId(GuestUnit.Get());
        MoveCoord = GetMoveCoord(Index + 1);
        MoveResult.Reset();
        PendingSequence = 0;
        bLoggedNavigationCoverage = false;
    }

    bool InitializeRun()
    {
        AGameplayGameModeBase* Mode = ServerWorld->GetAuthGameMode<AGameplayGameModeBase>();
        AGameplayGameState* State = ServerWorld->GetGameState<AGameplayGameState>();
        URunStateSubsystem* Run = ServerWorld->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        Encounter = Mode->GetEncounterManager();
        ServerCombat = Encounter->GetCombatManager();
        FProfessionDefinition Hunter;
        if (!Test->TestTrue(TEXT("Saved Gameplay supplies a valid Hunter profession."), Mode->PartyDefinition && Mode->PartyDefinition->ResolveProfession(TEXT("Hunter"), Hunter)))
        {
            return false;
        }
        PartyCatalog.Reset(DuplicateObject<UPartyDefinitionDataAsset>(Mode->PartyDefinition, GetTransientPackage()));
        Hunter.bUseUnitClassDefaults = false;
        Hunter.ActionPoints = 4;
        Hunter.SubActionPoints = 2;
        if (ParticipantCount > 2)
        {
            // Keep the longer fixture alive for every owner's command round without changing authored balance.
            // 원본 밸런스를 바꾸지 않고 모든 소유자의 명령 차례가 끝날 때까지 긴 테스트를 유지합니다.
            Hunter.MaxHP = 1000.0f;
        }
        PartyCatalog->Professions.Add(TEXT("Hunter"), Hunter);
        Run->PartyDefinition = PartyCatalog.Get();
        Identity.Origin = ERunIdentityOrigin::AccountProvider;
        Identity.RunId = FGuid::NewGuid();
        Identity.HostEpoch = 1;
        TArray<FRunPartyMember> Party;
        for (int32 Index = 0; Index < ParticipantCount; ++Index)
        {
            FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("CoopPIEFixture");
            Participant.AccountId.Subject = Index == 0 ? TEXT("ExplicitHostOwner") : FString::Printf(TEXT("ExplicitGuestOwner%d"), Index);
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
        if (!Test->TestTrue(TEXT("The server initializes the Run with every explicit owner."), Run->InitializeRunWithIdentity(Party, Identity, Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        // The fixture explicitly maps accounts to known connections; connection order is not persisted as identity.
        // 테스트가 알려진 연결에 계정을 명시적으로 배정하며 접속 순서를 영구 식별자로 저장하지 않습니다.
        if (!Test->TestTrue(TEXT("The server assigns the fixture host account."), Mode->AssignRunParticipant(Host.Get(), Identity.OriginalParticipants[0].AccountId)))
        {
            return false;
        }
        for (int32 Index = 0; Index < ParticipantCount - 1; ++Index)
        {
            if (!Test->TestTrue(FString::Printf(TEXT("The server explicitly assigns owner %d to its fixture connection."), Index + 1), Mode->AssignRunParticipant(ServerRemoteControllers[Index].Get(), Identity.OriginalParticipants[Index + 1].AccountId)))
            {
                return false;
            }
        }
        ServerCombat->OnCombatResult.RemoveAll(Encounter.Get());
        Encounter->InitializeEncounter(State->GetArena(), ServerCombat.Get(), PartyCatalog.Get(), Mode->EncounterDefinitions);
        if (!Test->TestTrue(TEXT("The server fixture starts the first saved Gameplay encounter."), Encounter->RequestStartNode(Run->GetNodes()[0].NodeId)))
        {
            Test->AddError(Encounter->GetFlowMessage().ToString());
            return false;
        }
        PartyUnits.SetNum(ParticipantCount);
        for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
        {
            if (Unit->GetTeam() == ETeam::Enemy)
            {
                EnemyUnit = Unit;
            }
            else
            {
                for (int32 OwnerIndex = 0; OwnerIndex < ParticipantCount; ++OwnerIndex)
                {
                    if (ServerCombat->GetOwnerAccountId(Unit) == Identity.OriginalParticipants[OwnerIndex].AccountId)
                    {
                        PartyUnits[OwnerIndex] = Unit;
                        Test->TestTrue(TEXT("A spawned character preserves its original owner and CharacterId."), ServerCombat->GetCharacterId(Unit) == Party[OwnerIndex].CharacterId);
                    }
                }
            }
        }
        bool bAllOwnersSpawned = true;
        for (const TWeakObjectPtr<AUnitBase>& Unit : PartyUnits)
        {
            bAllOwnersSpawned &= Unit.IsValid();
        }
        if (!Test->TestTrue(TEXT("The fixture creates all owned characters and one opponent."), EnemyUnit.IsValid() && Encounter->GetSpawnedUnits().Num() == ParticipantCount + 1 && bAllOwnersSpawned))
        {
            return false;
        }
        HostUnit = PartyUnits[0];
        EnemyUnitId = ServerCombat->GetRuntimeUnitId(EnemyUnit.Get());
        for (int32 OwnerIndex = 1; OwnerIndex < ParticipantCount; ++OwnerIndex)
        {
            AUnitBase* Unit = PartyUnits[OwnerIndex].Get();
            Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), Unit->GetAttributeSet()->GetMaxHP() - 40.0f);
            Unit->ForceNetUpdate();
        }
        if (ParticipantCount > 2)
        {
            EnemyUnit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), 1000.0f);
            EnemyUnit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 1000.0f);
            EnemyUnit->ForceNetUpdate();
        }
        return true;
    }

    bool ViewsMatch(ACombatManager* ClientCombat) const
    {
        return ClientCombat->GetCombatInstanceId() == ServerCombat->GetCombatInstanceId() && ClientCombat->GetRunId() == ServerCombat->GetRunId() && ClientCombat->GetHostEpoch() == ServerCombat->GetHostEpoch() && ClientCombat->GetTurnSerial() == ServerCombat->GetTurnSerial() && ClientCombat->GetCombatResult() == ServerCombat->GetCombatResult() && ClientCombat->IsCombatActive() == ServerCombat->IsCombatActive() && ClientCombat->GetRuntimeUnitId(ClientCombat->GetCurrentUnit()) == ServerCombat->GetRuntimeUnitId(ServerCombat->GetCurrentUnit());
    }

    bool AllClientsMatch() const
    {
        AGameplayGameState* ServerState = ServerWorld->GetGameState<AGameplayGameState>();
        ACombatArena* ServerArena = ServerState ? ServerState->GetArena() : nullptr;
        if (!ServerArena || !ServerArena->Grid || RemoteClients.Num() != ParticipantCount - 1)
        {
            return false;
        }
        for (int32 Index = 0; Index < RemoteClients.Num(); ++Index)
        {
            AGameplayPlayerController* Controller = RemoteClients[Index].Get();
            AGameplayGameState* State = Controller ? Controller->GetWorld()->GetGameState<AGameplayGameState>() : nullptr;
            ACombatManager* Combat = State ? State->GetCombatManager() : nullptr;
            ACombatArena* Arena = State ? State->GetArena() : nullptr;
            if (!Combat || !Arena || !Arena->Grid || Controller->GetCombatManager() != Combat || Controller->GetBoundParticipantAccount() != Identity.OriginalParticipants[Index + 1].AccountId || Controller->GetParticipantBindingId() != ServerRemoteControllers[Index]->GetParticipantBindingId() || !ViewsMatch(Combat) || Combat->GetRegisteredUnits().Num() != ServerCombat->GetRegisteredUnits().Num() || State->GetViewState().Phase != ServerState->GetViewState().Phase || State->GetViewState().LastResult != ServerState->GetViewState().LastResult || Arena->Grid->TileMap.Num() != ServerArena->Grid->TileMap.Num())
            {
                return false;
            }
            for (AUnitBase* Unit : ServerCombat->GetRegisteredUnits())
            {
                if (!UnitMatches(Unit, Combat))
                {
                    return false;
                }
            }
            // Compare empty tiles too so a stale occupant cannot hide outside the units' current tiles.
            // 유닛의 현재 타일 밖에 남은 점유자도 확인하도록 빈 타일까지 비교합니다.
            for (const TPair<FIntPoint, ACombatGridTile*>& Entry : ServerArena->Grid->TileMap)
            {
                ACombatGridTile* ClientTile = Combat->GetTileByCoord(Entry.Key);
                AUnitBase* ServerOccupant = Entry.Value ? Entry.Value->GetOccupyingUnit() : nullptr;
                AUnitBase* ClientOccupant = ClientTile ? ClientTile->GetOccupyingUnit() : nullptr;
                if (!ClientTile || (ServerOccupant == nullptr) != (ClientOccupant == nullptr) || Combat->GetRuntimeUnitId(ClientOccupant) != ServerCombat->GetRuntimeUnitId(ServerOccupant))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool AllClientsShowResult() const
    {
        for (const TWeakObjectPtr<AGameplayPlayerController>& Controller : RemoteClients)
        {
            if (!Controller.IsValid() || !FindScreen<UEncounterResultWidget>(Controller.Get()))
            {
                return false;
            }
        }
        return true;
    }

    bool AllClientsCleanedUp() const
    {
        for (const TWeakObjectPtr<AGameplayPlayerController>& Controller : RemoteClients)
        {
            AGameplayGameState* State = Controller.IsValid() ? Controller->GetWorld()->GetGameState<AGameplayGameState>() : nullptr;
            ACombatManager* Combat = State ? State->GetCombatManager() : nullptr;
            ACombatArena* Arena = State ? State->GetArena() : nullptr;
            if (!Combat || !Arena || !Arena->Grid || State->GetViewState().Phase != ERunPhase::Map || !Combat->GetRegisteredUnits().IsEmpty() || !FindScreen<URunMapWidget>(Controller.Get()))
            {
                return false;
            }
            for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Arena->Grid->TileMap)
            {
                if (!Entry.Value || Entry.Value->GetOccupyingUnit())
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool ServerNavigationReady()
    {
        UWorld* World = ServerWorld.Get();
        UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
        ANavigationData* NavData = Navigation ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        ACombatGridTile* GoalTile = ServerCombat->GetTileByCoord(MoveCoord);
        UCharacterMovementComponent* Movement = GuestUnit->GetCharacterMovement();
        const bool bBuildingOrLocked = UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);
        FVector Goal = GoalTile ? GoalTile->GetActorLocation() : FVector::ZeroVector;
        Goal.Z = GuestUnit->GetActorLocation().Z;
        const FVector QueryExtent = NavData ? NavData->GetDefaultQueryExtent() : FVector::ZeroVector;
        FNavLocation ProjectedGoal;
        const bool bGoalProjected = NavData && !bBuildingOrLocked && Navigation->ProjectPointToNavigation(Goal, ProjectedGoal, QueryExtent, NavData);
        // Wait for the normal server world to become ready before issuing the single movement request.
        // 서버 월드가 정상적으로 준비될 때까지 기다린 뒤 이동 요청을 한 번만 전송합니다.
        UNavigationPath* Path = NavData && !bBuildingOrLocked && GoalTile ? UNavigationSystemV1::FindPathToLocationSynchronously(World, GuestUnit->GetActorLocation(), Goal, GuestUnit.Get()) : nullptr;
        const bool bPathValid = Path && Path->IsValid();
        const bool bPartial = Path && Path->IsPartial();
        const bool bWalking = Movement && Movement->MovementMode == MOVE_Walking;
        if (!bLoggedNavigationCoverage && NavData && !bBuildingOrLocked && bWalking)
        {
            bLoggedNavigationCoverage = true;
            // Inspect every arena coordinate without changing the requested destination or navigation state.
            // 요청 목적지나 내비게이션 상태를 바꾸지 않고 아레나의 모든 좌표를 조사합니다.
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    ACombatGridTile* Tile = ServerCombat->GetTileByCoord(FIntPoint(Row, Column));
                    if (!Tile)
                    {
                        UE_LOG(LogTemp, Display, TEXT("[CoopPIENavigationCoverage] Coord=(%d,%d) Tile=missing"), Row, Column);
                        continue;
                    }
                    FVector TileGoal = Tile->GetActorLocation();
                    TileGoal.Z = GuestUnit->GetActorLocation().Z;
                    FNavLocation TileProjection;
                    const bool bProjected = Navigation->ProjectPointToNavigation(TileGoal, TileProjection, QueryExtent, NavData);
                    UNavigationPath* TilePath = UNavigationSystemV1::FindPathToLocationSynchronously(World, GuestUnit->GetActorLocation(), TileGoal, GuestUnit.Get());
                    UE_LOG(LogTemp, Display, TEXT("[CoopPIENavigationCoverage] Coord=(%d,%d) Tile=%s Goal=%s Projected=%d ProjectedGoal=%s PathValid=%d Partial=%d"), Row, Column, *Tile->GetActorLocation().ToString(), *TileGoal.ToString(), bProjected, *TileProjection.Location.ToString(), TilePath && TilePath->IsValid(), TilePath && TilePath->IsPartial());
                }
            }
        }
        NavigationStatus = FString::Printf(TEXT("System=%s DefaultData=%s BuildingOrLocked=%d PathValid=%d Partial=%d MovementMode=%d Guest=%s Goal=%s QueryExtent=%s GoalProjected=%d ProjectedGoal=%s"), *GetNameSafe(Navigation), *GetNameSafe(NavData), bBuildingOrLocked, bPathValid, bPartial, Movement ? static_cast<int32>(Movement->MovementMode) : -1, *GuestUnit->GetActorLocation().ToString(), *Goal.ToString(), *QueryExtent.ToString(), bGoalProjected, *ProjectedGoal.Location.ToString());
        const bool bReady = NavData && !bBuildingOrLocked && bPathValid && !bPartial && bWalking && bGoalProjected;
        if (bReady)
        {
            UE_LOG(LogTemp, Display, TEXT("[CoopPIENavigation] %s"), *NavigationStatus);
        }
        return bReady;
    }

    void RemoveMoveObserver()
    {
        if (GuestUnit.IsValid() && MoveObserver.IsValid())
        {
            GuestUnit->OnActionCompleted.Remove(MoveObserver);
        }
        MoveObserver.Reset();
    }

    bool UnitMatches(AUnitBase* ServerUnit, ACombatManager* ClientCombat) const
    {
        if (!IsValid(ServerUnit))
        {
            return false;
        }
        AUnitBase* ClientUnit = ClientCombat->ResolveRuntimeUnit(ServerCombat->GetRuntimeUnitId(ServerUnit));
        if (!ClientUnit || !ClientUnit->GetAttributeSet() || ClientUnit->IsUnitAlive() != ServerUnit->IsUnitAlive() || ClientUnit->IsBusy() != ServerUnit->IsBusy() || ClientUnit->GetTeam() != ServerUnit->GetTeam() || ClientUnit->GetCurrentActionPoint() != ServerUnit->GetCurrentActionPoint() || ClientUnit->GetCurrentSubActionPoint() != ServerUnit->GetCurrentSubActionPoint() || ClientUnit->HealingItemCount != ServerUnit->HealingItemCount || !FMath::IsNearlyEqual(ClientUnit->GetAttributeSet()->GetHP(), ServerUnit->GetAttributeSet()->GetHP()) || !FMath::IsNearlyEqual(ClientUnit->GetAttributeSet()->GetMaxHP(), ServerUnit->GetAttributeSet()->GetMaxHP()) || ClientCombat->GetOwnerAccountId(ClientUnit) != ServerCombat->GetOwnerAccountId(ServerUnit) || ClientCombat->GetCharacterId(ClientUnit) != ServerCombat->GetCharacterId(ServerUnit))
        {
            return false;
        }
        ACombatGridTile* ServerTile = ServerUnit->GetCurrentTile();
        ACombatGridTile* ClientTile = ClientUnit->GetCurrentTile();
        return ServerTile ? ClientTile && ServerTile->GridCoord == ClientTile->GridCoord && ClientTile->GetOccupyingUnit() == ClientUnit : !ClientTile;
    }

    void CheckHUDOwnership(bool bHostTurn)
    {
        UCombatHUDWidget* HostHUD = FindScreen<UCombatHUDWidget>(Host.Get());
        UButton* HostEnd = HostHUD ? Cast<UButton>(HostHUD->GetWidgetFromName(TEXT("Button_EndTurn"))) : nullptr;
        Test->TestTrue(TEXT("The host's end-turn control follows ownership."), HostEnd && HostEnd->GetIsEnabled() == bHostTurn);
        for (int32 Index = 0; Index < RemoteClients.Num(); ++Index)
        {
            UCombatHUDWidget* ClientHUD = FindScreen<UCombatHUDWidget>(RemoteClients[Index].Get());
            UButton* ClientEnd = ClientHUD ? Cast<UButton>(ClientHUD->GetWidgetFromName(TEXT("Button_EndTurn"))) : nullptr;
            const bool bOwnerTurn = !bHostTurn && Index == ActiveRemoteIndex;
            Test->TestTrue(TEXT("Every HUD shows the same replicated turn text."), HostHUD && ClientHUD && HostHUD->GetTurnInfoText().ToString() == ClientHUD->GetTurnInfoText().ToString());
            Test->TestTrue(TEXT("Only the active owner sees an enabled end-turn control."), ClientEnd && ClientEnd->GetIsEnabled() == bOwnerTurn);
            Test->TestTrue(TEXT("Each client can act only during its own character's turn."), RemoteClients[Index]->CanUseActiveUnitAction() == bOwnerTurn);
        }
    }

    bool HUDOwnershipReady(bool bHostTurn) const
    {
        UCombatHUDWidget* HostHUD = FindScreen<UCombatHUDWidget>(Host.Get());
        UButton* HostEnd = HostHUD ? Cast<UButton>(HostHUD->GetWidgetFromName(TEXT("Button_EndTurn"))) : nullptr;
        if (!HostEnd || HostEnd->GetIsEnabled() != bHostTurn)
        {
            return false;
        }
        for (int32 Index = 0; Index < RemoteClients.Num(); ++Index)
        {
            UCombatHUDWidget* ClientHUD = FindScreen<UCombatHUDWidget>(RemoteClients[Index].Get());
            UButton* ClientEnd = ClientHUD ? Cast<UButton>(ClientHUD->GetWidgetFromName(TEXT("Button_EndTurn"))) : nullptr;
            if (!ClientEnd || ClientEnd->GetIsEnabled() != (!bHostTurn && Index == ActiveRemoteIndex) || HostHUD->GetTurnInfoText().ToString() != ClientHUD->GetTurnInfoText().ToString())
            {
                return false;
            }
        }
        return true;
    }

    bool ClickHUD(APartyPlayerController* Controller, FName ButtonName)
    {
        UCombatHUDWidget* HUD = FindScreen<UCombatHUDWidget>(Controller);
        UButton* Button = HUD ? Cast<UButton>(HUD->GetWidgetFromName(ButtonName)) : nullptr;
        if (!Test->TestTrue(FString::Printf(TEXT("HUD control %s is available to its owner."), *ButtonName.ToString()), Button && Button->GetIsEnabled()))
        {
            return false;
        }
        Button->OnClicked.Broadcast();
        return true;
    }

    void QueueRequest(const FCombatActionRequest& Request)
    {
        PendingSequence = Request.RequestSequence;
        Test->TestTrue(TEXT("Remote submission queues an RPC instead of executing locally."), Client->SubmitCombatActionRequest(Request).Result == ECombatRequestResult::Pending);
    }

    bool CapturePendingResponse()
    {
        const FCombatActionResponse& Response = Client->GetLastCombatActionResponse();
        PendingSequence = Response.RequestSequence;
        return Test->TestTrue(TEXT("Client HUD dispatch queues a valid network request."), Response.Result == ECombatRequestResult::Pending && Response.RequestSequence > 0 && Response.CombatInstanceId == ServerCombat->GetCombatInstanceId());
    }

    bool ResponseArrived() const
    {
        const FCombatActionResponse& Response = Client->GetLastCombatActionResponse();
        return Response.RequestSequence == PendingSequence && Response.Result != ECombatRequestResult::Pending;
    }

    bool CheckAccepted(const TCHAR* Action)
    {
        return Test->TestTrue(FString(Action) + TEXT(" receives an accepted response over the connection."), Client->GetLastCombatActionResponse().Result == ECombatRequestResult::Accepted);
    }

    void Advance()
    {
        SetStage(Stage + 1);
    }

    void SetStage(int32 NextStage)
    {
        Stage = NextStage;
        StageStarted = FPlatformTime::Seconds();
        Test->AddInfo(FString::Printf(TEXT("Co-op network PIE (%d players, owner %d) stage %d."), ParticipantCount, ActiveRemoteIndex + 1, Stage));
    }

    bool EndSession()
    {
        RemoveMoveObserver();
        Stage = Ending;
        StageStarted = FPlatformTime::Seconds();
        GEditor->RequestEndPlayMap();
        return false;
    }

    static constexpr int32 Ending = 100;
    static constexpr int32 Finished = 101;
    FAutomationTestBase* Test;
    int32 ParticipantCount = 2;
    int32 ActiveRemoteIndex = 0;
    int32 Stage = 0;
    double StageStarted;
    int32 TurnBefore = 0;
    int32 APBeforeSkill = 0;
    int32 APAfterSkill = 0;
    int32 SkillCost = 0;
    float HPBeforeSkill = 0.0f;
    float HPAfterSkill = 0.0f;
    int64 PendingSequence = 0;
    FIntPoint MoveCoord = FIntPoint(2, 1);
    FGuid GuestUnitId;
    FGuid EnemyUnitId;
    FCombatActionRequest SkillRequest;
    FDelegateHandle MoveObserver;
    TOptional<EUnitActionResult> MoveResult;
    FString NavigationStatus = TEXT("Not evaluated yet.");
    FString EditorNavigationStatus = TEXT("Not evaluated yet.");
    bool bLoggedNavigationCoverage = false;
    bool bClientBindingsReady = false;
    FRunIdentityData Identity;
    TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
    TStrongObjectPtr<UPartyDefinitionDataAsset> PartyCatalog;
    TWeakObjectPtr<UWorld> ServerWorld;
    TWeakObjectPtr<UWorld> ClientWorld;
    TWeakObjectPtr<AGameplayPlayerController> Host;
    TWeakObjectPtr<AGameplayPlayerController> RemoteServerController;
    TWeakObjectPtr<AGameplayPlayerController> Client;
    TArray<TWeakObjectPtr<AGameplayPlayerController>> ObservedClients;
    TArray<TWeakObjectPtr<AGameplayPlayerController>> RemoteClients;
    TArray<TWeakObjectPtr<AGameplayPlayerController>> ServerRemoteControllers;
    TArray<TWeakObjectPtr<AUnitBase>> PartyUnits;
    TWeakObjectPtr<ACombatManager> ServerCombat;
    TWeakObjectPtr<AEncounterManager> Encounter;
    TWeakObjectPtr<AUnitBase> HostUnit;
    TWeakObjectPtr<AUnitBase> GuestUnit;
    TWeakObjectPtr<AUnitBase> EnemyUnit;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoopNetworkPIETest, "ProjectA.Coop.ListenServerClientCombat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoopNetworkPIETest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectACoopNetworkTests::FPlayCoopNetwork>(this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoopThreePlayerNetworkPIETest, "ProjectA.Coop.ListenServerThreePlayerCombat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoopThreePlayerNetworkPIETest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectACoopNetworkTests::FPlayCoopNetwork>(this, 3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoopFourPlayerNetworkPIETest, "ProjectA.Coop.ListenServerFourPlayerCombat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoopFourPlayerNetworkPIETest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ProjectACoopNetworkTests::FPlayCoopNetwork>(this, 4));
    return true;
}

#endif
