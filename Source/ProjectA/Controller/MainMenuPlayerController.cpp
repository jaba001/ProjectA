#include "Controller/MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
    bShowMouseCursor = true;
    bAutoManageActiveCameraTarget = false;
    GameplayLevelName = TEXT("/Game/User_JeHoon/LEVEL/Gameplay");
}

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = true;

    FInputModeUIOnly InputMode;
    SetInputMode(InputMode);

    MainMenuPreviewStage = Cast<AMainMenuPreviewStage>(UGameplayStatics::GetActorOfClass(this, AMainMenuPreviewStage::StaticClass()));

    if (MainMenuPreviewStage)
    {
        SetViewTarget(MainMenuPreviewStage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] MainMenuPreviewStage was not found in the level."));
    }

    if (!MainMenuRootWidgetClass)
    {
        MainMenuRootWidgetClass = UMainMenuRootWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] MainMenuRootWidgetClass is not set. Using native fallback."));
    }

    if (!MainMenuScreenWidgetClass)
    {
        MainMenuScreenWidgetClass = UMainMenuScreenWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] MainMenuScreenWidgetClass is not set. Using native fallback."));
    }

    if (!CharacterCreationWidgetClass)
    {
        CharacterCreationWidgetClass = UCharacterCreationWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] CharacterCreationWidgetClass is not set. Using native fallback."));
    }

    MainMenuRootWidget = CreateWidget<UMainMenuRootWidget>(this, MainMenuRootWidgetClass);

    if (!MainMenuRootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create MainMenuRootWidget."));
        return;
    }

    MainMenuRootWidget->AddToViewport();
    ShowMainMenuScreen();
}

UMainMenuRootWidget* AMainMenuPlayerController::GetMainMenuRootWidget() const
{
    return MainMenuRootWidget;
}

AMainMenuPreviewStage* AMainMenuPlayerController::GetPreviewStage() const
{
    return MainMenuPreviewStage;
}

void AMainMenuPlayerController::ShowMainMenuScreen()
{
    if (!MainMenuRootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuRootWidget is not available."));
        return;
    }

    MainMenuRootWidget->PushMainScreen(MainMenuScreenWidgetClass);
}

void AMainMenuPlayerController::ShowCharacterCreationScreen()
{
    if (!MainMenuRootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuRootWidget is not available."));
        return;
    }

    ActiveCharacterCreationWidget = Cast<UCharacterCreationWidget>(MainMenuRootWidget->PushMenuScreen(CharacterCreationWidgetClass));
}

void AMainMenuPlayerController::StartNewGameFromCharacterCreation(const FText& CharacterName, FName CharacterClassId)
{
    if (ActiveCharacterCreationWidget)
    {
        FText Error;
        StartNewGameFromParty(ActiveCharacterCreationWidget->GetPartyMembers(), Error);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] Start requires an active character creation screen with party slots."));
}

bool AMainMenuPlayerController::StartNewGameFromParty(const TArray<FRunPartyMember>& PartyMembers, FText& OutError)
{
    if (GameplayLevelName.IsNone() || !FPackageName::DoesPackageExist(GameplayLevelName.ToString()))
    {
        OutError = FText::FromString(TEXT("Gameplay level is missing. Create the configured Gameplay level first. / Gameplay 레벨을 먼저 생성해 주세요."));
        UE_LOG(LogTemp, Error, TEXT("[MainMenuPlayerController] GameplayLevelName does not exist: %s"), *GameplayLevelName.ToString());
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    URunStateSubsystem* RunState = nullptr;

    if (GameInstance)
    {
        RunState = GameInstance->GetSubsystem<URunStateSubsystem>();
    }

    if (!RunState)
    {
        OutError = FText::FromString(TEXT("Run state is unavailable. / 진행 상태를 사용할 수 없습니다."));
        return false;
    }

    if (!RunState->InitializeRun(PartyMembers, OutError))
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuPlayerController] Party validation failed: %s"), *OutError.ToString());
        return false;
    }

    UGameplayStatics::OpenLevel(this, GameplayLevelName);
    return true;
}
