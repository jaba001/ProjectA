#include "Controller/GameplayPlayerController.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/Run/RunStateSubsystem.h"
#include "UI/Gameplay/GameplayRootWidget.h"

AGameplayPlayerController::AGameplayPlayerController()
{
    bAutoManageActiveCameraTarget = false;
    GameplayRootWidgetClass = UGameplayRootWidget::StaticClass();
}

void AGameplayPlayerController::BeginPlay()
{
    Super::BeginPlay();
    SetCombatContext(nullptr, false);

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

        RunState = GameInstance->GetSubsystem<URunStateSubsystem>();
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
    if (EncounterManager)
    {
        EncounterManager->RequestStartNode(NodeId);
    }
}

void AGameplayPlayerController::RequestContinueRun()
{
    if (EncounterManager)
    {
        EncounterManager->ContinueRun();
    }
}

void AGameplayPlayerController::RefreshGameplayFlow()
{
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

    if (GameplayRootWidget)
    {
        GameplayRootWidget->RefreshFlow(RunState, FlowMessage);
    }

    // Active CommonUI screens own the input config; the controller keeps combat authorization.
    // 활성 CommonUI 화면이 입력 설정을 소유하고 컨트롤러는 전투 조작 허용 상태를 유지합니다.
    bShowMouseCursor = true;
}
