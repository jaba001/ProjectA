#include "Controller/GameplayPlayerController.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/GameplayRootWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

AGameplayPlayerController::AGameplayPlayerController()
{
    bAutoManageActiveCameraTarget = false;
    GameplayRootWidgetClass = UGameplayRootWidget::StaticClass();
}

void AGameplayPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // Initial replication can precede client BeginPlay; keep the server's received context intact.
    // 초기 복제는 클라이언트 BeginPlay보다 먼저 올 수 있으므로 수신한 서버 문맥을 유지합니다.
    if (HasAuthority())
    {
        SetCombatContext(nullptr, false);
    }

    if (!IsLocalController())
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        // Clear the legacy UIOnly viewport lock carried across travel from MainMenu.
        // MainMenu의 기존 UIOnly 설정에서 레벨 이동 후 남은 뷰포트 입력 잠금을 해제합니다.
        if (UGameViewportClient* ViewportClient = GameInstance->GetGameViewportClient())
        {
            ViewportClient->SetIgnoreInput(false);
        }

        if (HasAuthority())
        {
            RunState = GameInstance->GetSubsystem<URunStateSubsystem>();
        }
    }

    if (RunState)
    {
        RunState->OnRunStateChanged.AddUObject(this, &AGameplayPlayerController::RefreshGameplayFlow);
    }

    if (!GameplayRootWidgetClass)
    {
        GameplayRootWidgetClass = UGameplayRootWidget::StaticClass();
    }

    GameplayRootWidget = CreateWidget<UGameplayRootWidget>(this, GameplayRootWidgetClass);

    if (GameplayRootWidget)
    {
        GameplayRootWidget->AddToViewport();
    }

    TryBindGameplayState();
    if (!GameplayState)
    {
        GetWorldTimerManager().SetTimer(BindStateTimer, this, &AGameplayPlayerController::TryBindGameplayState, 0.1f, true);
    }
    RefreshGameplayFlow();

    // Restore this player's viewport focus after travel so CommonUI can apply the active screen's input config.
    // 레벨 이동 후 이 플레이어의 뷰포트 포커스를 복원하여 CommonUI가 활성 화면의 입력 설정을 적용하도록 합니다.
    if (FSlateApplication::IsInitialized())
    {
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (TSharedPtr<FSlateUser> SlateUser = LocalPlayer->GetSlateUser())
            {
                FSlateApplication::Get().SetUserFocusToGameViewport(SlateUser->GetUserIndex(), EFocusCause::SetDirectly);
            }
        }
    }
}

void AGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(BindStateTimer);
    if (GameplayState)
    {
        GameplayState->OnGameplayViewChanged.RemoveAll(this);
    }
    if (RunState)
    {
        RunState->OnRunStateChanged.RemoveAll(this);
    }

    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.RemoveAll(this);
    }

    if (GameplayRootWidget)
    {
        GameplayRootWidget->RemoveFromParent();
    }

    Super::EndPlay(EndPlayReason);
}

void AGameplayPlayerController::InitializeGameplay(AEncounterManager* InEncounterManager)
{
    if (HasAuthority() && GetGameInstance())
    {
        RunState = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    }
    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.RemoveAll(this);
    }

    EncounterManager = InEncounterManager;

    if (EncounterManager)
    {
        EncounterManager->OnFlowChanged.AddUObject(this, &AGameplayPlayerController::RefreshGameplayFlow);
    }

    RefreshGameplayFlow();
}

void AGameplayPlayerController::RequestStartNode(FName NodeId)
{
    if (CanIssueRunCommands() && EncounterManager)
    {
        EncounterManager->RequestStartNode(NodeId);
    }
}

void AGameplayPlayerController::RequestContinueRun()
{
    if (CanIssueRunCommands() && EncounterManager)
    {
        EncounterManager->ContinueRun();
    }
}

bool AGameplayPlayerController::CanIssueRunCommands() const
{
    if (!HasAuthority() || !IsLocalController())
    {
        return false;
    }
    if (GetNetMode() == NM_Standalone)
    {
        return true;
    }
    // Recheck the current Run host against the server's trusted connection assignment for every request.
    // 요청마다 현재 Run Host와 서버가 신뢰 배정한 연결을 다시 대조합니다.
    const AGameplayGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameplayGameModeBase>() : nullptr;
    return Mode && Mode->CanControlRunFlow(this);
}

void AGameplayPlayerController::RefreshRunFlowPermissions()
{
    RefreshGameplayFlow();
}

void AGameplayPlayerController::RequestRetryCombatCheckpoint()
{
    // The current local server retries its own storage; this is not a client progression command.
    // 현재 로컬 서버가 자신의 저장을 재시도하며 클라이언트 진행 명령으로 사용하지 않습니다.
    if (HasAuthority() && IsLocalController() && EncounterManager)
    {
        FText Error;
        EncounterManager->RetryCombatCheckpoint(Error);
        RefreshGameplayFlow();
    }
}

void AGameplayPlayerController::RefreshGameplayFlow()
{
    if (!HasAuthority())
    {
        if (GameplayState && GameplayRootWidget)
        {
            const FGameplayViewState& View = GameplayState->GetViewState();
            GameplayRootWidget->RefreshFlowView(View, false);
            if (View.Phase == ERunPhase::Combat && GameplayState->GetArena())
            {
                GameplayState->GetArena()->ActivateArena(this);
            }
        }
        return;
    }
    if (!RunState)
    {
        return;
    }

    const bool bInCombat = RunState->GetPhase() == ERunPhase::Combat;
    ACombatManager* Manager = nullptr;
    FText FlowMessage;

    if (EncounterManager)
    {
        Manager = EncounterManager->GetCombatManager();
        FlowMessage = EncounterManager->GetFlowMessage();
    }

    SetCombatContext(Manager, bInCombat);

    if (!RunState->GetSaveError().IsEmpty())
    {
        FlowMessage = RunState->GetSaveError();
    }

    if (GameplayRootWidget)
    {
        GameplayRootWidget->RefreshFlowView(FGameplayViewState::FromRun(RunState, FlowMessage), CanIssueRunCommands(), IsLocalController() && EncounterManager && EncounterManager->CanRetryCombatCheckpoint());
    }

    // Active CommonUI screens own the input config; the controller keeps combat authorization.
    // 활성 CommonUI 화면이 입력 설정을 소유하고 컨트롤러는 전투 조작 허용 상태를 유지합니다.
    bShowMouseCursor = true;
}

void AGameplayPlayerController::TryBindGameplayState()
{
    AGameplayGameState* State = GetWorld()->GetGameState<AGameplayGameState>();
    if (!State)
    {
        return;
    }
    if (GameplayState != State)
    {
        if (GameplayState)
        {
            GameplayState->OnGameplayViewChanged.RemoveAll(this);
        }
        GameplayState = State;
        GameplayState->OnGameplayViewChanged.AddUObject(this, &AGameplayPlayerController::RefreshGameplayFlow);
    }
    GetWorldTimerManager().ClearTimer(BindStateTimer);
    RefreshGameplayFlow();
}
