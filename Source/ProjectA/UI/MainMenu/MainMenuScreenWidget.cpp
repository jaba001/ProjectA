#include "UI/MainMenu/MainMenuScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"

void UMainMenuScreenWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (bCreateLayoutInCode)
    {
        EnsureCodeGeneratedLayout();
    }

    if (Image_Background)
    {
        ConfigureBackgroundImage();
    }

    if (Button_NewGame)
    {
        Button_NewGame->OnClicked.AddUniqueDynamic(this, &UMainMenuScreenWidget::HandleNewGameClicked);
    }

    if (Button_Continue)
    {
        Button_Continue->OnClicked.AddUniqueDynamic(this, &UMainMenuScreenWidget::HandleContinueClicked);
    }

    if (Button_Options)
    {
        Button_Options->OnClicked.AddUniqueDynamic(this, &UMainMenuScreenWidget::HandleOptionsClicked);
    }

    if (Button_Quit)
    {
        Button_Quit->OnClicked.AddUniqueDynamic(this, &UMainMenuScreenWidget::HandleQuitClicked);
    }
}

void UMainMenuScreenWidget::EnsureCodeGeneratedLayout()
{
    if (Button_NewGame || Button_Continue || Button_Options || Button_Quit)
    {
        return;
    }

    if (!WidgetTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuScreenWidget] WidgetTree is not available."));
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CodeGeneratedMainMenuOverlay"));
    Image_Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Background"));
    UVerticalBox* MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CodeGeneratedMainMenuBox"));

    if (!RootOverlay || !Image_Background || !MenuBox)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuScreenWidget] Failed to create menu root layout."));
        return;
    }

    WidgetTree->RootWidget = RootOverlay;
    RootOverlay->SetVisibility(ESlateVisibility::Visible);
    ConfigureBackgroundImage();

    UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(Image_Background);

    if (BackgroundSlot)
    {
        BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
        BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    }

    UOverlaySlot* MenuBoxSlot = RootOverlay->AddChildToOverlay(MenuBox);

    if (MenuBoxSlot)
    {
        MenuBoxSlot->SetHorizontalAlignment(HAlign_Center);
        MenuBoxSlot->SetVerticalAlignment(VAlign_Center);
    }

    Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));

    if (Text_Title)
    {
        Text_Title->SetText(FText::FromString(TEXT("PROJECT A")));
        UVerticalBoxSlot* TitleSlot = MenuBox->AddChildToVerticalBox(Text_Title);

        if (TitleSlot)
        {
            TitleSlot->SetHorizontalAlignment(HAlign_Center);
            TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
        }
    }

    Button_NewGame = CreateMenuButton(MenuBox, FText::FromString(TEXT("New Game")));
    Button_Continue = CreateMenuButton(MenuBox, FText::FromString(TEXT("Continue")));
    Button_Options = CreateMenuButton(MenuBox, FText::FromString(TEXT("Options")));
    Button_Quit = CreateMenuButton(MenuBox, FText::FromString(TEXT("Quit")));
}

void UMainMenuScreenWidget::ConfigureBackgroundImage()
{
    if (!Image_Background)
    {
        return;
    }

    Image_Background->SetVisibility(ESlateVisibility::Visible);
    Image_Background->SetColorAndOpacity(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f));
}

UButton* UMainMenuScreenWidget::CreateMenuButton(UVerticalBox* ParentBox, const FText& ButtonText)
{
    if (!WidgetTree || !ParentBox)
    {
        return nullptr;
    }

    UButton* NewButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

    if (!NewButton)
    {
        return nullptr;
    }

    CreateButtonText(NewButton, ButtonText);

    UVerticalBoxSlot* ButtonSlot = ParentBox->AddChildToVerticalBox(NewButton);

    if (ButtonSlot)
    {
        ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
        ButtonSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
    }

    return NewButton;
}

UTextBlock* UMainMenuScreenWidget::CreateButtonText(UButton* ParentButton, const FText& ButtonText)
{
    if (!WidgetTree || !ParentButton)
    {
        return nullptr;
    }

    UTextBlock* ButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());

    if (!ButtonTextBlock)
    {
        return nullptr;
    }

    ButtonTextBlock->SetText(ButtonText);
    ParentButton->AddChild(ButtonTextBlock);
    return ButtonTextBlock;
}

void UMainMenuScreenWidget::HandleNewGameClicked()
{
    RequestNewGame();
}

void UMainMenuScreenWidget::HandleContinueClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("[MainMenuScreenWidget] Continue is not implemented yet."));
}

void UMainMenuScreenWidget::HandleOptionsClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("[MainMenuScreenWidget] Options is not implemented yet."));
}

void UMainMenuScreenWidget::HandleQuitClicked()
{
    RequestQuitGame();
}

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
