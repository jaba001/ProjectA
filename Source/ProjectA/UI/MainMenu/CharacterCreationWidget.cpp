#include "UI/MainMenu/CharacterCreationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Controller/MainMenuPlayerController.h"
#include "Engine/Texture2D.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"

namespace
{
const TArray<FName> AvailablePartyClassIds = {
    TEXT("StableHand"),
    TEXT("Scholar"),
    TEXT("Herbalist"),
    TEXT("Hunter")
};

constexpr float PartySlotsFixedHeight = 260.0f;
}

UCharacterCreationWidget::UCharacterCreationWidget()
    : CurrentCharacterClassId(TEXT("StableHand"))
{
}

void UCharacterCreationWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!PartyDefinition)
    {
        PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    }

    if (bCreateLayoutInCode)
    {
        EnsureCodeGeneratedLayout();
    }
    else if (!BackgroundBlocker && !FullscreenInputBlocker)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Background blocker is missing and code-generated layout is disabled."));
    }

    InitializeClassSlotWidgetArrays();
    InitializeClassSlots();
    BuildDetailPanel();

    if (FullscreenInputBlocker)
    {
        ConfigureFullscreenInputBlocker();
    }

    ConfigurePartySlotsFixedHeight();

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
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Warrior->GetChildAt(0)))
        {
            Label->SetText(GetDisplayNameForClassId(TEXT("StableHand")));
        }
        Button_Warrior->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleWarriorClicked);
    }

    if (Button_Archer)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Archer->GetChildAt(0)))
        {
            Label->SetText(GetDisplayNameForClassId(TEXT("Hunter")));
        }
        Button_Archer->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleArcherClicked);
    }

    if (Button_Mage)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Mage->GetChildAt(0)))
        {
            Label->SetText(GetDisplayNameForClassId(TEXT("Scholar")));
        }
        Button_Mage->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleMageClicked);
    }

    if (Button_Back)
    {
        Button_Back->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleBackClicked);
    }

    if (Button_Close)
    {
        Button_Close->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleCloseClicked);
    }

    if (Button_StartGame)
    {
        Button_StartGame->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleStartGameClicked);

        if (!Text_StartGameStatus && WidgetTree)
        {
            if (UOverlay* HeaderOverlay = Cast<UOverlay>(Button_StartGame->GetParent()))
            {
                Text_StartGameStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_StartGameStatus"));
                UOverlaySlot* StatusSlot = HeaderOverlay->AddChildToOverlay(Text_StartGameStatus);
                StatusSlot->SetHorizontalAlignment(HAlign_Center);
                StatusSlot->SetVerticalAlignment(VAlign_Top);
                StatusSlot->SetPadding(FMargin(0.0f, 96.0f, 0.0f, 0.0f));
            }
        }
    }

    if (Button_Slot0_Create)
    {
        Button_Slot0_Create->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0CreateClicked);
    }

    if (Button_Slot0_Prev)
    {
        Button_Slot0_Prev->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0PrevClicked);
    }

    if (Button_Slot0_Next)
    {
        Button_Slot0_Next->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0NextClicked);
    }

    if (Button_Slot0_Edit)
    {
        Button_Slot0_Edit->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0EditClicked);
    }

    if (Button_Slot0_Delete)
    {
        Button_Slot0_Delete->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0DeleteClicked);
    }

    if (Button_Slot0_ClassInfo)
    {
        Button_Slot0_ClassInfo->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0ClassInfoClicked);
    }

    if (Button_Slot1_Create)
    {
        Button_Slot1_Create->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1CreateClicked);
    }

    if (Button_Slot1_Prev)
    {
        Button_Slot1_Prev->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1PrevClicked);
    }

    if (Button_Slot1_Next)
    {
        Button_Slot1_Next->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1NextClicked);
    }

    if (Button_Slot1_Edit)
    {
        Button_Slot1_Edit->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1EditClicked);
    }

    if (Button_Slot1_Delete)
    {
        Button_Slot1_Delete->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1DeleteClicked);
    }

    if (Button_Slot1_ClassInfo)
    {
        Button_Slot1_ClassInfo->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1ClassInfoClicked);
    }

    if (Button_Slot2_Create)
    {
        Button_Slot2_Create->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2CreateClicked);
    }

    if (Button_Slot2_Prev)
    {
        Button_Slot2_Prev->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2PrevClicked);
    }

    if (Button_Slot2_Next)
    {
        Button_Slot2_Next->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2NextClicked);
    }

    if (Button_Slot2_Edit)
    {
        Button_Slot2_Edit->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2EditClicked);
    }

    if (Button_Slot2_Delete)
    {
        Button_Slot2_Delete->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2DeleteClicked);
    }

    if (Button_Slot2_ClassInfo)
    {
        Button_Slot2_ClassInfo->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2ClassInfoClicked);
    }

    if (Button_Slot3_Create)
    {
        Button_Slot3_Create->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3CreateClicked);
    }

    if (Button_Slot3_Prev)
    {
        Button_Slot3_Prev->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3PrevClicked);
    }

    if (Button_Slot3_Next)
    {
        Button_Slot3_Next->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3NextClicked);
    }

    if (Button_Slot3_Edit)
    {
        Button_Slot3_Edit->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3EditClicked);
    }

    if (Button_Slot3_Delete)
    {
        Button_Slot3_Delete->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3DeleteClicked);
    }

    if (Button_Slot3_ClassInfo)
    {
        Button_Slot3_ClassInfo->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3ClassInfoClicked);
    }

    RefreshPreview();
    RefreshClassSlotWidgets();
}

void UCharacterCreationWidget::EnsureCodeGeneratedLayout()
{
    if (FullscreenInputBlocker || BottomPanel || BottomHorizontalBox)
    {
        return;
    }

    if (BackgroundBlocker)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Using designer BackgroundBlocker."));
    }

    if (EditableTextBox_Name || Button_Warrior || Button_Archer || Button_Mage || Button_Back || Button_Close || Button_StartGame)
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
    FullscreenInputBlocker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FullscreenInputBlocker"));
    BottomPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomPanel"));
    PartySlotsFixedHeightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PartySlotsFixedHeightBox"));
    BottomHorizontalBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomHorizontalBox"));
    Button_StartGame = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_StartGame"));
    Button_Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Close"));

    if (!RootOverlay || !FullscreenInputBlocker || !BottomPanel || !PartySlotsFixedHeightBox || !BottomHorizontalBox || !Button_StartGame || !Button_Close)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Failed to create character creation party slot fallback layout."));
        return;
    }

    WidgetTree->RootWidget = RootOverlay;

    RootOverlay->SetVisibility(ESlateVisibility::Visible);
    ConfigureFullscreenInputBlocker();
    ConfigurePartySlotsFixedHeight();

    BottomPanel->SetVisibility(ESlateVisibility::Visible);
    BottomPanel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.75f));

    UOverlaySlot* InputBlockerSlot = RootOverlay->AddChildToOverlay(FullscreenInputBlocker);

    if (InputBlockerSlot)
    {
        InputBlockerSlot->SetHorizontalAlignment(HAlign_Fill);
        InputBlockerSlot->SetVerticalAlignment(VAlign_Fill);
    }

    UOverlaySlot* BottomPanelSlot = RootOverlay->AddChildToOverlay(BottomPanel);

    if (BottomPanelSlot)
    {
        BottomPanelSlot->SetHorizontalAlignment(HAlign_Fill);
        BottomPanelSlot->SetVerticalAlignment(VAlign_Bottom);
    }

    BottomPanel->SetContent(PartySlotsFixedHeightBox);

    USizeBoxSlot* PartySlotsSlot = Cast<USizeBoxSlot>(PartySlotsFixedHeightBox->AddChild(BottomHorizontalBox));

    if (PartySlotsSlot)
    {
        PartySlotsSlot->SetHorizontalAlignment(HAlign_Fill);
        PartySlotsSlot->SetVerticalAlignment(VAlign_Fill);
        PartySlotsSlot->SetPadding(FMargin(24.0f, 16.0f, 24.0f, 20.0f));
    }

    UOverlaySlot* StartGameButtonSlot = RootOverlay->AddChildToOverlay(Button_StartGame);

    if (StartGameButtonSlot)
    {
        StartGameButtonSlot->SetHorizontalAlignment(HAlign_Center);
        StartGameButtonSlot->SetVerticalAlignment(VAlign_Top);
        StartGameButtonSlot->SetPadding(FMargin(0.0f, 28.0f, 0.0f, 0.0f));
    }

    UTextBlock* StartGameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_StartGame"));

    if (StartGameText)
    {
        StartGameText->SetText(FText::FromString(TEXT("게임 시작")));
        UButtonSlot* StartGameTextSlot = Cast<UButtonSlot>(Button_StartGame->AddChild(StartGameText));

        if (StartGameTextSlot)
        {
            StartGameTextSlot->SetHorizontalAlignment(HAlign_Center);
            StartGameTextSlot->SetVerticalAlignment(VAlign_Center);
            StartGameTextSlot->SetPadding(FMargin(36.0f, 12.0f, 36.0f, 12.0f));
        }
    }

    UOverlaySlot* CloseButtonSlot = RootOverlay->AddChildToOverlay(Button_Close);

    if (CloseButtonSlot)
    {
        CloseButtonSlot->SetHorizontalAlignment(HAlign_Right);
        CloseButtonSlot->SetVerticalAlignment(VAlign_Top);
        CloseButtonSlot->SetPadding(FMargin(0.0f, 22.0f, 26.0f, 0.0f));
    }

    CreateButtonText(Button_Close, FText::FromString(TEXT("X")));

    auto AddTextToVerticalBox = [this](UVerticalBox* ParentBox, const FName& WidgetName, const FText& Text, const FMargin& SlotPadding, EHorizontalAlignment HorizontalAlignment) -> UTextBlock*
    {
        if (!WidgetTree || !ParentBox)
        {
            return nullptr;
        }

        UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);

        if (!TextBlock)
        {
            return nullptr;
        }

        TextBlock->SetText(Text);
        UVerticalBoxSlot* TextSlot = ParentBox->AddChildToVerticalBox(TextBlock);

        if (TextSlot)
        {
            TextSlot->SetHorizontalAlignment(HorizontalAlignment);
            TextSlot->SetPadding(SlotPadding);
        }

        return TextBlock;
    };

    auto AddButtonToHorizontalBox = [this](UHorizontalBox* ParentBox, const FName& WidgetName, const FText& Text) -> UButton*
    {
        if (!WidgetTree || !ParentBox)
        {
            return nullptr;
        }

        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);

        if (!Button)
        {
            return nullptr;
        }

        CreateButtonText(Button, Text);
        UHorizontalBoxSlot* ButtonSlot = ParentBox->AddChildToHorizontalBox(Button);

        if (ButtonSlot)
        {
            FSlateChildSize ButtonSize;
            ButtonSize.SizeRule = ESlateSizeRule::Automatic;
            ButtonSlot->SetSize(ButtonSize);
            ButtonSlot->SetVerticalAlignment(VAlign_Center);
            ButtonSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 0.0f));
        }

        return Button;
    };

    auto AddButtonToVerticalBox = [this](UVerticalBox* ParentBox, const FName& WidgetName, const FText& Text, const FMargin& SlotPadding) -> UButton*
    {
        if (!WidgetTree || !ParentBox)
        {
            return nullptr;
        }

        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);

        if (!Button)
        {
            return nullptr;
        }

        CreateButtonText(Button, Text);
        UVerticalBoxSlot* ButtonSlot = ParentBox->AddChildToVerticalBox(Button);

        if (ButtonSlot)
        {
            ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
            ButtonSlot->SetPadding(SlotPadding);
        }

        return Button;
    };

    auto AssignSlotWidgets = [this](int32 SlotIndex, UButton* CreateButton, UVerticalBox* EditorBox, UTextBlock* TitleText, UButton* PrevButton, UButton* NextButton, UImage* ClassIcon, UTextBlock* ClassNameText, UButton* EditButton, UButton* DeleteButton, UButton* ClassInfoButton)
    {
        if (SlotIndex == 0)
        {
            Button_Slot0_Create = CreateButton;
            SlotEditorBox_0 = EditorBox;
            Text_Slot0_Title = TitleText;
            Button_Slot0_Prev = PrevButton;
            Button_Slot0_Next = NextButton;
            Image_Slot0_ClassIcon = ClassIcon;
            Text_Slot0_ClassName = ClassNameText;
            Button_Slot0_Edit = EditButton;
            Button_Slot0_Delete = DeleteButton;
            Button_Slot0_ClassInfo = ClassInfoButton;
        }

        if (SlotIndex == 1)
        {
            Button_Slot1_Create = CreateButton;
            SlotEditorBox_1 = EditorBox;
            Text_Slot1_Title = TitleText;
            Button_Slot1_Prev = PrevButton;
            Button_Slot1_Next = NextButton;
            Image_Slot1_ClassIcon = ClassIcon;
            Text_Slot1_ClassName = ClassNameText;
            Button_Slot1_Edit = EditButton;
            Button_Slot1_Delete = DeleteButton;
            Button_Slot1_ClassInfo = ClassInfoButton;
        }

        if (SlotIndex == 2)
        {
            Button_Slot2_Create = CreateButton;
            SlotEditorBox_2 = EditorBox;
            Text_Slot2_Title = TitleText;
            Button_Slot2_Prev = PrevButton;
            Button_Slot2_Next = NextButton;
            Image_Slot2_ClassIcon = ClassIcon;
            Text_Slot2_ClassName = ClassNameText;
            Button_Slot2_Edit = EditButton;
            Button_Slot2_Delete = DeleteButton;
            Button_Slot2_ClassInfo = ClassInfoButton;
        }

        if (SlotIndex == 3)
        {
            Button_Slot3_Create = CreateButton;
            SlotEditorBox_3 = EditorBox;
            Text_Slot3_Title = TitleText;
            Button_Slot3_Prev = PrevButton;
            Button_Slot3_Next = NextButton;
            Image_Slot3_ClassIcon = ClassIcon;
            Text_Slot3_ClassName = ClassNameText;
            Button_Slot3_Edit = EditButton;
            Button_Slot3_Delete = DeleteButton;
            Button_Slot3_ClassInfo = ClassInfoButton;
        }
    };

    for (int32 SlotIndex = 0; SlotIndex < AvailablePartyClassIds.Num(); ++SlotIndex)
    {
        const FText DisplayName = GetDisplayNameForClassId(AvailablePartyClassIds[SlotIndex]);
        UBorder* SlotPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("SlotPanel_%d"), SlotIndex)));
        UOverlay* SlotContentOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*FString::Printf(TEXT("SlotContentOverlay_%d"), SlotIndex)));
        UButton* CreateSlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("Button_Slot%d_Create"), SlotIndex)));
        UVerticalBox* SlotEditorBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("SlotEditorBox_%d"), SlotIndex)));

        if (!SlotPanel || !SlotContentOverlay || !CreateSlotButton || !SlotEditorBox)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Failed to create fallback party slot widgets. SlotIndex: %d"), SlotIndex);
            continue;
        }

        SlotPanel->SetVisibility(ESlateVisibility::Visible);
        SlotPanel->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.1f, 0.95f));
        SlotPanel->SetPadding(FMargin(12.0f, 10.0f, 12.0f, 12.0f));

        UHorizontalBoxSlot* SlotPanelSlot = BottomHorizontalBox->AddChildToHorizontalBox(SlotPanel);

        if (SlotPanelSlot)
        {
            FSlateChildSize SlotSize;
            SlotSize.SizeRule = ESlateSizeRule::Fill;
            SlotSize.Value = 1.0f;
            SlotPanelSlot->SetSize(SlotSize);
            SlotPanelSlot->SetHorizontalAlignment(HAlign_Fill);
            SlotPanelSlot->SetVerticalAlignment(VAlign_Fill);
            SlotPanelSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 0.0f));
        }

        SlotPanel->SetContent(SlotContentOverlay);
        CreateButtonText(CreateSlotButton, FText::FromString(TEXT("캐릭터 생성하기")));

        UOverlaySlot* CreateButtonSlot = SlotContentOverlay->AddChildToOverlay(CreateSlotButton);

        if (CreateButtonSlot)
        {
            CreateButtonSlot->SetHorizontalAlignment(HAlign_Center);
            CreateButtonSlot->SetVerticalAlignment(VAlign_Center);
        }

        UOverlaySlot* EditorBoxSlot = SlotContentOverlay->AddChildToOverlay(SlotEditorBox);

        if (EditorBoxSlot)
        {
            EditorBoxSlot->SetHorizontalAlignment(HAlign_Fill);
            EditorBoxSlot->SetVerticalAlignment(VAlign_Center);
        }

        UTextBlock* TitleText = AddTextToVerticalBox(SlotEditorBox, FName(*FString::Printf(TEXT("Text_Slot%d_Title"), SlotIndex)), DisplayName, FMargin(0.0f, 0.0f, 0.0f, 8.0f), HAlign_Center);
        UHorizontalBox* SelectorBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("SlotSelectorBox_%d"), SlotIndex)));

        if (SelectorBox)
        {
            UVerticalBoxSlot* SelectorBoxSlot = SlotEditorBox->AddChildToVerticalBox(SelectorBox);

            if (SelectorBoxSlot)
            {
                SelectorBoxSlot->SetHorizontalAlignment(HAlign_Fill);
                SelectorBoxSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 4.0f));
            }
        }

        UButton* PrevButton = AddButtonToHorizontalBox(SelectorBox, FName(*FString::Printf(TEXT("Button_Slot%d_Prev"), SlotIndex)), FText::FromString(TEXT("<")));
        UImage* ClassIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("Image_Slot%d_ClassIcon"), SlotIndex)));

        if (SelectorBox && ClassIcon)
        {
            UHorizontalBoxSlot* IconSlot = SelectorBox->AddChildToHorizontalBox(ClassIcon);

            if (IconSlot)
            {
                FSlateChildSize IconSize;
                IconSize.SizeRule = ESlateSizeRule::Fill;
                IconSize.Value = 1.0f;
                IconSlot->SetSize(IconSize);
                IconSlot->SetHorizontalAlignment(HAlign_Fill);
                IconSlot->SetVerticalAlignment(VAlign_Fill);
                IconSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 0.0f));
            }
        }

        UButton* NextButton = AddButtonToHorizontalBox(SelectorBox, FName(*FString::Printf(TEXT("Button_Slot%d_Next"), SlotIndex)), FText::FromString(TEXT(">")));
        UTextBlock* ClassNameText = AddTextToVerticalBox(SlotEditorBox, FName(*FString::Printf(TEXT("Text_Slot%d_ClassName"), SlotIndex)), DisplayName, FMargin(0.0f, 4.0f, 0.0f, 8.0f), HAlign_Center);
        UHorizontalBox* ActionBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("SlotActionBox_%d"), SlotIndex)));

        if (ActionBox)
        {
            UVerticalBoxSlot* ActionBoxSlot = SlotEditorBox->AddChildToVerticalBox(ActionBox);

            if (ActionBoxSlot)
            {
                ActionBoxSlot->SetHorizontalAlignment(HAlign_Center);
                ActionBoxSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));
            }
        }

        UButton* EditButton = AddButtonToHorizontalBox(ActionBox, FName(*FString::Printf(TEXT("Button_Slot%d_Edit"), SlotIndex)), FText::FromString(TEXT("Edit")));
        UButton* DeleteButton = AddButtonToHorizontalBox(ActionBox, FName(*FString::Printf(TEXT("Button_Slot%d_Delete"), SlotIndex)), FText::FromString(TEXT("X")));
        UButton* ClassInfoButton = AddButtonToVerticalBox(SlotEditorBox, FName(*FString::Printf(TEXT("Button_Slot%d_ClassInfo"), SlotIndex)), FText::FromString(TEXT("클래스 정보")), FMargin(0.0f, 0.0f, 0.0f, 0.0f));

        AssignSlotWidgets(SlotIndex, CreateSlotButton, SlotEditorBox, TitleText, PrevButton, NextButton, ClassIcon, ClassNameText, EditButton, DeleteButton, ClassInfoButton);

        if (SlotIndex < AvailablePartyClassIds.Num() - 1)
        {
            USizeBox* DividerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("SlotDividerBox_%d"), SlotIndex)));
            UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("SlotDivider_%d"), SlotIndex)));

            if (!DividerBox || !Divider)
            {
                continue;
            }

            DividerBox->SetWidthOverride(1.0f);
            Divider->SetBrushColor(FLinearColor(0.35f, 0.33f, 0.25f, 0.65f));

            UHorizontalBoxSlot* DividerBoxSlot = BottomHorizontalBox->AddChildToHorizontalBox(DividerBox);

            if (DividerBoxSlot)
            {
                FSlateChildSize DividerSize;
                DividerSize.SizeRule = ESlateSizeRule::Automatic;
                DividerBoxSlot->SetSize(DividerSize);
                DividerBoxSlot->SetHorizontalAlignment(HAlign_Center);
                DividerBoxSlot->SetVerticalAlignment(VAlign_Fill);
            }

            USizeBoxSlot* DividerSlot = Cast<USizeBoxSlot>(DividerBox->AddChild(Divider));

            if (DividerSlot)
            {
                DividerSlot->SetHorizontalAlignment(HAlign_Fill);
                DividerSlot->SetVerticalAlignment(VAlign_Fill);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] Character creation party slot fallback layout was created in code."));
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

void UCharacterCreationWidget::ConfigureFullscreenInputBlocker()
{
    if (!FullscreenInputBlocker)
    {
        return;
    }

    FullscreenInputBlocker->SetVisibility(ESlateVisibility::Visible);
    FullscreenInputBlocker->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.01f));
}

void UCharacterCreationWidget::ConfigurePartySlotsFixedHeight()
{
    if (!PartySlotsFixedHeightBox)
    {
        if (BottomPanel && BottomHorizontalBox)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CharacterCreationWidget] PartySlotsFixedHeightBox is missing; regenerate WBP_CharacterCreationWidget to lock PartySlots height."));
        }

        return;
    }

    PartySlotsFixedHeightBox->SetHeightOverride(PartySlotsFixedHeight);
}

void UCharacterCreationWidget::InitializeClassSlotWidgetArrays()
{
    CreateSlotButtons = { Button_Slot0_Create, Button_Slot1_Create, Button_Slot2_Create, Button_Slot3_Create };
    SlotEditorBoxes = { SlotEditorBox_0, SlotEditorBox_1, SlotEditorBox_2, SlotEditorBox_3 };
    SlotTitleTexts = { Text_Slot0_Title, Text_Slot1_Title, Text_Slot2_Title, Text_Slot3_Title };
    PreviousClassButtons = { Button_Slot0_Prev, Button_Slot1_Prev, Button_Slot2_Prev, Button_Slot3_Prev };
    NextClassButtons = { Button_Slot0_Next, Button_Slot1_Next, Button_Slot2_Next, Button_Slot3_Next };
    ClassIconImages = { Image_Slot0_ClassIcon, Image_Slot1_ClassIcon, Image_Slot2_ClassIcon, Image_Slot3_ClassIcon };
    ClassNameTexts = { Text_Slot0_ClassName, Text_Slot1_ClassName, Text_Slot2_ClassName, Text_Slot3_ClassName };
    EditButtons = { Button_Slot0_Edit, Button_Slot1_Edit, Button_Slot2_Edit, Button_Slot3_Edit };
    DeleteButtons = { Button_Slot0_Delete, Button_Slot1_Delete, Button_Slot2_Delete, Button_Slot3_Delete };
    ClassInfoButtons = { Button_Slot0_ClassInfo, Button_Slot1_ClassInfo, Button_Slot2_ClassInfo, Button_Slot3_ClassInfo };
}

void UCharacterCreationWidget::InitializeClassSlots()
{
    SlotClassIds = AvailablePartyClassIds;
    SlotCharacterNames.SetNum(SlotClassIds.Num());
    SlotCreationStates.Empty();
    const bool bUseDeferredCreation = HasDeferredSlotCreationWidgets();

    for (int32 SlotIndex = 0; SlotIndex < SlotClassIds.Num(); ++SlotIndex)
    {
        uint8 InitialState = 1;

        if (bUseDeferredCreation)
        {
            InitialState = 0;
        }

        SlotCreationStates.Add(InitialState);
    }

    if (SlotClassIds.Num() > 0)
    {
        CurrentCharacterClassId = SlotClassIds[0];
    }
}

void UCharacterCreationWidget::RefreshClassSlotWidgets()
{
    for (int32 SlotIndex = 0; SlotIndex < SlotClassIds.Num(); ++SlotIndex)
    {
        SetSlotClass(SlotIndex, SlotClassIds[SlotIndex]);
        RefreshSlotVisibility(SlotIndex);
    }
}

void UCharacterCreationWidget::ChangeSlotClass(int32 SlotIndex, int32 Direction)
{
    if (!SlotClassIds.IsValidIndex(SlotIndex) || AvailablePartyClassIds.IsEmpty() || !IsSlotCreated(SlotIndex))
    {
        return;
    }

    int32 CurrentIndex = AvailablePartyClassIds.IndexOfByKey(SlotClassIds[SlotIndex]);
    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }

    int32 NextIndex = CurrentIndex + Direction;
    while (NextIndex < 0)
    {
        NextIndex += AvailablePartyClassIds.Num();
    }

    NextIndex %= AvailablePartyClassIds.Num();
    SetSlotClass(SlotIndex, AvailablePartyClassIds[NextIndex]);
}

void UCharacterCreationWidget::SetSlotClass(int32 SlotIndex, FName ClassId)
{
    if (!SlotClassIds.IsValidIndex(SlotIndex))
    {
        return;
    }

    SlotClassIds[SlotIndex] = ClassId;
    const FText DisplayName = GetDisplayNameForClassId(ClassId);

    if (SlotTitleTexts.IsValidIndex(SlotIndex) && SlotTitleTexts[SlotIndex])
    {
        SlotTitleTexts[SlotIndex]->SetText(SlotCharacterNames.IsValidIndex(SlotIndex) && !SlotCharacterNames[SlotIndex].IsEmpty() ? SlotCharacterNames[SlotIndex] : DisplayName);
    }

    if (ClassNameTexts.IsValidIndex(SlotIndex) && ClassNameTexts[SlotIndex])
    {
        ClassNameTexts[SlotIndex]->SetText(DisplayName);
    }

    if (ClassIconImages.IsValidIndex(SlotIndex) && ClassIconImages[SlotIndex])
    {
        const FProfessionDefinition* Definition = PartyDefinition ? PartyDefinition->Professions.Find(ClassId) : nullptr;
        ClassIconImages[SlotIndex]->SetBrushFromTexture(Definition ? Definition->Icon.Get() : nullptr);
        ClassIconImages[SlotIndex]->SetToolTipText(DisplayName);
    }
    if (SlotIndex == 0)
    {
        CurrentCharacterClassId = ClassId;
    }

    RefreshSlotVisibility(SlotIndex);

    if (IsSlotCreated(SlotIndex))
    {
        UpdatePreviewStageSlot(SlotIndex, ClassId);
    }
    else
    {
        ClearPreviewStageSlot(SlotIndex);
    }
}

void UCharacterCreationWidget::CreateCharacterInSlot(int32 SlotIndex)
{
    if (!SlotClassIds.IsValidIndex(SlotIndex) || !SlotCreationStates.IsValidIndex(SlotIndex))
    {
        return;
    }

    SlotCreationStates[SlotIndex] = 1;
    SetSlotClass(SlotIndex, SlotClassIds[SlotIndex]);
    UE_LOG(LogTemp, Log, TEXT("[CharacterCreationWidget] Character creation panel opened. SlotIndex: %d, ClassId: %s"), SlotIndex, *SlotClassIds[SlotIndex].ToString());
}

void UCharacterCreationWidget::ClearCharacterSlot(int32 SlotIndex)
{
    if (!SlotCreationStates.IsValidIndex(SlotIndex))
    {
        return;
    }

    SlotCreationStates[SlotIndex] = 0;

    if (SlotCharacterNames.IsValidIndex(SlotIndex))
    {
        SlotCharacterNames[SlotIndex] = FText::GetEmpty();
    }

    ClearPreviewStageSlot(SlotIndex);
    RefreshSlotVisibility(SlotIndex);
    UE_LOG(LogTemp, Log, TEXT("[CharacterCreationWidget] Character creation panel closed. SlotIndex: %d"), SlotIndex);
}

void UCharacterCreationWidget::RefreshSlotVisibility(int32 SlotIndex)
{
    const bool bCreated = IsSlotCreated(SlotIndex);

    if (CreateSlotButtons.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(CreateSlotButtons[SlotIndex].Get(), !bCreated);
    }

    if (SlotEditorBoxes.IsValidIndex(SlotIndex) && SlotEditorBoxes[SlotIndex])
    {
        SetWidgetVisible(SlotEditorBoxes[SlotIndex].Get(), bCreated);
        return;
    }

    if (PreviousClassButtons.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(PreviousClassButtons[SlotIndex].Get(), bCreated);
    }

    if (NextClassButtons.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(NextClassButtons[SlotIndex].Get(), bCreated);
    }

    if (ClassIconImages.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(ClassIconImages[SlotIndex].Get(), bCreated);
    }

    if (SlotTitleTexts.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(SlotTitleTexts[SlotIndex].Get(), bCreated);
    }

    if (ClassNameTexts.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(ClassNameTexts[SlotIndex].Get(), bCreated);
    }

    if (EditButtons.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(EditButtons[SlotIndex].Get(), bCreated);
    }

    if (DeleteButtons.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(DeleteButtons[SlotIndex].Get(), bCreated);
    }

    if (ClassInfoButtons.IsValidIndex(SlotIndex))
    {
        SetWidgetVisible(ClassInfoButtons[SlotIndex].Get(), bCreated);
    }
}

void UCharacterCreationWidget::SetWidgetVisible(UWidget* Widget, bool bIsVisible) const
{
    if (!Widget)
    {
        return;
    }

    if (bIsVisible)
    {
        Widget->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    Widget->SetVisibility(ESlateVisibility::Collapsed);
}

bool UCharacterCreationWidget::IsSlotCreated(int32 SlotIndex) const
{
    if (!SlotCreationStates.IsValidIndex(SlotIndex))
    {
        return false;
    }

    return SlotCreationStates[SlotIndex] != 0;
}

bool UCharacterCreationWidget::HasDeferredSlotCreationWidgets() const
{
    for (const TObjectPtr<UButton>& CreateSlotButton : CreateSlotButtons)
    {
        if (CreateSlotButton)
        {
            return true;
        }
    }

    for (const TObjectPtr<UVerticalBox>& SlotEditorBox : SlotEditorBoxes)
    {
        if (SlotEditorBox)
        {
            return true;
        }
    }

    return false;
}

FText UCharacterCreationWidget::GetDisplayNameForClassId(FName ClassId) const
{
    const FProfessionDefinition* Definition = PartyDefinition ? PartyDefinition->Professions.Find(ClassId) : nullptr;
    return Definition ? Definition->DisplayName : FText::FromName(ClassId);
}

void UCharacterCreationWidget::UpdatePreviewStageSlot(int32 SlotIndex, FName ClassId)
{
    AMainMenuPlayerController* MainMenuPlayerController = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    if (!MainMenuPlayerController)
    {
        return;
    }

    AMainMenuPreviewStage* PreviewStage = MainMenuPlayerController->GetPreviewStage();
    if (PreviewStage)
    {
        PreviewStage->SetPreviewActorForSlot(SlotIndex, ClassId);
    }
}

void UCharacterCreationWidget::ClearPreviewStageSlot(int32 SlotIndex)
{
    AMainMenuPlayerController* MainMenuPlayerController = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    if (!MainMenuPlayerController)
    {
        return;
    }

    AMainMenuPreviewStage* PreviewStage = MainMenuPlayerController->GetPreviewStage();
    if (PreviewStage)
    {
        PreviewStage->ClearPreviewActorForSlot(SlotIndex);
    }
}

void UCharacterCreationWidget::LogSlotAction(int32 SlotIndex, const TCHAR* ActionName) const
{
    if (!SlotClassIds.IsValidIndex(SlotIndex))
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CharacterCreationWidget] Slot action requested. SlotIndex: %d, ClassId: %s, Action: %s"), SlotIndex, *SlotClassIds[SlotIndex].ToString(), ActionName);
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
    UButtonSlot* ButtonSlot = Cast<UButtonSlot>(ParentButton->AddChild(ButtonTextBlock));

    if (ButtonSlot)
    {
        ButtonSlot->SetHorizontalAlignment(HAlign_Center);
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
        ButtonSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 8.0f));
    }

    return ButtonTextBlock;
}

void UCharacterCreationWidget::HandleNameTextChanged(const FText& NewText)
{
    SetCharacterName(NewText);
}

void UCharacterCreationWidget::HandleWarriorClicked()
{
    SelectCharacterClass(TEXT("StableHand"));
    RefreshPreview();
}

void UCharacterCreationWidget::HandleArcherClicked()
{
    SelectCharacterClass(TEXT("Hunter"));
    RefreshPreview();
}

void UCharacterCreationWidget::HandleMageClicked()
{
    SelectCharacterClass(TEXT("Scholar"));
    RefreshPreview();
}

void UCharacterCreationWidget::HandleBackClicked()
{
    RequestBack();
}

void UCharacterCreationWidget::HandleCloseClicked()
{
    RequestBack();
}

void UCharacterCreationWidget::HandleStartGameClicked()
{
    RequestStartGame();
}

void UCharacterCreationWidget::HandleSlot0CreateClicked()
{
    CreateCharacterInSlot(0);
}

void UCharacterCreationWidget::HandleSlot0PrevClicked()
{
    ChangeSlotClass(0, -1);
}

void UCharacterCreationWidget::HandleSlot0NextClicked()
{
    ChangeSlotClass(0, 1);
}

void UCharacterCreationWidget::HandleSlot0EditClicked()
{
    ShowSlotDetails(0, true);
}

void UCharacterCreationWidget::HandleSlot0DeleteClicked()
{
    ClearCharacterSlot(0);
}

void UCharacterCreationWidget::HandleSlot0ClassInfoClicked()
{
    ShowSlotDetails(0, false);
}

void UCharacterCreationWidget::HandleSlot1CreateClicked()
{
    CreateCharacterInSlot(1);
}

void UCharacterCreationWidget::HandleSlot1PrevClicked()
{
    ChangeSlotClass(1, -1);
}

void UCharacterCreationWidget::HandleSlot1NextClicked()
{
    ChangeSlotClass(1, 1);
}

void UCharacterCreationWidget::HandleSlot1EditClicked()
{
    ShowSlotDetails(1, true);
}

void UCharacterCreationWidget::HandleSlot1DeleteClicked()
{
    ClearCharacterSlot(1);
}

void UCharacterCreationWidget::HandleSlot1ClassInfoClicked()
{
    ShowSlotDetails(1, false);
}

void UCharacterCreationWidget::HandleSlot2CreateClicked()
{
    CreateCharacterInSlot(2);
}

void UCharacterCreationWidget::HandleSlot2PrevClicked()
{
    ChangeSlotClass(2, -1);
}

void UCharacterCreationWidget::HandleSlot2NextClicked()
{
    ChangeSlotClass(2, 1);
}

void UCharacterCreationWidget::HandleSlot2EditClicked()
{
    ShowSlotDetails(2, true);
}

void UCharacterCreationWidget::HandleSlot2DeleteClicked()
{
    ClearCharacterSlot(2);
}

void UCharacterCreationWidget::HandleSlot2ClassInfoClicked()
{
    ShowSlotDetails(2, false);
}

void UCharacterCreationWidget::HandleSlot3CreateClicked()
{
    CreateCharacterInSlot(3);
}

void UCharacterCreationWidget::HandleSlot3PrevClicked()
{
    ChangeSlotClass(3, -1);
}

void UCharacterCreationWidget::HandleSlot3NextClicked()
{
    ChangeSlotClass(3, 1);
}

void UCharacterCreationWidget::HandleSlot3EditClicked()
{
    ShowSlotDetails(3, true);
}

void UCharacterCreationWidget::HandleSlot3DeleteClicked()
{
    ClearCharacterSlot(3);
}

void UCharacterCreationWidget::HandleSlot3ClassInfoClicked()
{
    ShowSlotDetails(3, false);
}

void UCharacterCreationWidget::SetCharacterName(const FText& NewName)
{
    CurrentCharacterName = NewName;
    SetSlotCharacterName(0, NewName);
}

void UCharacterCreationWidget::SetSlotCharacterName(int32 SlotIndex, const FText& NewName)
{
    if (SlotCharacterNames.IsValidIndex(SlotIndex))
    {
        SlotCharacterNames[SlotIndex] = NewName;
    }
}

TArray<FRunPartyMember> UCharacterCreationWidget::GetPartyMembers() const
{
    TArray<FRunPartyMember> PartyMembers;

    for (int32 SlotIndex = 0; SlotIndex < SlotClassIds.Num(); ++SlotIndex)
    {
        FRunPartyMember& Member = PartyMembers.AddDefaulted_GetRef();
        Member.SlotIndex = SlotIndex;
        Member.ClassId = SlotClassIds[SlotIndex];
        Member.bCreated = IsSlotCreated(SlotIndex);

        if (SlotCharacterNames.IsValidIndex(SlotIndex))
        {
            Member.CharacterName = SlotCharacterNames[SlotIndex];
        }

        // Unedited slot names use a stable, readable default until the name editor is expanded.
        // 이름 편집 확장 전에는 수정하지 않은 슬롯에 알아보기 쉬운 기본 이름을 사용합니다.
        if (Member.bCreated && Member.CharacterName.ToString().TrimStartAndEnd().IsEmpty())
        {
            Member.CharacterName = FText::FromString(FString::Printf(TEXT("%s %d"), *GetDisplayNameForClassId(Member.ClassId).ToString(), SlotIndex + 1));
        }
    }

    return PartyMembers;
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
    return GetDisplayNameForClassId(CurrentCharacterClassId);
}

FText UCharacterCreationWidget::GetStatPreviewText() const
{
    return PartyDefinition ? PartyDefinition->GetProfessionDetails(CurrentCharacterClassId) : FText::FromString(TEXT("직업 데이터를 불러올 수 없습니다."));
}

void UCharacterCreationWidget::RequestBack()
{
    if (DetailSlot != INDEX_NONE)
    {
        CloseSlotDetails();
        return;
    }
    AMainMenuPlayerController* MainMenuPlayerController = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    if (MainMenuPlayerController && MainMenuPlayerController->GetMainMenuRootWidget())
    {
        MainMenuPlayerController->GetMainMenuRootWidget()->SetMainStackHiddenByMenu(false);
    }

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

    FText Error;

    if (DetailSlot != INDEX_NONE || !PartyDefinition)
    {
        return;
    }
    for (const FRunPartyMember& Member : GetPartyMembers())
    {
        FProfessionDefinition Definition;
        if (Member.bCreated && !PartyDefinition->ResolveProfession(Member.ClassId, Definition))
        {
            if (Text_StartGameStatus)
            {
                Text_StartGameStatus->SetText(FText::FromString(TEXT("직업 전투 설정을 확인해 주세요.")));
            }
            return;
        }
    }
    GetGameInstance()->GetSubsystem<URunStateSubsystem>()->PartyDefinition = PartyDefinition;
    if (!MainMenuPlayerController->StartNewGameFromParty(GetPartyMembers(), Error))
    {
        if (Text_StartGameStatus)
        {
            Text_StartGameStatus->SetText(Error);
        }

        if (Button_StartGame)
        {
            Button_StartGame->SetToolTipText(Error);
        }
    }
}

void UCharacterCreationWidget::BuildDetailPanel()
{
    if (!WidgetTree || !WidgetTree->RootWidget || DetailPanel)
    {
        return;
    }
    DetailUnderlyingRoot = WidgetTree->RootWidget;
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ProfessionOverlay"));
    WidgetTree->RootWidget = Root;
    UOverlaySlot* UnderlyingSlot = Root->AddChildToOverlay(DetailUnderlyingRoot);
    UnderlyingSlot->SetHorizontalAlignment(HAlign_Fill);
    UnderlyingSlot->SetVerticalAlignment(VAlign_Fill);
    DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfessionDetailPanel"));
    DetailPanel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f));
    DetailPanel->SetPadding(FMargin(32.0f));
    UOverlaySlot* DetailOverlaySlot = Root->AddChildToOverlay(DetailPanel);
    DetailOverlaySlot->SetHorizontalAlignment(HAlign_Center);
    DetailOverlaySlot->SetVerticalAlignment(VAlign_Center);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Size->SetWidthOverride(520.0f);
    DetailPanel->SetContent(Size);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Size->SetContent(Content);
    DetailName = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ProfessionNameInput"));
    DetailName->SetHintText(FText::FromString(TEXT("캐릭터 이름")));
    Content->AddChildToVerticalBox(DetailName)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    DetailClass = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ProfessionClassSelect"));
    for (FName ClassId : AvailablePartyClassIds)
    {
        DetailClass->AddOption(GetDisplayNameForClassId(ClassId).ToString());
    }
    DetailClass->OnSelectionChanged.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleDetailClassChanged);
    Content->AddChildToVerticalBox(DetailClass)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfessionDetailText"));
    DetailText->SetAutoWrapText(true);
    Content->AddChildToVerticalBox(DetailText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
    DetailError = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfessionDetailError"));
    DetailError->SetColorAndOpacity(FLinearColor(1.0f, 0.45f, 0.35f));
    Content->AddChildToVerticalBox(DetailError);
    DetailSave = CreateButton(Content, FText::FromString(TEXT("저장")));
    DetailSave->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::SaveSlotDetails);
    UButton* Close = CreateButton(Content, FText::FromString(TEXT("닫기 / 취소")));
    Close->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::CloseSlotDetails);
    DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UCharacterCreationWidget::ShowSlotDetails(int32 SlotIndex, bool bEditable)
{
    if (!DetailPanel || !IsSlotCreated(SlotIndex) || !SlotClassIds.IsValidIndex(SlotIndex))
    {
        return;
    }
    DetailSlot = SlotIndex;
    bDetailEditable = bEditable;
    DetailError->SetText(FText::GetEmpty());
    DetailName->SetText(GetPartyMembers()[SlotIndex].CharacterName);
    DetailName->SetIsReadOnly(!bEditable);
    DetailClass->SetSelectedIndex(AvailablePartyClassIds.IndexOfByKey(SlotClassIds[SlotIndex]));
    DetailClass->SetIsEnabled(bEditable);
    DetailText->SetText(PartyDefinition ? PartyDefinition->GetProfessionDetails(SlotClassIds[SlotIndex]) : FText::GetEmpty());
    DetailSave->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    DetailUnderlyingRoot->SetIsEnabled(false);
    DetailPanel->SetVisibility(ESlateVisibility::Visible);
    if (bEditable)
    {
        DetailName->SetKeyboardFocus();
    }
}

void UCharacterCreationWidget::HandleDetailClassChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    const int32 Index = DetailClass->GetSelectedIndex();
    if (AvailablePartyClassIds.IsValidIndex(Index) && PartyDefinition)
    {
        DetailText->SetText(PartyDefinition->GetProfessionDetails(AvailablePartyClassIds[Index]));
    }
}

void UCharacterCreationWidget::SaveSlotDetails()
{
    if (!bDetailEditable || DetailSlot == INDEX_NONE || !IsSlotCreated(DetailSlot))
    {
        return;
    }
    const FString Name = DetailName->GetText().ToString().TrimStartAndEnd();
    const int32 Index = DetailClass->GetSelectedIndex();
    FProfessionDefinition Definition;
    if (Name.IsEmpty() || Name.Len() > 32)
    {
        DetailError->SetText(FText::FromString(TEXT("이름은 1~32자로 입력해 주세요.")));
        return;
    }
    if (!AvailablePartyClassIds.IsValidIndex(Index) || !PartyDefinition || !PartyDefinition->ResolveProfession(AvailablePartyClassIds[Index], Definition))
    {
        DetailError->SetText(FText::FromString(TEXT("직업 전투 설정을 확인해 주세요.")));
        return;
    }
    SetSlotCharacterName(DetailSlot, FText::FromString(Name));
    SetSlotClass(DetailSlot, AvailablePartyClassIds[Index]);
    CloseSlotDetails();
}

void UCharacterCreationWidget::CloseSlotDetails()
{
    DetailSlot = INDEX_NONE;
    bDetailEditable = false;
    if (DetailPanel)
    {
        DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
        DetailUnderlyingRoot->SetIsEnabled(true);
    }
}

void UCharacterCreationWidget::NativeOnDeactivated()
{
    CloseSlotDetails();
    Super::NativeOnDeactivated();
}
