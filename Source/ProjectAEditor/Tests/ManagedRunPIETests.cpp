#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/Authority/LocalRunAuthorityStore.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Combat/CombatHUDWidget.h"
#include "UI/Gameplay/EncounterResultWidget.h"
#include "UI/Gameplay/RunMapWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"
#include "Unit/PlayerUnit.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace ManagedRunPIETests
{
template <typename T>
T* FindScreen(APlayerController* Controller)
{
    if (!Controller) return nullptr;
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Controller, Widgets, T::StaticClass(), false);
    for (UUserWidget* Widget : Widgets)
    {
        T* Screen = Cast<T>(Widget);
        if (Screen && Screen->GetWorld() == Controller->GetWorld() && Screen->GetOwningPlayer() == Controller && Screen->IsActivated()) return Screen;
    }
    return nullptr;
}

enum class EStep : uint8
{
    Launch,
    Connect,
    InitialMap,
    InitialCombat,
    InitialBoundary,
    Closing,
    Menu,
    MenuReady,
    MenuTravelFailed,
    Restored,
    AICombat,
    Result,
    NextMap,
    NextCombat,
    Finished
};

struct FPeer
{
    int32 PIEInstance = INDEX_NONE;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AGameplayPlayerController> Client;
    TWeakObjectPtr<AGameplayPlayerController> Remote;
};

// These opt-in sessions exercise local development authority with actual engine travel and replication.
// 선택 실행하는 이 세션은 실제 엔진 이동·복제를 사용하며 개발용 권위를 온라인 인증으로 취급하지 않습니다.
class FManagedSessions : public IAutomationLatentCommand
{
public:
    FManagedSessions(FAutomationTestBase* InTest, bool bInSolo, int32 InOriginalCount = 3) : Test(InTest), bSolo(bInSolo), OriginalCount(bInSolo ? 4 : InOriginalCount), StoreNamespace(TEXT("ManagedPIE_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(20)), StepStarted(FPlatformTime::Seconds())
    {
        Identity.SchemaVersion = 2;
        Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
        Identity.RunId = FGuid::NewGuid();
        Identity.HostEpoch = 1;
        for (int32 Index = 0; Index < OriginalCount; ++Index)
        {
            FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("Development");
            Participant.AccountId.Subject = FString::Printf(TEXT("OriginalOwner%d_%s"), Index + 1, *Identity.RunId.ToString(EGuidFormats::Digits));
            Participant.JoinOrdinal = Index + 1;
            FRunPartyMember& Member = Party.AddDefaulted_GetRef();
            Member.SlotIndex = Index;
            Member.CharacterName = FText::FromString(FString::Printf(TEXT("Original owner %d"), Index + 1));
            Member.ClassId = TEXT("Hunter");
            Member.bCreated = true;
            Member.CharacterId = FGuid::NewGuid();
            Member.OwnerAccountId = Participant.AccountId;
        }
        Identity.HostAccountId = Party[0].OwnerAccountId;
    }

    virtual ~FManagedSessions() override
    {
        ShutdownExecution();
        if (Step != EStep::Finished && GEditor && HasPIEWorld()) GEditor->RequestEndPlayMap();
        UGameplayStatics::DeleteGameInSlot(FLocalRunAuthorityStore(StoreNamespace).GetSlotName(Identity.RunId), 0);
    }

    virtual bool Update() override
    {
        if (Step == EStep::Closing)
        {
            if (HasPIEWorld())
            {
                if (FPlatformTime::Seconds() - StepStarted > 30.0)
                {
                    Test->AddError(TEXT("Managed PIE worlds did not close."));
                    return true;
                }
                return false;
            }
            if (!bRestart)
            {
                Step = EStep::Finished;
                return true;
            }
            for (const TWeakObjectPtr<AUnitBase>& Unit : OldUnits) Test->TestFalse(TEXT("The original execution no longer owns live combat actors."), Unit.IsValid());
            ClearSession();
            bRestart = false;
            bResuming = true;
            Advance(EStep::Launch);
            return false;
        }
        if (FPlatformTime::Seconds() - StepStarted > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Managed PIE timed out: solo=%d resume=%d step=%d turn=%d AI skills=%d; %s"), bSolo, bResuming, static_cast<int32>(Step), Combat.IsValid() ? Combat->GetTurnSerial() : -1, AISkills, *Diagnostic));
            return Close();
        }
        if (Step == EStep::Launch)
        {
            if (!EditorNavigationReady()) return false;
            Launch();
            Advance(bResuming && bSolo ? EStep::Menu : EStep::Connect);
            return false;
        }
        if (Step == EStep::Menu || Step == EStep::MenuReady || Step == EStep::MenuTravelFailed)
        {
            return UpdateMenu();
        }
        if (Step == EStep::Connect)
        {
            if (!FindSession()) return false;
            if (bResuming)
            {
                if (!RestoreRun()) return Close();
                Advance(EStep::Restored);
            }
            else
            {
                if (!CreateRun()) return Close();
                Advance(EStep::InitialMap);
            }
            return false;
        }
        if (Step == EStep::Restored && bSolo && !bResumeUnitsObserved)
        {
            if (!FindSession() || !Combat->IsCombatActive() || !CacheUnits()) return false;
            if (!CheckResumeIdentity()) return Close();
            ObserveAI();
            bResumeUnitsObserved = true;
        }
        if (!Run.IsValid() || !Combat.IsValid() || !Host.IsValid() || !Encounter.IsValid()) return false;
        Diagnostic = FString::Printf(TEXT("Phase=%d active=%d pending=%d checkpoint=%lld flow=%s"), static_cast<int32>(Run->GetPhase()), Combat->IsCombatActive(), Run->IsManagedResumePending(), Run->GetCombatCheckpoint().Revision, *Encounter->GetFlowMessage().ToString());
        if (Step == EStep::InitialMap)
        {
            UButton* Node = NodeButton(Host.Get(), 0);
            if (!Node || !Node->GetIsEnabled()) return false;
            Node->OnClicked.Broadcast();
            if (!Test->TestTrue(TEXT("The initial Host starts managed combat through its visible node button."), Run->GetPhase() == ERunPhase::Combat) || !CacheUnits()) return Close();
            // High runtime HP keeps the fixture alive while observing real AI decisions; authored loadouts remain unchanged.
            // 실제 AI 판단 관찰 중 생존하도록 테스트 실행 HP만 높이며 작성된 장착 데이터는 변경하지 않습니다.
            for (AUnitBase* Unit : Combat->GetRegisteredUnits())
            {
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), 1000.0f);
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 1000.0f);
                Unit->ForceNetUpdate();
            }
            PartyUnits[0]->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 400.0f);
            Advance(EStep::InitialCombat);
        }
        else if (Step == EStep::InitialCombat)
        {
            if (!AllViewsMatch() || !Host->CanUseActiveUnitAction()) return false;
            if (!EndHumanTurn(Host.Get())) return Close();
            Advance(EStep::InitialBoundary);
        }
        else if (Step == EStep::InitialBoundary)
        {
            if (!AllViewsMatch() || Combat->GetCurrentUnit() != PartyUnits[1].Get() || Run->GetCombatCheckpoint().CompletedTurnSerial != 1) return false;
            Expected = Run->GetCombatCheckpoint();
            ExpectedStamp = Run->GetManagedStamp();
            OldCombatId = Combat->GetCombatInstanceId();
            OldUnits.Reset();
            for (AUnitBase* Unit : Combat->GetRegisteredUnits()) OldUnits.Add(Unit);
            FManagedRunPreview Preview;
            FText Error;
            if (!Test->TestTrue(TEXT("The initial managed execution writes a canonical combat checkpoint."), Run->HasManagedLease() && Run->ReadManagedRun(Identity.RunId, Preview, Error) && Preview.Stamp == ExpectedStamp && Preview.bHasCombatCheckpoint && Preview.Participation.HumanParticipants.Num() == OriginalCount)) return Close();
            return Close(true);
        }
        else if (Step == EStep::Restored)
        {
            if (!AllViewsMatch()) return false;
            if (!Test->TestTrue(TEXT("Successful Gameplay restoration consumes the pending resume barrier."), Run->HasManagedLease() && !Run->IsManagedResumePending()) || !CheckModes() || !CheckHumanBindings()) return Close();
            if (!bSolo && !RejectOtherCharacter()) return Close();
            Advance(EStep::AICombat);
        }
        else if (Step == EStep::AICombat)
        {
            if (bActionFailed)
            {
                Test->AddError(TEXT("The resumed server AI failed a real combat action."));
                return Close();
            }
            if (Run->GetPhase() != ERunPhase::Combat)
            {
                Test->AddError(TEXT("The high-HP fixture ended before confirming resumed AI and a newer checkpoint."));
                return Close();
            }
            if (AISkills > 0 && !PendingController.IsValid() && Run->GetCombatCheckpoint().Revision > Expected.Revision && Combat->GetCurrentUnit() == PartyUnits[HostIndex()].Get() && AllViewsMatch())
            {
                if (!Test->TestTrue(TEXT("An absent original owner's AI performs real successful attacks."), AIHPAfterAttack < AIHPBeforeAttack) || !CheckModes() || !CheckSavedAI()) return Close();
                for (int32 Index = 0; Index < Peers.Num(); ++Index)
                {
                    if (!Test->TestTrue(TEXT("Each remaining original client receives acceptance for its own actual combat RPC."), AcceptedRemoteAccounts.Contains(Party[Index + 2].OwnerAccountId))) return Close();
                }
                for (const FPeer& Peer : Peers) Test->TestFalse(TEXT("Remaining clients cannot decide managed Run progression."), Peer.Client->CanIssueRunCommands());
                // End only the fixture encounter after natural AI execution so result and next-encounter persistence are tested.
                // 실제 AI 실행을 확인한 뒤 테스트 전투만 종료하여 결과·다음 전투의 영속 상태를 검증합니다.
                const TArray<AUnitBase*> Units = Combat->GetRegisteredUnits();
                for (AUnitBase* Unit : Units)
                {
                    if (Unit->GetTeam() == ETeam::Enemy && Unit->IsUnitAlive()) Test->TestTrue(TEXT("The server fixture completes combat through real lethal GAS damage."), UCombatEffectLibrary::ApplyDamageToUnit(PartyUnits[HostIndex()].Get(), Unit, UGE_Damage::StaticClass(), 100000.0f));
                }
                Advance(EStep::Result);
                return false;
            }
            if (!DriveHumanTurn()) return Close();
        }
        else if (Step == EStep::Result)
        {
            if (Run->GetPhase() != ERunPhase::Result || Run->GetLastResult() != ECombatResult::Victory) return false;
            UEncounterResultWidget* Result = FindScreen<UEncounterResultWidget>(Host.Get());
            UButton* Button = Result ? Cast<UButton>(Result->GetWidgetFromName(TEXT("Button_Continue"))) : nullptr;
            if (!Button || !Button->GetIsEnabled()) return false;
            for (const FPeer& Peer : Peers)
            {
                UEncounterResultWidget* ClientResult = FindScreen<UEncounterResultWidget>(Peer.Client.Get());
                UButton* ClientButton = ClientResult ? Cast<UButton>(ClientResult->GetWidgetFromName(TEXT("Button_Continue"))) : nullptr;
                if (!ClientButton || ClientButton->GetIsEnabled() || Peer.World->GetGameState<AGameplayGameState>()->GetViewState().Phase != ERunPhase::Result) return false;
                Peer.Client->RequestContinueRun();
            }
            if (!Test->TestTrue(TEXT("Client Continue attempts preserve the managed result."), Run->GetPhase() == ERunPhase::Result)) return Close();
            RemoveObservers();
            Button->OnClicked.Broadcast();
            if (!Test->TestTrue(TEXT("Current Host's result button commits the next map without losing managed authority."), Run->GetPhase() == ERunPhase::Map && Run->HasManagedLease() && !Run->HasCombatCheckpoint())) return Close();
            if (!CheckParticipation()) return Close();
            Advance(EStep::NextMap);
        }
        else if (Step == EStep::NextMap)
        {
            UButton* Button = NodeButton(Host.Get(), 1);
            if (!Button || !Button->GetIsEnabled()) return false;
            FManagedRunPreview Preview;
            FText Error;
            if (!Test->TestTrue(TEXT("Canonical map storage retains approved participation after the combat payload clears."), Run->ReadManagedRun(Identity.RunId, Preview, Error) && Preview.Phase == ERunPhase::Map && Preview.Participation.HumanParticipants == Run->GetParticipation().HumanParticipants && Preview.Identity.HostAccountId == Party[HostIndex()].OwnerAccountId)) return Close();
            Button->OnClicked.Broadcast();
            if (!Test->TestTrue(TEXT("The resumed Host starts the second encounter through the real node button."), Run->GetPhase() == ERunPhase::Combat) || !CacheUnits() || !CheckModes()) return Close();
            // Keep the final fixture opponent alive until the human Host can provide a valid pre-close command.
            // 닫기 전 유효한 명령을 인간 Host가 만들 때까지 마지막 테스트 상대를 생존시킵니다.
            for (AUnitBase* Unit : Combat->GetRegisteredUnits())
            {
                if (Unit->GetTeam() != ETeam::Enemy) continue;
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), 1000.0f);
                Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), 1000.0f);
                Unit->ForceNetUpdate();
            }
            Advance(EStep::NextCombat);
        }
        else if (Step == EStep::NextCombat)
        {
            if (!AllViewsMatch() || !Run->HasCombatCheckpoint()) return false;
            if (Combat->GetCurrentUnit() != PartyUnits[HostIndex()].Get() || !Host->CanUseActiveUnitAction())
            {
                if (!DriveHumanTurn()) return Close();
                return false;
            }
            if (!CheckModes() || !CheckSavedAI()) return Close();
            Test->TestTrue(TEXT("The second encounter retains the successor Host and all original character owners."), Run->GetRunIdentity().HostAccountId == Party[HostIndex()].OwnerAccountId && Run->GetRunIdentity().HostEpoch == ExpectedStamp.HostEpoch + 1 && Run->HasManagedLease());
            CheckClosedExecutionFencing();
            return Close();
        }
        return false;
    }

private:
    int32 HostIndex() const { return bResuming ? (bSolo ? 3 : 1) : 0; }
    int32 ConnectionCount() const { return bResuming ? (bSolo ? 1 : OriginalCount - 1) : OriginalCount; }

    TArray<FRunAccountId> ResumedHumans() const
    {
        if (bSolo) return { Party[3].OwnerAccountId };
        TArray<FRunAccountId> Humans;
        for (int32 Index = 1; Index < OriginalCount; ++Index) Humans.Add(Party[Index].OwnerAccountId);
        return Humans;
    }

    void Advance(EStep Next)
    {
        Step = Next;
        StepStarted = FPlatformTime::Seconds();
        Test->AddInfo(FString::Printf(TEXT("Managed PIE solo=%d resume=%d step=%d."), bSolo, bResuming, static_cast<int32>(Step)));
    }

    bool Close(bool bThenResume = false)
    {
        ShutdownExecution();
        bRestart = bThenResume;
        Advance(EStep::Closing);
        GEditor->RequestEndPlayMap();
        return false;
    }

    void ShutdownExecution()
    {
        RemoveObservers();
        if (TravelFailureObserver.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureObserver);
        TravelFailureObserver.Reset();
        if (Encounter.IsValid()) Encounter->ShutdownGameplay();
        if (Run.IsValid()) Run->CloseManagedRun();
    }

    bool HasPIEWorld() const
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World()) return true;
        }
        return false;
    }

    void ClearSession()
    {
        Server.Reset();
        Host.Reset();
        Combat.Reset();
        Encounter.Reset();
        Run.Reset();
        Peers.Reset();
        PartyUnits.Reset();
        PendingController.Reset();
        AcceptedRemoteAccounts.Reset();
        bResumeUnitsObserved = false;
    }

    void Launch()
    {
        PlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
        PlaySettings->SetPlayNetMode(ConnectionCount() == 1 ? PIE_Standalone : PIE_ListenServer);
        PlaySettings->SetPlayNumberOfClients(ConnectionCount());
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
        Params.GlobalMapOverride = bResuming && bSolo ? TEXT("/Game/User_JeHoon/LEVEL/MainMenu") : TEXT("/Game/User_JeHoon/LEVEL/Gameplay");
        Params.StartLocation = FVector(0.0f, 0.0f, 300.0f);
        GEditor->RequestPlaySession(Params);
    }

    bool FindSession()
    {
        TArray<FPeer> Found;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World || !World->GetMapName().Contains(TEXT("Gameplay"))) continue;
            if (World->GetNetMode() == NM_ListenServer || (ConnectionCount() == 1 && World->GetNetMode() == NM_Standalone)) Server = World;
            else if (World->GetNetMode() == NM_Client)
            {
                FPeer& Peer = Found.AddDefaulted_GetRef();
                Peer.PIEInstance = Context.PIEInstance;
                Peer.World = World;
                Peer.Client = Cast<AGameplayPlayerController>(World->GetFirstPlayerController());
            }
        }
        if (!Server.IsValid() || Found.Num() != ConnectionCount() - 1) return false;
        UNetDriver* Driver = Server->GetNetDriver();
        if (ConnectionCount() > 1 && (!Driver || Driver->ClientConnections.Num() != ConnectionCount() - 1)) return false;
        Found.Sort([](const FPeer& A, const FPeer& B) { return A.PIEInstance < B.PIEInstance; });
        for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
        {
            AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(It->Get());
            if (Controller && Controller->IsLocalController()) Host = Controller;
            else if (Controller && Controller->GetNetConnection() && Controller->GetPlayerState<APlayerState>())
            {
                for (FPeer& Peer : Found)
                {
                    APlayerState* State = Peer.Client.IsValid() ? Peer.Client->GetPlayerState<APlayerState>() : nullptr;
                    if (State && State->GetPlayerId() == Controller->GetPlayerState<APlayerState>()->GetPlayerId()) Peer.Remote = Controller;
                }
            }
        }
        TSet<AGameplayPlayerController*> Unique;
        for (const FPeer& Peer : Found)
        {
            UNetDriver* ClientDriver = Peer.World->GetNetDriver();
            if (!Peer.Client.IsValid() || !Peer.Remote.IsValid() || !ClientDriver || ClientDriver == Driver || !ClientDriver->ServerConnection || ClientDriver->ServerConnection->GetConnectionState() != USOCK_Open || Peer.Remote->GetNetConnection()->GetConnectionState() != USOCK_Open || Unique.Contains(Peer.Remote.Get())) return false;
            Unique.Add(Peer.Remote.Get());
        }
        AGameplayGameModeBase* Mode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        AGameplayGameState* State = Server->GetGameState<AGameplayGameState>();
        if (!Host.IsValid() || !Mode || !Mode->GetEncounterManager() || !State || !State->GetArena()) return false;
        Peers = MoveTemp(Found);
        Encounter = Mode->GetEncounterManager();
        Combat = Encounter->GetCombatManager();
        Run = Server->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        return Combat.IsValid() && Run.IsValid();
    }

    bool ConfigureCaller(URunStateSubsystem* State)
    {
        FLocalDevelopmentCallerContext Context;
        Context.StoreNamespace = StoreNamespace;
        Context.AccountId = Party[HostIndex()].OwnerAccountId;
        FText Error;
        const bool bValid = State->ConfigureLocalDevelopmentCaller(Context, Error);
        if (!Test->TestTrue(TEXT("The trusted fixture assigns an immutable local development caller."), bValid)) Test->AddError(Error.ToString());
        return bValid;
    }

    bool BindHumans()
    {
        AGameplayGameModeBase* Mode = Server->GetAuthGameMode<AGameplayGameModeBase>();
        if (!Test->TestTrue(TEXT("The current local Host is explicitly bound to its original development identity."), Mode->AssignRunParticipant(Host.Get(), Party[HostIndex()].OwnerAccountId))) return false;
        for (int32 Index = 0; Index < Peers.Num(); ++Index)
        {
            const int32 OwnerIndex = bResuming ? Index + 2 : Index + 1;
            if (!Test->TestTrue(TEXT("Only current human original participants receive actual remote connections."), Mode->AssignRunParticipant(Peers[Index].Remote.Get(), Party[OwnerIndex].OwnerAccountId))) return false;
        }
        return true;
    }

    bool CreateRun()
    {
        Run->PartyDefinition = Server->GetAuthGameMode<AGameplayGameModeBase>()->PartyDefinition;
        FText Error;
        if (!ConfigureCaller(Run.Get()) || !Test->TestTrue(TEXT("The original Host creates a numbered managed Run with no consent requirement."), Run->CreateManagedRun(Party, Identity, Error)))
        {
            if (!Error.IsEmpty()) Test->AddError(Error.ToString());
            return false;
        }
        return BindHumans();
    }

    bool RestoreRun()
    {
        FText Error;
        FManagedRunPreview Preview;
        if (!ConfigureCaller(Run.Get()) || !Test->TestTrue(TEXT("The successor reads the latest record after the original Gameplay execution closes."), Run->ReadManagedRun(Identity.RunId, Preview, Error) && Preview.Stamp == ExpectedStamp)) return false;
        const TArray<FRunAccountId> Humans = ResumedHumans();
        if (!Test->TestTrue(TEXT("Original number two acquires exactly the latest checkpoint for all remaining original humans."), Run->ResumeManagedRun(Preview.Stamp, Humans, Error)) || !CheckAcquiredBoundary(Run.Get()) || !BindHumans() || !Test->TestTrue(TEXT("The successor restores combat without the original Host connection."), Encounter->RestoreSavedCombat(Party[1].OwnerAccountId, Error)))
        {
            Test->AddError(Error.ToString());
            return false;
        }
        if (!CacheUnits() || !CheckResumeIdentity()) return false;
        ObserveAI();
        bResumeUnitsObserved = true;
        return true;
    }

    bool UpdateMenu()
    {
        AMainMenuPlayerController* Menu = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Standalone) Menu = Cast<AMainMenuPlayerController>(Context.World()->GetFirstPlayerController());
        }
        UMainMenuScreenWidget* Screen = FindScreen<UMainMenuScreenWidget>(Menu);
        if (!Menu || !Screen) return false;
        URunStateSubsystem* State = Menu->GetGameInstance()->GetSubsystem<URunStateSubsystem>();
        Run = State;
        if (Step == EStep::Menu)
        {
            FText Error;
            if (!ConfigureCaller(State) || !Test->TestTrue(TEXT("Original number four selects the shared managed Run from the actual menu."), State->SetManagedResumeTarget(Identity.RunId, Error))) return Close();
            Screen->RefreshResumeActions();
            Advance(EStep::MenuReady);
            return false;
        }
        UButton* Convert = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_ConvertToSolo")));
        UButton* Resume = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_ResumeSolo")));
        if (!Convert || !Resume) return false;
        if (Step == EStep::MenuTravelFailed)
        {
            if (!bTravelFailureObserved || State->HasManagedLease() || State->IsManagedResumePending()) return false;
            FManagedRunPreview Preview;
            FText Error;
            TArray<uint8> CurrentBytes;
            const FString Slot = FLocalRunAuthorityStore(StoreNamespace).GetSlotName(Identity.RunId);
            if (!Test->TestTrue(TEXT("Actual failed map travel destroys the old menu controller and returns through the same GameInstance."), !FailedTravelMenu.IsValid() && State == FailedTravelRun.Get()) || !Test->TestTrue(TEXT("Travel failure preserves the immutable caller and selected Run for retry."), State->GetLocalCaller() == Party[3].OwnerAccountId && State->GetManagedResumeTarget() == Identity.RunId)) return Close();
            if (!Test->TestTrue(TEXT("The committed successor Host and permanent AI roster survive actual travel failure."), State->ReadManagedRun(Identity.RunId, Preview, Error) && Preview.Stamp == ExpectedStamp && Preview.Identity.HostAccountId == Party[3].OwnerAccountId && Preview.Participation.HumanParticipants == TArray<FRunAccountId>{ Party[3].OwnerAccountId } && Preview.bHasCombatCheckpoint)) return Close();
            if (!Test->TestTrue(TEXT("Travel failure does not roll back or rewrite the canonical committed bytes."), UGameplayStatics::LoadDataFromSlot(CurrentBytes, Slot, 0) && CurrentBytes == FailedTravelBytes)) return Close();
            if (!Test->TestTrue(TEXT("The new menu offers solo retry instead of a second permanent conversion."), Resume->GetIsEnabled() && !Convert->GetIsEnabled())) return Close();
            const FNameProperty* Destination = FindFProperty<FNameProperty>(AMainMenuPlayerController::StaticClass(), TEXT("GameplayLevelName"));
            if (!Test->TestNotNull(TEXT("The retry uses the real menu travel destination property."), Destination)) return Close();
            Destination->SetPropertyValue_InContainer(Menu, FName(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
            Resume->OnClicked.Broadcast();
            if (!Test->TestTrue(TEXT("The actual retry button reacquires a newer execution and arms normal Gameplay travel."), State->HasManagedLease() && State->IsManagedResumePending() && State->GetManagedStamp().HostEpoch == ExpectedStamp.HostEpoch + 1) || !CheckAcquiredBoundary(State)) return Close();
            if (TravelFailureObserver.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureObserver);
            TravelFailureObserver.Reset();
            Advance(EStep::Restored);
            return false;
        }
        if (!Test->TestTrue(TEXT("The real menu offers conversion for an original human in a cooperative managed Run."), Convert->GetIsEnabled() && !Resume->GetIsEnabled())) return Close();
        if (!bMenuScreenshotRequested)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/T14ManagedResumeMenu.png")), true, false);
            bMenuScreenshotRequested = true;
            MenuScreenshotRequestedAt = FPlatformTime::Seconds();
            return false;
        }
        if (FPlatformTime::Seconds() - MenuScreenshotRequestedAt < 0.5) return false;
        const FNameProperty* Destination = FindFProperty<FNameProperty>(AMainMenuPlayerController::StaticClass(), TEXT("GameplayLevelName"));
        if (!Test->TestNotNull(TEXT("The fixture can select an existing non-map destination without bypassing the real button."), Destination)) return Close();
        Destination->SetPropertyValue_InContainer(Menu, FName(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty")));
        FailedTravelMenu = Menu;
        FailedTravelRun = State;
        TravelFailureObserver = GEngine->OnTravelFailure().AddLambda([this](UWorld* World, ETravelFailure::Type, const FString& ErrorString)
        {
            if (World && FailedTravelRun.IsValid() && World->GetGameInstance() == FailedTravelRun->GetGameInstance() && ErrorString.Contains(TEXT("DA_VerticalSliceParty")))
            {
                bTravelFailureObserved = true;
                Test->AddInfo(TEXT("Observed actual non-map TravelFailure after committed managed solo acquisition."));
            }
        });
        Convert->OnClicked.Broadcast();
        if (!Test->TestTrue(TEXT("The menu button commits a pending solo execution for original number four before travel."), State->HasManagedLease() && State->IsManagedResumePending() && State->GetRunIdentity().HostAccountId == Party[3].OwnerAccountId && State->GetParticipation().HumanParticipants == TArray<FRunAccountId>{ Party[3].OwnerAccountId })) return Close();
        if (!CheckAcquiredBoundary(State)) return Close();
        Expected = State->GetCombatCheckpoint();
        ExpectedStamp = State->GetManagedStamp();
        if (!Test->TestTrue(TEXT("The fixture captures committed canonical bytes before the queued map load fails."), UGameplayStatics::LoadDataFromSlot(FailedTravelBytes, FLocalRunAuthorityStore(StoreNamespace).GetSlotName(Identity.RunId), 0))) return Close();
        Advance(EStep::MenuTravelFailed);
        return false;
    }

    bool CacheUnits()
    {
        PartyUnits.SetNum(OriginalCount);
        for (AUnitBase* Unit : Combat->GetRegisteredUnits())
        {
            for (int32 Index = 0; Index < OriginalCount; ++Index)
            {
                if (Combat->GetCharacterId(Unit) == Party[Index].CharacterId) PartyUnits[Index] = Cast<APlayerUnit>(Unit);
            }
        }
        return Test->TestTrue(TEXT("Every original character exists even when its owner is absent."), !PartyUnits.ContainsByPredicate([](const TWeakObjectPtr<APlayerUnit>& Unit) { return !Unit.IsValid(); }));
    }

    bool CheckParticipation()
    {
        const TArray<FRunAccountId> Humans = ResumedHumans();
        return Test->TestTrue(TEXT("Approved human participation remains fixed through results and future encounters."), Run->GetParticipation().HumanParticipants == Humans && Run->GetRunIdentity().OriginalParticipants.Num() == OriginalCount);
    }

    bool CheckHumanBindings()
    {
        UCombatActionAuthority* Authority = Combat->GetActionAuthority();
        if (!Test->TestTrue(TEXT("The successor has one live connection per remaining human."), Authority && Peers.Num() + 1 == ResumedHumans().Num())) return false;
        const FGuid HostBinding = Host->GetParticipantBindingId();
        if (!Test->TestTrue(TEXT("The successor Host has its own original account and server-issued binding."), HostBinding.IsValid() && HostBinding == Authority->GetParticipantBindingId(Host.Get()) && Host->GetBoundParticipantAccount() == Party[HostIndex()].OwnerAccountId)) return false;
        TSet<FGuid> Bindings{ HostBinding };
        for (int32 Index = 0; Index < Peers.Num(); ++Index)
        {
            const FPeer& Peer = Peers[Index];
            const int32 OwnerIndex = Index + 2;
            const FGuid Binding = Peer.Client->GetParticipantBindingId();
            if (!Test->TestTrue(TEXT("Every client has a distinct binding for the matching original server-side account."), Binding.IsValid() && !Bindings.Contains(Binding) && Binding == Authority->GetParticipantBindingId(Peer.Remote.Get()) && Peer.Client->GetBoundParticipantAccount() == Party[OwnerIndex].OwnerAccountId && Peer.Remote->GetBoundParticipantAccount() == Party[OwnerIndex].OwnerAccountId)) return false;
            Bindings.Add(Binding);
            for (int32 CharacterIndex = 0; CharacterIndex < OriginalCount; ++CharacterIndex)
            {
                if (!Test->TestEqual(TEXT("A remaining client controls exactly its original character, including after Host succession."), Authority->CanControllerControl(Peer.Remote.Get(), PartyUnits[CharacterIndex].Get()), CharacterIndex == OwnerIndex)) return false;
            }
        }
        return true;
    }

    bool CheckResumeIdentity()
    {
        const FCombatCheckpointData& Checkpoint = Run->GetCombatCheckpoint();
        if (!Test->TestTrue(TEXT("Resume retains the same Run, original roster, attempt and confirmed turn with one newer Host epoch."), Run->GetRunIdentity().RunId == Identity.RunId && Run->GetRunIdentity().HostAccountId == Party[HostIndex()].OwnerAccountId && Run->GetRunIdentity().HostEpoch == ExpectedStamp.HostEpoch + 1 && Checkpoint.AttemptId == Expected.AttemptId && Checkpoint.CompletedTurnSerial >= Expected.CompletedTurnSerial && Combat->GetCombatInstanceId() != OldCombatId)) return false;
        for (int32 Index = 0; Index < OriginalCount; ++Index)
        {
            const FRunParticipantData& Participant = Run->GetRunIdentity().OriginalParticipants[Index];
            if (!Test->TestTrue(TEXT("Original join numbers, identities and Unknown legacy consent survive real resume."), Participant.AccountId == Party[Index].OwnerAccountId && Participant.JoinOrdinal == Index + 1 && Participant.AIConsent == ERunAIConsent::Unknown && Participant.ConsentPolicyVersion == 0)) return false;
        }
        return CheckParticipation();
    }

    bool CheckAcquiredBoundary(const URunStateSubsystem* State)
    {
        FCombatCheckpointData Actual = State->GetCombatCheckpoint();
        if (!Test->TestTrue(TEXT("Acquisition changes only Host identity and approved control modes before any restored action starts."), Actual.Units.Num() == Expected.Units.Num() && Actual.Identity.HostAccountId == Party[HostIndex()].OwnerAccountId && Actual.Identity.HostEpoch == Expected.Identity.HostEpoch + 1)) return false;
        Actual.Identity = Expected.Identity;
        for (int32 Index = 0; Index < Actual.Units.Num(); ++Index)
        {
            const FCombatCheckpointUnit& StoredUnit = State->GetCombatCheckpoint().Units[Index];
            if (StoredUnit.Team == ETeam::Player)
            {
                const bool bAI = !State->GetParticipation().HumanParticipants.Contains(StoredUnit.OwnerAccountId);
                if (!Test->TestTrue(TEXT("Acquisition derives each mode from approved human participation and its original owner."), (StoredUnit.PartyControlMode == EPartyControlMode::ServerAI) == bAI)) return false;
            }
            Actual.Units[Index].PartyControlMode = Expected.Units[Index].PartyControlMode;
        }
        return Test->TestTrue(TEXT("HP/AP, turn order, placement, skills, identities, attempt and all other confirmed values survive acquisition exactly."), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Actual, &Expected, 0));
    }

    bool CheckModes()
    {
        for (int32 Index = 0; Index < OriginalCount; ++Index)
        {
            APlayerUnit* Unit = PartyUnits[Index].Get();
            const bool bAI = !Run->GetParticipation().HumanParticipants.Contains(Party[Index].OwnerAccountId);
            if (!Test->TestTrue(TEXT("Persistent AI changes control only and retains each original character owner and player team."), Unit && Unit->IsServerAIControlled() == bAI && Unit->GetTeam() == ETeam::Player && Combat->GetOwnerAccountId(Unit) == Party[Index].OwnerAccountId && Combat->GetCharacterId(Unit) == Party[Index].CharacterId)) return false;
        }
        return CheckParticipation();
    }

    bool CheckSavedAI()
    {
        for (const FCombatCheckpointUnit& Unit : Run->GetCombatCheckpoint().Units)
        {
            if (Unit.Team != ETeam::Player) continue;
            const bool bAI = !Run->GetParticipation().HumanParticipants.Contains(Unit.OwnerAccountId);
            if (!Test->TestTrue(TEXT("New canonical turn checkpoints retain each approved human or AI mode."), (Unit.PartyControlMode == EPartyControlMode::ServerAI) == bAI)) return false;
        }
        return true;
    }

    UButton* NodeButton(AGameplayPlayerController* Controller, int32 Index) const
    {
        URunMapWidget* Screen = FindScreen<URunMapWidget>(Controller);
        UVerticalBox* Nodes = Screen ? Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("NodeList"))) : nullptr;
        return Nodes && Index < Nodes->GetChildrenCount() ? Cast<UButton>(Nodes->GetChildAt(Index)) : nullptr;
    }

    bool AllViewsMatch() const
    {
        if (!Combat.IsValid() || !Run.IsValid()) return false;
        if (Run->GetPhase() == ERunPhase::Combat && (!Combat->IsCombatActive() || !Combat->GetCurrentUnit() || !FindScreen<UCombatHUDWidget>(Host.Get()) || Host->GetBoundParticipantAccount() != Party[HostIndex()].OwnerAccountId)) return false;
        for (const FPeer& Peer : Peers)
        {
            if (!Peer.Client->GetParticipantBindingId().IsValid() || Peer.Client->GetParticipantBindingId() != Peer.Remote->GetParticipantBindingId() || Peer.Client->GetBoundParticipantAccount() != Peer.Remote->GetBoundParticipantAccount()) return false;
            AGameplayGameState* State = Peer.World->GetGameState<AGameplayGameState>();
            ACombatManager* ClientCombat = State ? State->GetCombatManager() : nullptr;
            if (!ClientCombat || ClientCombat != Peer.Client->GetCombatManager() || State->GetViewState().Phase != Run->GetPhase() || State->GetViewState().ConfirmedCombatRevision != Run->GetCombatCheckpoint().Revision || ClientCombat->GetCombatInstanceId() != Combat->GetCombatInstanceId() || ClientCombat->GetRunId() != Combat->GetRunId() || ClientCombat->GetHostEpoch() != Combat->GetHostEpoch() || ClientCombat->GetTurnSerial() != Combat->GetTurnSerial() || ClientCombat->IsCombatActive() != Combat->IsCombatActive() || ClientCombat->IsAwaitingTurnCheckpoint() != Combat->IsAwaitingTurnCheckpoint() || ClientCombat->GetCombatResult() != Combat->GetCombatResult() || ClientCombat->GetRuntimeUnitId(ClientCombat->GetCurrentUnit()) != Combat->GetRuntimeUnitId(Combat->GetCurrentUnit()) || ClientCombat->GetRegisteredUnits().Num() != Combat->GetRegisteredUnits().Num()) return false;
            for (AUnitBase* Unit : Combat->GetRegisteredUnits())
            {
                AUnitBase* Other = ClientCombat->ResolveRuntimeUnit(Combat->GetRuntimeUnitId(Unit));
                if (!Other || !Other->GetAttributeSet() || Other->GetTeam() != Unit->GetTeam() || Other->IsUnitAlive() != Unit->IsUnitAlive() || Other->IsBusy() != Unit->IsBusy() || Other->IsActiveTurn() != Unit->IsActiveTurn() || Other->GetCurrentActionPoint() != Unit->GetCurrentActionPoint() || Other->GetCurrentSubActionPoint() != Unit->GetCurrentSubActionPoint() || !FMath::IsNearlyEqual(Other->GetAttributeSet()->GetHP(), Unit->GetAttributeSet()->GetHP()) || ClientCombat->GetOwnerAccountId(Other) != Combat->GetOwnerAccountId(Unit) || ClientCombat->GetCharacterId(Other) != Combat->GetCharacterId(Unit)) return false;
                if (APlayerUnit* Player = Cast<APlayerUnit>(Unit))
                {
                    APlayerUnit* OtherPlayer = Cast<APlayerUnit>(Other);
                    if (!OtherPlayer || OtherPlayer->GetPartyControlMode() != Player->GetPartyControlMode()) return false;
                }
                ACombatGridTile* Tile = Unit->GetCurrentTile();
                ACombatGridTile* OtherTile = Other->GetCurrentTile();
                if (Tile ? !OtherTile || Tile->GridCoord != OtherTile->GridCoord || OtherTile->GetOccupyingUnit() != Other : OtherTile != nullptr) return false;
            }
            UCombatHUDWidget* HostHUD = FindScreen<UCombatHUDWidget>(Host.Get());
            UCombatHUDWidget* ClientHUD = FindScreen<UCombatHUDWidget>(Peer.Client.Get());
            if (Run->GetPhase() == ERunPhase::Combat && (!HostHUD || !ClientHUD || !HostHUD->GetTurnInfoText().EqualTo(ClientHUD->GetTurnInfoText()))) return false;
        }
        return true;
    }

    bool EndHumanTurn(AGameplayPlayerController* Controller)
    {
        FCombatActionRequest Request;
        if (!Test->TestTrue(TEXT("The current human owner builds a real turn command."), Controller->BuildCombatActionRequest(ECombatActionKind::EndTurn, nullptr, nullptr, Request))) return false;
        const FCombatActionResponse Response = Controller->SubmitCombatActionRequest(Request);
        if (Controller->HasAuthority()) return Test->TestTrue(TEXT("The local human Host executes through common command validation."), Response.Result == ECombatRequestResult::Accepted);
        PendingController = Controller;
        PendingSequence = Request.RequestSequence;
        return Test->TestTrue(TEXT("The remaining client sends an actual owning-connection RPC."), Response.Result == ECombatRequestResult::Pending);
    }

    bool DriveHumanTurn()
    {
        if (PendingController.IsValid())
        {
            const FCombatActionResponse& Response = PendingController->GetLastCombatActionResponse();
            if (Response.RequestSequence != PendingSequence || Response.Result == ECombatRequestResult::Pending) return true;
            if (!Test->TestTrue(TEXT("The successor server acknowledges the remaining client's real command."), Response.Result == ECombatRequestResult::Accepted)) return false;
            AcceptedRemoteAccounts.AddUnique(PendingController->GetBoundParticipantAccount());
            PendingController.Reset();
        }
        if (!AllViewsMatch()) return true;
        if (Host->CanUseActiveUnitAction()) return EndHumanTurn(Host.Get());
        for (const FPeer& Peer : Peers)
        {
            if (Peer.Client->CanUseActiveUnitAction()) return EndHumanTurn(Peer.Client.Get());
        }
        return true;
    }

    bool RejectOtherCharacter()
    {
        FCombatActionRequest Request;
        if (!Test->TestTrue(TEXT("The successor Host builds a current-context command."), Host->BuildCombatActionRequest(ECombatActionKind::EndTurn, nullptr, nullptr, Request))) return false;
        Request.UnitId = Combat->GetRuntimeUnitId(PartyUnits[0].Get());
        const int32 Before = Combat->GetTurnSerial();
        return Test->TestTrue(TEXT("Host succession never grants human control of the absent original Host's character."), Host->SubmitCombatActionRequest(Request).Result == ECombatRequestResult::NotOwner && Combat->GetTurnSerial() == Before);
    }

    bool CheckClosedExecutionFencing()
    {
        UCombatActionAuthority* Authority = Combat->GetActionAuthority();
        APlayerUnit* HumanUnit = PartyUnits[HostIndex()].Get();
        APlayerUnit* AIUnit = PartyUnits[0].Get();
        FCombatActionRequest HumanRequest;
        if (!Test->TestTrue(TEXT("The live successor Host can prepare a valid current-turn command before direct closure."), Authority && Run->HasManagedLease() && Authority->CanControllerControl(Host.Get(), HumanUnit) && Host->BuildCombatActionRequest(ECombatActionKind::EndTurn, nullptr, nullptr, HumanRequest))) return false;
        FCombatActionRequest AIRequest = HumanRequest;
        AIRequest.UnitId = Combat->GetRuntimeUnitId(AIUnit);
        AIRequest.ParticipantBindingId.Invalidate();
        AIRequest.RequestSequence = MAX_int64;
        const FGuid AISession = AIUnit->GetAIControlSessionId();
        const FRunAccountId HostAccount = Run->GetRunIdentity().HostAccountId;
        const int32 Turn = Combat->GetTurnSerial();
        const int32 HumanAP = HumanUnit->GetCurrentActionPoint();
        const int32 AIAP = AIUnit->GetCurrentActionPoint();
        const float BeforeEnemyHP = EnemyHP();
        const EPartyControlMode Mode = AIUnit->GetPartyControlMode();
        // This intentionally bypasses normal shutdown to test an old authority's lease fencing on still-live actors.
        // 살아 있는 액터의 이전 Authority가 lease를 검사하는지 확인하기 위해 정상 종료 순서를 의도적으로 우회합니다.
        Run->CloseManagedRun();
        Test->TestTrue(TEXT("Direct closure releases the managed execution without destroying its old combat actors."), !Run->HasManagedLease() && IsValid(Authority) && IsValid(HumanUnit) && IsValid(AIUnit));
        Test->TestEqual(TEXT("A formerly valid human command cannot execute after its managed Run closes."), Authority->Execute(Host.Get(), HumanRequest).Result, ECombatRequestResult::InvalidContext);
        Test->TestEqual(TEXT("A live old AI session cannot execute after its managed Run closes."), Authority->ExecuteServerAI(AIUnit, AIRequest, AISession).Result, ECombatRequestResult::InvalidContext);
        Test->TestFalse(TEXT("An old authority cannot rebind even its original Host after closure."), Authority->BindParticipant(Host.Get(), HostAccount));
        FText Error;
        Test->TestFalse(TEXT("An old authority cannot change party control mode after closure."), Authority->SetPartyControlMode(AIUnit, EPartyControlMode::Human, Error));
        return Test->TestTrue(TEXT("Rejected post-close operations preserve turn, HP, AP and AI mode without an offline fallback."), Combat->GetTurnSerial() == Turn && HumanUnit->GetCurrentActionPoint() == HumanAP && AIUnit->GetCurrentActionPoint() == AIAP && EnemyHP() == BeforeEnemyHP && AIUnit->GetPartyControlMode() == Mode);
    }

    float EnemyHP() const
    {
        float HP = 0.0f;
        for (AUnitBase* Unit : Combat->GetRegisteredUnits())
        {
            if (Unit->GetTeam() == ETeam::Enemy) HP += Unit->GetAttributeSet()->GetHP();
        }
        return HP;
    }

    void ObserveAI()
    {
        AIHPBeforeAttack = EnemyHP();
        AIHPAfterAttack = AIHPBeforeAttack;
        for (const TWeakObjectPtr<APlayerUnit>& Unit : PartyUnits)
        {
            if (!Unit->IsServerAIControlled()) continue;
            const FDelegateHandle Handle = Unit->OnActionCompleted.AddLambda([this](AUnitBase*, EUnitActionType Type, EUnitActionResult Result)
            {
                if (Result != EUnitActionResult::Succeeded) bActionFailed = true;
                if (Type == EUnitActionType::Skill && Result == EUnitActionResult::Succeeded)
                {
                    ++AISkills;
                    AIHPAfterAttack = EnemyHP();
                }
            });
            Observers.Add(Unit, Handle);
        }
    }

    void RemoveObservers()
    {
        for (const TPair<TWeakObjectPtr<APlayerUnit>, FDelegateHandle>& Entry : Observers)
        {
            if (Entry.Key.IsValid()) Entry.Key->OnActionCompleted.Remove(Entry.Value);
        }
        Observers.Reset();
    }

    bool EditorNavigationReady()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (!World || !World->GetMapName().Contains(TEXT("Gameplay"))) return false;
        UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
        ANavigationData* NavData = Navigation ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        if (!NavData || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World) || Navigation->GetNumRemainingBuildTasks() != 0) return false;
        for (TActorIterator<ACombatArena> It(World); It; ++It)
        {
            ACombatGridManager* Grid = It->Grid;
            if (!Grid) continue;
            const FFloatProperty* Spacing = FindFProperty<FFloatProperty>(Grid->GetClass(), TEXT("Spacing"));
            if (!Spacing) return false;
            const FVector Start = Grid->GetActorLocation() + FVector(0.0f, Spacing->GetPropertyValue_InContainer(Grid), 100.0f);
            const FVector Goal = Grid->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
            FNavLocation ProjectedStart;
            FNavLocation ProjectedGoal;
            if (!Navigation->ProjectPointToNavigation(Start, ProjectedStart, NavData->GetDefaultQueryExtent(), NavData) || !Navigation->ProjectPointToNavigation(Goal, ProjectedGoal, NavData->GetDefaultQueryExtent(), NavData)) return false;
            UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, Goal, NavData);
            return Path && Path->IsValid() && !Path->IsPartial();
        }
        return false;
    }

    FAutomationTestBase* Test;
    bool bSolo;
    int32 OriginalCount;
    FString StoreNamespace;
    FRunIdentityData Identity;
    TArray<FRunPartyMember> Party;
    FCombatCheckpointData Expected;
    FRunAuthorityStamp ExpectedStamp;
    FGuid OldCombatId;
    EStep Step = EStep::Launch;
    double StepStarted;
    bool bResuming = false;
    bool bRestart = false;
    bool bActionFailed = false;
    bool bResumeUnitsObserved = false;
    bool bMenuScreenshotRequested = false;
    bool bTravelFailureObserved = false;
    double MenuScreenshotRequestedAt = 0.0;
    int32 AISkills = 0;
    float AIHPBeforeAttack = 0.0f;
    float AIHPAfterAttack = 0.0f;
    FString Diagnostic;
    TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
    TWeakObjectPtr<UWorld> Server;
    TWeakObjectPtr<AGameplayPlayerController> Host;
    TWeakObjectPtr<URunStateSubsystem> Run;
    TWeakObjectPtr<AEncounterManager> Encounter;
    TWeakObjectPtr<ACombatManager> Combat;
    TArray<FPeer> Peers;
    TArray<TWeakObjectPtr<APlayerUnit>> PartyUnits;
    TArray<TWeakObjectPtr<AUnitBase>> OldUnits;
    TMap<TWeakObjectPtr<APlayerUnit>, FDelegateHandle> Observers;
    FDelegateHandle TravelFailureObserver;
    TWeakObjectPtr<AMainMenuPlayerController> FailedTravelMenu;
    TWeakObjectPtr<URunStateSubsystem> FailedTravelRun;
    TArray<uint8> FailedTravelBytes;
    TWeakObjectPtr<AGameplayPlayerController> PendingController;
    TArray<FRunAccountId> AcceptedRemoteAccounts;
    int64 PendingSequence = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunHostSuccessionPIETest, "ProjectA.ManagedRunPIE.HostSuccession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunHostSuccessionPIETest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("T14ManagedRunPIE")))
    {
        AddInfo(TEXT("Use -T14ManagedRunPIE for actual managed Run Host succession sessions."));
        return true;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ManagedRunPIETests::FManagedSessions>(this, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunFourPlayerSuccessionPIETest, "ProjectA.ManagedRunPIE.HostSuccession4Players", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunFourPlayerSuccessionPIETest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("T14ManagedRunPIE")))
    {
        AddInfo(TEXT("Use -T14ManagedRunPIE for actual four-original-player succession to three humans and one absent AI."));
        return true;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ManagedRunPIETests::FManagedSessions>(this, false, 4));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunSoloMenuPIETest, "ProjectA.ManagedRunPIE.SoloMenuConversion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunSoloMenuPIETest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("T14ManagedRunPIE")))
    {
        AddInfo(TEXT("Use -T14ManagedRunPIE for the actual fourth participant's menu conversion and Standalone restoration."));
        return true;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/User_JeHoon/LEVEL/Gameplay")));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<ManagedRunPIETests::FManagedSessions>(this, true));
    return true;
}

#endif
