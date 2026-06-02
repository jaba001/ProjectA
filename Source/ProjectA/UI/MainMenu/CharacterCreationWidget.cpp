#include "UI/MainMenu/CharacterCreationWidget.h"

#include "Controller/MainMenuPlayerController.h"

UCharacterCreationWidget::UCharacterCreationWidget()
    : CurrentCharacterClassId(TEXT("Warrior"))
{
}

void UCharacterCreationWidget::SetCharacterName(const FText& NewName)
{
    CurrentCharacterName = NewName;
}

void UCharacterCreationWidget::SelectCharacterClass(FName CharacterClassId)
{
    CurrentCharacterClassId = CharacterClassId;
}

FText UCharacterCreationWidget::GetCharacterName() const
{
    return CurrentCharacterName;
}

FName UCharacterCreationWidget::GetCharacterClassId() const
{
    return CurrentCharacterClassId;
}

FText UCharacterCreationWidget::GetSelectedClassText() const
{
    return FText::FromName(CurrentCharacterClassId);
}

FText UCharacterCreationWidget::GetStatPreviewText() const
{
    if (CurrentCharacterClassId == TEXT("Warrior"))
    {
        return FText::FromString(TEXT("HP 120 / AP 4 / Move 3 / Range 1"));
    }

    if (CurrentCharacterClassId == TEXT("Archer"))
    {
        return FText::FromString(TEXT("HP 90 / AP 4 / Move 3 / Range 4"));
    }

    if (CurrentCharacterClassId == TEXT("Mage"))
    {
        return FText::FromString(TEXT("HP 80 / AP 5 / Move 3 / Range 3"));
    }

    return FText::FromString(TEXT("HP 100 / AP 4 / Move 3 / Range 1"));
}

void UCharacterCreationWidget::RequestBack()
{
    DeactivateWidget();
}

void UCharacterCreationWidget::RequestStartGame()
{
    AMainMenuPlayerController* MainMenuPlayerController = Cast<AMainMenuPlayerController>(GetOwningPlayer());

    if (!MainMenuPlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestStartGame failed because owning player is not AMainMenuPlayerController."));
        return;
    }

    MainMenuPlayerController->StartNewGameFromCharacterCreation(CurrentCharacterName, CurrentCharacterClassId);
}
