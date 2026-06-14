#include "UI/MainMenu/CharacterCreationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"

UCharacterCreationWidget::UCharacterCreationWidget()
    : CurrentCharacterClassId(TEXT("Warrior"))
{
}

void UCharacterCreationWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (bCreateLayoutInCode)
    {
        EnsureCodeGeneratedLayout();
    }
    else if (!BackgroundBlocker)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Background blocker is missing and code-generated layout is disabled."));
    }

    if (BackgroundBlocker)
    {
        ConfigureBackgroundBlocker();
    }

    if (CenterPanelBackground)
    {
        ConfigureCenterPanelBackground();
    }

    if (EditableTextBox_Name)
    {
        EditableTextBox_Name->OnTextChanged.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleNameTextChanged);
    }

    if (Button_Warrior)
    {
        Button_Warrior->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleWarriorClicked);
    }

    if (Button_Archer)
    {
        Button_Archer->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleArcherClicked);
    }

    if (Button_Mage)
    {
        Button_Mage->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleMageClicked);
    }

    if (Button_Back)
    {
        Button_Back->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleBackClicked);
    }

    if (Button_StartGame)
    {
        Button_StartGame->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleStartGameClicked);
    }

    RefreshPreview();
}

void UCharacterCreationWidget::EnsureCodeGeneratedLayout()
{
    if (BackgroundBlocker)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Using designer BackgroundBlocker."));
    }

    if (EditableTextBox_Name || Button_Warrior || Button_Archer || Button_Mage || Button_Back || Button_StartGame)
    {
        if (!BackgroundBlocker)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Background blocker is missing while designer character creation widgets are present."));
        }

        return;
    }

    if (!WidgetTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] WidgetTree is not available."));
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CodeGeneratedCharacterCreationOverlay"));
    BackgroundBlocker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BackgroundBlocker"));
    CenterPanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterPanelBackground"));
    UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CodeGeneratedCharacterCreationBox"));

    if (!RootOverlay || !BackgroundBlocker || !CenterPanelBackground || !ContentBox)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Failed to create character creation root layout."));
        return;
    }

    WidgetTree->RootWidget = RootOverlay;

    RootOverlay->SetVisibility(ESlateVisibility::Visible);
    ConfigureBackgroundBlocker();
    ConfigureCenterPanelBackground();

    UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(BackgroundBlocker);

    if (BackgroundSlot)
    {
        BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
        BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    }

    CenterPanelBackground->AddChild(ContentBox);

    UOverlaySlot* CenterPanelSlot = RootOverlay->AddChildToOverlay(CenterPanelBackground);

    if (CenterPanelSlot)
    {
        CenterPanelSlot->SetHorizontalAlignment(HAlign_Center);
        CenterPanelSlot->SetVerticalAlignment(VAlign_Center);
    }

    UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Background blocker was created in code."));

    UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("Character Creation")));
        UVerticalBoxSlot* TitleSlot = ContentBox->AddChildToVerticalBox(TitleText);

        if (TitleSlot)
        {
            TitleSlot->SetHorizontalAlignment(HAlign_Center);
            TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
        }
    }

    UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_NameLabel"));

    if (NameLabel)
    {
        NameLabel->SetText(FText::FromString(TEXT("Name")));
        ContentBox->AddChildToVerticalBox(NameLabel);
    }

    EditableTextBox_Name = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_Name"));

    if (EditableTextBox_Name)
    {
        UVerticalBoxSlot* NameInputSlot = ContentBox->AddChildToVerticalBox(EditableTextBox_Name);

        if (NameInputSlot)
        {
            NameInputSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 12.0f));
        }
    }

    UTextBlock* ClassLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ClassLabel"));

    if (ClassLabel)
    {
        ClassLabel->SetText(FText::FromString(TEXT("Class")));
        ContentBox->AddChildToVerticalBox(ClassLabel);
    }

    Button_Warrior = CreateButton(ContentBox, FText::FromString(TEXT("Warrior")));
    Button_Archer = CreateButton(ContentBox, FText::FromString(TEXT("Archer")));
    Button_Mage = CreateButton(ContentBox, FText::FromString(TEXT("Mage")));
    Text_SelectedClass = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_SelectedClass"));
    Text_StatPreview = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_StatPreview"));

    if (Text_SelectedClass)
    {
        UVerticalBoxSlot* SelectedClassSlot = ContentBox->AddChildToVerticalBox(Text_SelectedClass);

        if (SelectedClassSlot)
        {
            SelectedClassSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 4.0f));
        }
    }

    if (Text_StatPreview)
    {
        UVerticalBoxSlot* StatPreviewSlot = ContentBox->AddChildToVerticalBox(Text_StatPreview);

        if (StatPreviewSlot)
        {
            StatPreviewSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
        }
    }

    Button_Back = CreateButton(ContentBox, FText::FromString(TEXT("Back")));
    Button_StartGame = CreateButton(ContentBox, FText::FromString(TEXT("Start Game")));
}

void UCharacterCreationWidget::ConfigureBackgroundBlocker()
{
    if (!BackgroundBlocker)
    {
        return;
    }

    BackgroundBlocker->SetVisibility(ESlateVisibility::Visible);
    BackgroundBlocker->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.12f, 0.85f));
}

void UCharacterCreationWidget::ConfigureCenterPanelBackground()
{
    if (!CenterPanelBackground)
    {
        return;
    }

    CenterPanelBackground->SetVisibility(ESlateVisibility::Visible);
    CenterPanelBackground->SetBrushColor(FLinearColor(0.22f, 0.22f, 0.22f, 1.0f));
    CenterPanelBackground->SetPadding(FMargin(24.0f, 20.0f, 24.0f, 20.0f));
}

void UCharacterCreationWidget::RefreshPreview()
{
    if (Text_SelectedClass)
    {
        Text_SelectedClass->SetText(GetSelectedClassText());
    }

    if (Text_StatPreview)
    {
        Text_StatPreview->SetText(GetStatPreviewText());
    }
}

UButton* UCharacterCreationWidget::CreateButton(UVerticalBox* ParentBox, const FText& ButtonText)
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

UTextBlock* UCharacterCreationWidget::CreateButtonText(UButton* ParentButton, const FText& ButtonText)
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

void UCharacterCreationWidget::HandleNameTextChanged(const FText& NewText)
{
    SetCharacterName(NewText);
}

void UCharacterCreationWidget::HandleWarriorClicked()
{
    SelectCharacterClass(TEXT("Warrior"));
    RefreshPreview();
}

void UCharacterCreationWidget::HandleArcherClicked()
{
    SelectCharacterClass(TEXT("Archer"));
    RefreshPreview();
}

void UCharacterCreationWidget::HandleMageClicked()
{
    SelectCharacterClass(TEXT("Mage"));
    RefreshPreview();
}

void UCharacterCreationWidget::HandleBackClicked()
{
    RequestBack();
}

void UCharacterCreationWidget::HandleStartGameClicked()
{
    RequestStartGame();
}

void UCharacterCreationWidget::SetCharacterName(const FText& NewName)
{
    CurrentCharacterName = NewName;
}

void UCharacterCreationWidget::SelectCharacterClass(FName CharacterClassId)
{
    CurrentCharacterClassId = CharacterClassId;
    RefreshPreview();
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
