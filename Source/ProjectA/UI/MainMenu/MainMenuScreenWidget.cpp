#include "UI/MainMenu/MainMenuScreenWidget.h"

#include "Controller/MainMenuPlayerController.h"

void UMainMenuScreenWidget::RequestNewGame()
{
    AMainMenuPlayerController* MainMenuPlayerController = Cast<AMainMenuPlayerController>(GetOwningPlayer());

    if (!MainMenuPlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestNewGame failed because owning player is not AMainMenuPlayerController."));
        return;
    }

    MainMenuPlayerController->ShowCharacterCreationScreen();
}

void UMainMenuScreenWidget::RequestQuitGame()
{
    UE_LOG(LogTemp, Log, TEXT("Quit game requested from main menu."));
}
