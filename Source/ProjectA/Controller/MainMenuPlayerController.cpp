#include "Controller/MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UI/MainMenu/CharacterCreationWidget.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/MainMenuScreenWidget.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
    bShowMouseCursor = true;
}

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = true;

    FInputModeUIOnly InputMode;
    SetInputMode(InputMode);

    if (!MainMenuRootWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuRootWidgetClass is not set."));
        return;
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

    MainMenuRootWidget->PushMenuScreen(CharacterCreationWidgetClass);
}

void AMainMenuPlayerController::StartNewGameFromCharacterCreation(const FText& CharacterName, FName CharacterClassId)
{
    UE_LOG(LogTemp, Log, TEXT("Start new game requested. CharacterName: %s, CharacterClassId: %s"), *CharacterName.ToString(), *CharacterClassId.ToString());

    if (!StartGameLevelName.IsNone())
    {
        UGameplayStatics::OpenLevel(this, StartGameLevelName);
    }
}
