#include "UI/MainMenu/CharacterCreationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/CharacterAppearanceCatalog.h"
#include "Profession/ProfessionBase.h"
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
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Controller/MainMenuPlayerController.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "UI/MainMenu/MainMenuPreviewStage.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/Theme/DemonicUITheme.h"

namespace
{
const TArray<FName>& GetAvailablePartyClassIds()
{
    static const TArray<FName> ClassIds = UProfessionBase::GetPlayableIds();
    return ClassIds;
}

constexpr float PartySlotsFixedHeight = 260.0f;
}

UCharacterCreationWidget::UCharacterCreationWidget()
    : CurrentCharacterClassId(TEXT("Warrior"))
{
}

void UCharacterCreationWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!PartyDefinition)
    {
        PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
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
    BuildPlayerControlButtons();
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
            Label->SetText(GetDisplayNameForClassId(TEXT("Warrior")));
        }
        Button_Warrior->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleWarriorClicked);
    }

    if (Button_Archer)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Archer->GetChildAt(0)))
        {
            Label->SetText(GetDisplayNameForClassId(TEXT("Archer")));
        }
        Button_Archer->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleArcherClicked);
    }

    if (Button_Mage)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Mage->GetChildAt(0)))
        {
            Label->SetText(GetDisplayNameForClassId(TEXT("Mage")));
        }
        Button_Mage->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleMageClicked);
    }

    if (Button_Rogue)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Rogue->GetChildAt(0))) Label->SetText(GetDisplayNameForClassId(TEXT("Rogue")));
        Button_Rogue->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleRogueClicked);
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
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    Theme.ApplyControls(WidgetTree);
    Theme.StylePanel(BottomPanel);
    Theme.StylePanel(DetailPanel);
    Theme.StyleButton(Button_StartGame, true);
    Theme.StyleButton(DetailSave, true);
    for (int32 SlotIndex = 0; SlotIndex < FCharacterPartyDraft::Capacity; ++SlotIndex)
    {
        Theme.StylePanel(Cast<UBorder>(GetWidgetFromName(FName(*FString::Printf(TEXT("SlotPanel_%d"), SlotIndex)))));
    }
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

    Text_StartGameStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_StartGameStatus"));
    UOverlaySlot* StatusSlot = RootOverlay->AddChildToOverlay(Text_StartGameStatus);
    StatusSlot->SetHorizontalAlignment(HAlign_Center);
    StatusSlot->SetVerticalAlignment(VAlign_Top);
    StatusSlot->SetPadding(FMargin(24.0f, 96.0f, 24.0f, 0.0f));

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

    for (int32 SlotIndex = 0; SlotIndex < FCharacterPartyDraft::Capacity; ++SlotIndex)
    {
        const TArray<FName>& Classes = GetAvailablePartyClassIds();
        const FText DisplayName = Classes.IsEmpty() ? FText::GetEmpty() : GetDisplayNameForClassId(Classes[SlotIndex % Classes.Num()]);
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

        if (SlotIndex < FCharacterPartyDraft::Capacity - 1)
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
    UDemonicUITheme::Get().StylePanel(CenterPanelBackground);
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

    // Keep a baseline height while allowing all slot controls to fit at the current UI scale.
    // 기본 높이는 유지하되 현재 UI 배율에서 모든 슬롯 버튼이 들어가도록 확장을 허용합니다.
    PartySlotsFixedHeightBox->ClearHeightOverride();
    PartySlotsFixedHeightBox->SetMinDesiredHeight(PartySlotsFixedHeight);
}

void UCharacterCreationWidget::InitializeClassSlotWidgetArrays()
{
    SlotWidgets.SetNum(FCharacterPartyDraft::Capacity);
    for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
    {
        FCharacterCreationSlotWidgets& SlotPresentation = SlotWidgets[Index];
        auto Find = [this, Index](const TCHAR* Pattern) { return GetWidgetFromName(FName(*FString(Pattern).Replace(TEXT("%d"), *FString::FromInt(Index)))); };
        SlotPresentation.CreateButton = Cast<UButton>(Find(TEXT("Button_Slot%d_Create")));
        SlotPresentation.EditorBox = Cast<UVerticalBox>(Find(TEXT("SlotEditorBox_%d")));
        SlotPresentation.Title = Cast<UTextBlock>(Find(TEXT("Text_Slot%d_Title")));
        SlotPresentation.PreviousClass = Cast<UButton>(Find(TEXT("Button_Slot%d_Prev")));
        SlotPresentation.NextClass = Cast<UButton>(Find(TEXT("Button_Slot%d_Next")));
        SlotPresentation.ClassIcon = Cast<UImage>(Find(TEXT("Image_Slot%d_ClassIcon")));
        SlotPresentation.ClassName = Cast<UTextBlock>(Find(TEXT("Text_Slot%d_ClassName")));
        SlotPresentation.Edit = Cast<UButton>(Find(TEXT("Button_Slot%d_Edit")));
        SlotPresentation.Delete = Cast<UButton>(Find(TEXT("Button_Slot%d_Delete")));
        SlotPresentation.ClassInfo = Cast<UButton>(Find(TEXT("Button_Slot%d_ClassInfo")));
    }
}

void UCharacterCreationWidget::SyncDraftProperties()
{
    // Keep existing Blueprint property names as read-only projections of the draft.
    // 기존 Blueprint 속성 이름을 초안의 읽기 전용 표현으로 유지합니다.
    SlotClassIds.Reset(FCharacterPartyDraft::Capacity);
    SlotCharacterNames.Reset(FCharacterPartyDraft::Capacity);
    SlotCreationStates.Reset(FCharacterPartyDraft::Capacity);
    for (const FRunPartyMember& DraftMember : PartyDraft.ExportParty())
    {
        SlotClassIds.Add(DraftMember.ClassId);
        SlotCharacterNames.Add(DraftMember.CharacterName);
        SlotCreationStates.Add(DraftMember.bCreated ? 1 : 0);
    }
}

void UCharacterCreationWidget::InitializeClassSlots()
{
    PartyDraft.Reset(GetAvailablePartyClassIds(), !HasDeferredSlotCreationWidgets());
    SyncDraftProperties();
    CurrentCharacterClassId = SlotClassIds[0];
}

void UCharacterCreationWidget::BuildPlayerControlButtons()
{
    if (!WidgetTree) return;
    for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
    {
        FCharacterCreationSlotWidgets& SlotPresentation = SlotWidgets[SlotIndex];
        if (SlotPresentation.PlayerControl) continue;
        UHorizontalBox* Actions = SlotPresentation.Edit ? Cast<UHorizontalBox>(SlotPresentation.Edit->GetParent()) : nullptr;
        if (!Actions) continue;
        SlotPresentation.PlayerControl = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("Button_Slot%d_PlayerControl"), SlotIndex)));
        CreateButtonText(SlotPresentation.PlayerControl, FText::FromString(TEXT("직접 조작")));
        Actions->AddChildToHorizontalBox(SlotPresentation.PlayerControl)->SetPadding(FMargin(2.0f, 0.0f));
    }
    if (SlotWidgets[0].PlayerControl) SlotWidgets[0].PlayerControl->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot0ControlClicked);
    if (SlotWidgets[1].PlayerControl) SlotWidgets[1].PlayerControl->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot1ControlClicked);
    if (SlotWidgets[2].PlayerControl) SlotWidgets[2].PlayerControl->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot2ControlClicked);
    if (SlotWidgets[3].PlayerControl) SlotWidgets[3].PlayerControl->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleSlot3ControlClicked);
}

bool UCharacterCreationWidget::SelectPlayerControlledSlot(int32 SlotIndex)
{
    if (DetailSlot != INDEX_NONE || !PartyDraft.SelectControlled(SlotIndex)) return false;
    RefreshPlayerControlSelection();
    return true;
}

void UCharacterCreationWidget::RefreshPlayerControlSelection()
{
    const bool bHasSelection = IsSlotCreated(PartyDraft.GetControlledSlot());
    for (int32 SlotIndex = 0; SlotIndex < SlotClassIds.Num(); ++SlotIndex)
    {
        const bool bSelected = bHasSelection && PartyDraft.GetControlledSlot() == SlotIndex;
        if (SlotWidgets.IsValidIndex(SlotIndex) && SlotWidgets[SlotIndex].PlayerControl)
        {
            UButton* Button = SlotWidgets[SlotIndex].PlayerControl;
            Button->SetIsEnabled(IsSlotCreated(SlotIndex) && !bSelected);
            if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0))) Label->SetText(FText::FromString(bSelected ? TEXT("선택됨") : TEXT("직접 조작")));
        }
        if (SlotWidgets.IsValidIndex(SlotIndex) && SlotWidgets[SlotIndex].ClassName)
        {
            SlotWidgets[SlotIndex].ClassName->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *GetDisplayNameForClassId(SlotClassIds[SlotIndex]).ToString(), bSelected ? TEXT("직접 조작") : TEXT("AI"))));
        }
    }
    const FText Status = bHasSelection ? FText::FromString(FString::Printf(TEXT("직접 조작: 슬롯 %d · 나머지 동료는 AI가 조작합니다."), PartyDraft.GetControlledSlot() + 1)) : FText::FromString(TEXT("캐릭터를 생성한 뒤 직접 조작할 1명을 선택하세요. 나머지 동료는 AI가 조작합니다."));
    if (Text_StartGameStatus) Text_StartGameStatus->SetText(Status);
    if (Button_StartGame)
    {
        Button_StartGame->SetIsEnabled(bHasSelection);
        Button_StartGame->SetToolTipText(Status);
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
    if (!SlotClassIds.IsValidIndex(SlotIndex) || GetAvailablePartyClassIds().IsEmpty() || !IsSlotCreated(SlotIndex))
    {
        return;
    }

    int32 CurrentIndex = GetAvailablePartyClassIds().IndexOfByKey(SlotClassIds[SlotIndex]);
    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }

    int32 NextIndex = CurrentIndex + Direction;
    while (NextIndex < 0)
    {
        NextIndex += GetAvailablePartyClassIds().Num();
    }

    NextIndex %= GetAvailablePartyClassIds().Num();
    SetSlotClass(SlotIndex, GetAvailablePartyClassIds()[NextIndex]);
}

void UCharacterCreationWidget::SetSlotClass(int32 SlotIndex, FName ClassId)
{
    if (!UProfessionBase::FindProfession(ClassId)) return;
    if (!SlotClassIds.IsValidIndex(SlotIndex))
    {
        return;
    }

    if (!PartyDraft.SetClass(SlotIndex, ClassId)) return;
    SyncDraftProperties();
    const FText DisplayName = GetDisplayNameForClassId(ClassId);

    if (SlotWidgets.IsValidIndex(SlotIndex) && SlotWidgets[SlotIndex].Title)
    {
        SlotWidgets[SlotIndex].Title->SetText(SlotCharacterNames.IsValidIndex(SlotIndex) && !SlotCharacterNames[SlotIndex].IsEmpty() ? SlotCharacterNames[SlotIndex] : DisplayName);
    }

    if (SlotWidgets.IsValidIndex(SlotIndex) && SlotWidgets[SlotIndex].ClassName)
    {
        SlotWidgets[SlotIndex].ClassName->SetText(DisplayName);
    }

    if (SlotWidgets.IsValidIndex(SlotIndex) && SlotWidgets[SlotIndex].ClassIcon)
    {
        const FProfessionDefinition* Definition = PartyDefinition ? PartyDefinition->Professions.Find(ClassId) : nullptr;
        SlotWidgets[SlotIndex].ClassIcon->SetBrushFromTexture(Definition ? Definition->Icon.Get() : nullptr);
        SlotWidgets[SlotIndex].ClassIcon->SetToolTipText(DisplayName);
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
    RefreshPlayerControlSelection();
}

void UCharacterCreationWidget::CreateCharacterInSlot(int32 SlotIndex)
{
    if (!PartyDraft.Create(SlotIndex)) return;
    SyncDraftProperties();
    SetSlotClass(SlotIndex, SlotClassIds[SlotIndex]);
    ShowSlotDetails(SlotIndex, true);
    bDetailNewCharacter = DetailSlot == SlotIndex;
    UE_LOG(LogTemp, Log, TEXT("[CharacterCreationWidget] Character creation panel opened. SlotIndex: %d, ClassId: %s"), SlotIndex, *SlotClassIds[SlotIndex].ToString());
}

void UCharacterCreationWidget::ClearCharacterSlot(int32 SlotIndex)
{
    if (!PartyDraft.Clear(SlotIndex)) return;
    SyncDraftProperties();
    ClearPreviewStageSlot(SlotIndex);
    RefreshSlotVisibility(SlotIndex);
    RefreshPlayerControlSelection();
    UE_LOG(LogTemp, Log, TEXT("[CharacterCreationWidget] Character creation panel closed. SlotIndex: %d"), SlotIndex);
}

void FCharacterCreationSlotWidgets::SetCharacterVisible(bool bCreated) const
{
    if (CreateButton) CreateButton->SetVisibility(bCreated ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (EditorBox)
    {
        EditorBox->SetVisibility(bCreated ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        return;
    }
    const TArray<UWidget*> Controls = { PreviousClass.Get(), NextClass.Get(), ClassIcon.Get(), Title.Get(), ClassName.Get(), Edit.Get(), Delete.Get(), ClassInfo.Get(), PlayerControl.Get() };
    for (UWidget* Control : Controls)
    {
        if (Control) Control->SetVisibility(bCreated ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UCharacterCreationWidget::RefreshSlotVisibility(int32 SlotIndex)
{
    if (SlotWidgets.IsValidIndex(SlotIndex)) SlotWidgets[SlotIndex].SetCharacterVisible(PartyDraft.IsCreated(SlotIndex));
}

bool UCharacterCreationWidget::IsSlotCreated(int32 SlotIndex) const
{
    return PartyDraft.IsCreated(SlotIndex);
}

bool UCharacterCreationWidget::HasDeferredSlotCreationWidgets() const
{
    return SlotWidgets.ContainsByPredicate([](const FCharacterCreationSlotWidgets& SlotPresentation) { return SlotPresentation.CreateButton || SlotPresentation.EditorBox; });
}

FText UCharacterCreationWidget::GetDisplayNameForClassId(FName ClassId) const
{
    const UProfessionBase* Definition = UProfessionBase::FindProfession(ClassId);
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
        const FRunPartyMember* Member = PartyDraft.GetSlot(SlotIndex);
        if (Member) PreviewStage->SetPreviewAppearance(SlotIndex, GetAppearanceCatalog(ClassId), Member->Appearance);
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

void UCharacterCreationWidget::HandleRogueClicked()
{
    SelectCharacterClass(TEXT("Rogue"));
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

void UCharacterCreationWidget::HandleSlot0ControlClicked()
{
    SelectPlayerControlledSlot(0);
}

void UCharacterCreationWidget::HandleSlot1ControlClicked()
{
    SelectPlayerControlledSlot(1);
}

void UCharacterCreationWidget::HandleSlot2ControlClicked()
{
    SelectPlayerControlledSlot(2);
}

void UCharacterCreationWidget::HandleSlot3ControlClicked()
{
    SelectPlayerControlledSlot(3);
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
    if (PartyDraft.SetName(SlotIndex, NewName)) SyncDraftProperties();
}

TArray<FRunPartyMember> UCharacterCreationWidget::GetPartyMembers() const
{
    TArray<FRunPartyMember> PartyMembers = PartyDraft.ExportParty();
    for (FRunPartyMember& Member : PartyMembers)
    {
        // Preserve the readable default name until the user edits this slot.
        // 사용자가 슬롯 이름을 편집하기 전까지 읽기 쉬운 기본 이름을 유지합니다.
        if (Member.bCreated && Member.CharacterName.ToString().TrimStartAndEnd().IsEmpty()) Member.CharacterName = FText::FromString(FString::Printf(TEXT("%s %d"), *GetDisplayNameForClassId(Member.ClassId).ToString(), Member.SlotIndex + 1));
    }
    return PartyMembers;
}

void UCharacterCreationWidget::SelectCharacterClass(FName CharacterClassId)
{
    if (!UProfessionBase::FindProfession(CharacterClassId)) return;
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
    if (!IsSlotCreated(PartyDraft.GetControlledSlot()))
    {
        RefreshPlayerControlSelection();
        return;
    }
    for (const FRunPartyMember& Member : GetPartyMembers())
    {
        if (!Member.bCreated) continue;
        FProfessionDefinition Definition;
        if (!PartyDefinition->ResolveProfession(Member.ClassId, Definition))
        {
            if (Text_StartGameStatus)
            {
                Text_StartGameStatus->SetText(FText::FromString(TEXT("직업 전투 설정을 확인해 주세요.")));
            }
            return;
        }
        if (Definition.AppearanceCatalog && !Definition.AppearanceCatalog->ValidateSelection(Member.Appearance, Error))
        {
            if (Text_StartGameStatus) Text_StartGameStatus->SetText(Error);
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
    DetailBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfessionDetailBackdrop"));
    DetailBackdrop->SetBrushColor(FLinearColor::Transparent);
    UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(DetailBackdrop);
    BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
    BackdropSlot->SetVerticalAlignment(VAlign_Fill);
    DetailBackdrop->SetVisibility(ESlateVisibility::Collapsed);
    DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfessionDetailPanel"));
    DetailPanel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.98f));
    DetailPanel->SetPadding(FMargin(24.0f));
    UOverlaySlot* DetailOverlaySlot = Root->AddChildToOverlay(DetailPanel);
    DetailOverlaySlot->SetHorizontalAlignment(HAlign_Right);
    DetailOverlaySlot->SetVerticalAlignment(VAlign_Fill);
    DetailOverlaySlot->SetPadding(FMargin(24.0f));
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Size->SetWidthOverride(420.0f);
    DetailPanel->SetContent(Size);
    UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Size->SetContent(PanelContent);
    UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Heading->SetText(FText::FromString(TEXT("캐릭터 설정")));
    UDemonicUITheme::Get().StyleText(Heading, true, 24);
    PanelContent->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CharacterDetailScroll"));
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
    PanelContent->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Scroll->AddChild(Content);
    const auto AddLabel = [this, Content](const TCHAR* Text)
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        UDemonicUITheme::Get().StyleText(Label, false, 16);
        Content->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 6.0f));
    };
    AddLabel(TEXT("이름"));
    DetailName = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ProfessionNameInput"));
    DetailName->SetHintText(FText::FromString(TEXT("캐릭터 이름")));
    Content->AddChildToVerticalBox(DetailName)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    AddLabel(TEXT("직업"));
    DetailClass = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), TEXT("ProfessionClassSelect"));
    for (FName ClassId : GetAvailablePartyClassIds())
    {
        DetailClass->AddOption(GetDisplayNameForClassId(ClassId).ToString());
    }
    DetailClass->OnSelectionChanged.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleDetailClassChanged);
    Content->AddChildToVerticalBox(DetailClass)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    AddLabel(TEXT("몸체"));
    UHorizontalBox* BodyControls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AppearanceBodyControls"));
    Content->AddChildToVerticalBox(BodyControls)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    DetailPreviousBody = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AppearanceBodyPrevious"));
    DetailNextBody = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AppearanceBodyNext"));
    CreateButtonText(DetailPreviousBody, FText::FromString(TEXT("◀")));
    CreateButtonText(DetailNextBody, FText::FromString(TEXT("▶")));
    DetailPreviousBody->SetToolTipText(FText::FromString(TEXT("이전 몸체")));
    DetailNextBody->SetToolTipText(FText::FromString(TEXT("다음 몸체")));
    USizeBox* PreviousBodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PreviousBodySize->SetWidthOverride(64.0f);
    PreviousBodySize->SetContent(DetailPreviousBody);
    BodyControls->AddChildToHorizontalBox(PreviousBodySize);
    DetailBodyName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AppearanceBodyName"));
    DetailBodyName->SetJustification(ETextJustify::Center);
    DetailBodyName->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(DetailBodyName, false, 20);
    UHorizontalBoxSlot* BodyNameSlot = BodyControls->AddChildToHorizontalBox(DetailBodyName);
    BodyNameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BodyNameSlot->SetVerticalAlignment(VAlign_Center);
    BodyNameSlot->SetPadding(FMargin(12.0f, 0.0f));
    USizeBox* NextBodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    NextBodySize->SetWidthOverride(64.0f);
    NextBodySize->SetContent(DetailNextBody);
    BodyControls->AddChildToHorizontalBox(NextBodySize);
    DetailPreviousBody->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandlePreviousBody);
    DetailNextBody->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleNextBody);
    DetailBodyPosition = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AppearanceBodyPosition"));
    DetailBodyPosition->SetJustification(ETextJustify::Center);
    UDemonicUITheme::Get().StyleText(DetailBodyPosition, false, 14);
    Content->AddChildToVerticalBox(DetailBodyPosition)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    DetailResetBody = CreateButton(Content, FText::FromString(TEXT("기본 몸체로 초기화")));
    DetailResetBody->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::HandleResetBody);
    AddLabel(TEXT("미리보기"));
    UTextBlock* RotationHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AppearanceRotationHint"));
    RotationHint->SetText(FText::FromString(TEXT("캐릭터가 보이는 영역을 우클릭한 채 좌우로 드래그하여 회전")));
    RotationHint->SetAutoWrapText(true);
    UDemonicUITheme::Get().StyleText(RotationHint, false, 14);
    Content->AddChildToVerticalBox(RotationHint)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    AddLabel(TEXT("직업 정보"));
    DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfessionDetailText"));
    DetailText->SetAutoWrapText(true);
    Content->AddChildToVerticalBox(DetailText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
    DetailError = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfessionDetailError"));
    DetailError->SetColorAndOpacity(FLinearColor(1.0f, 0.45f, 0.35f));
    DetailError->SetAutoWrapText(true);
    PanelContent->AddChildToVerticalBox(DetailError)->SetPadding(FMargin(0.0f, 8.0f));
    DetailSave = CreateButton(PanelContent, FText::FromString(TEXT("저장")));
    DetailSave->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::SaveSlotDetails);
    UButton* Close = CreateButton(PanelContent, FText::FromString(TEXT("닫기 / 취소")));
    Close->OnClicked.AddUniqueDynamic(this, &UCharacterCreationWidget::CloseSlotDetails);
    DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UCharacterCreationWidget::ShowSlotDetails(int32 SlotIndex, bool bEditable)
{
    if (!DetailPanel || !IsSlotCreated(SlotIndex) || !SlotClassIds.IsValidIndex(SlotIndex))
    {
        return;
    }
    if (DetailSlot != INDEX_NONE) CloseSlotDetails();
    const FRunPartyMember* Member = PartyDraft.GetSlot(SlotIndex);
    if (!Member || !Member->bCreated) return;
    DetailSlot = SlotIndex;
    bDetailEditable = bEditable;
    bDetailNewCharacter = false;
    PendingClassId = Member->ClassId;
    PendingAppearance = Member->Appearance;
    DetailError->SetText(FText::GetEmpty());
    DetailName->SetText(GetPartyMembers()[SlotIndex].CharacterName);
    DetailName->SetIsReadOnly(!bEditable);
    bUpdatingDetail = true;
    DetailClass->SetSelectedIndex(GetAvailablePartyClassIds().IndexOfByKey(PendingClassId));
    bUpdatingDetail = false;
    DetailClass->SetIsEnabled(bEditable);
    DetailText->SetText(PartyDefinition ? PartyDefinition->GetProfessionDetails(SlotClassIds[SlotIndex]) : FText::GetEmpty());
    DetailSave->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RefreshBodySelector();
    DetailUnderlyingVisibility = DetailUnderlyingRoot->GetVisibility();
    DetailUnderlyingRoot->SetVisibility(ESlateVisibility::Collapsed);
    DetailBackdrop->SetVisibility(ESlateVisibility::Visible);
    DetailPanel->SetVisibility(ESlateVisibility::Visible);
    RefreshDetailPreview(true);
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (AMainMenuPreviewStage* Stage = Controller->GetPreviewStage()) Stage->SetFocusedPreviewSlot(DetailSlot);
    }
    if (bEditable)
    {
        DetailName->SetKeyboardFocus();
    }
}

void UCharacterCreationWidget::HandleDetailClassChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingDetail || !bDetailEditable || DetailSlot == INDEX_NONE) return;
    const int32 Index = DetailClass->GetSelectedIndex();
    if (GetAvailablePartyClassIds().IsValidIndex(Index) && PartyDefinition)
    {
        const FName ClassId = GetAvailablePartyClassIds()[Index];
        if (PendingClassId != ClassId) PendingAppearance.ItemIds.Reset();
        PendingClassId = ClassId;
        DetailText->SetText(PartyDefinition->GetProfessionDetails(ClassId));
        DetailError->SetText(FText::GetEmpty());
        RefreshBodySelector();
        RefreshDetailPreview(true);
    }
}

UCharacterAppearanceCatalog* UCharacterCreationWidget::GetAppearanceCatalog(FName ClassId) const
{
    FProfessionDefinition Definition;
    return PartyDefinition && PartyDefinition->ResolveProfession(ClassId, Definition) ? Definition.AppearanceCatalog.Get() : nullptr;
}

void UCharacterCreationWidget::RefreshBodySelector()
{
    if (!DetailBodyName || !DetailBodyPosition || !DetailPreviousBody || !DetailNextBody || !DetailResetBody) return;
    const UCharacterAppearanceCatalog* Catalog = GetAppearanceCatalog(PendingClassId);
    const FCharacterAppearanceBodyVariant* Body = Catalog ? Catalog->FindBodyVariant(PendingAppearance.BodyId) : nullptr;
    const int32 BodyCount = Catalog ? Catalog->BodyVariants.Num() : 0;
    const int32 BodyIndex = Body ? Catalog->BodyVariants.IndexOfByPredicate([Body](const FCharacterAppearanceBodyVariant& Variant) { return Variant.BodyId == Body->BodyId; }) : INDEX_NONE;
    DetailBodyName->SetText(Body ? Body->DisplayName : FText::FromString(TEXT("기본 몸체")));
    DetailBodyPosition->SetText(BodyIndex != INDEX_NONE ? FText::FromString(FString::Printf(TEXT("%d / %d"), BodyIndex + 1, BodyCount)) : FText::GetEmpty());
    DetailPreviousBody->SetIsEnabled(bDetailEditable && BodyCount > 1);
    DetailNextBody->SetIsEnabled(bDetailEditable && BodyCount > 1);
    DetailResetBody->SetVisibility(bDetailEditable && BodyCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UCharacterCreationWidget::ChangeBodyVariant(int32 Direction)
{
    const UCharacterAppearanceCatalog* Catalog = GetAppearanceCatalog(PendingClassId);
    if (!bDetailEditable || bUpdatingDetail || DetailSlot == INDEX_NONE || !Catalog || Catalog->BodyVariants.Num() < 2 || Direction == 0) return;
    const FCharacterAppearanceBodyVariant* Body = Catalog->FindBodyVariant(PendingAppearance.BodyId);
    const int32 CurrentIndex = Body ? Catalog->BodyVariants.IndexOfByPredicate([Body](const FCharacterAppearanceBodyVariant& Variant) { return Variant.BodyId == Body->BodyId; }) : INDEX_NONE;
    const int32 BodyCount = Catalog->BodyVariants.Num();
    // Stable body identifiers and catalog order support adding more body variants without UI changes.
    // 안정적인 몸체 식별자와 카탈로그 순서를 사용하여 UI 변경 없이 몸체 종류를 추가할 수 있습니다.
    const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + (Direction > 0 ? 1 : -1) + BodyCount) % BodyCount;
    FCharacterAppearanceSelection Selection = PendingAppearance;
    Selection.BodyId = Catalog->BodyVariants[NextIndex].BodyId;
    FText Error;
    if (!Catalog->ValidateSelection(Selection, Error))
    {
        DetailError->SetText(Error);
        return;
    }
    PendingAppearance = MoveTemp(Selection);
    DetailError->SetText(FText::GetEmpty());
    RefreshBodySelector();
    RefreshDetailPreview(false);
}

void UCharacterCreationWidget::HandlePreviousBody()
{
    ChangeBodyVariant(-1);
}

void UCharacterCreationWidget::HandleNextBody()
{
    ChangeBodyVariant(1);
}

void UCharacterCreationWidget::RefreshDetailPreview(bool bReplaceActor)
{
    if (DetailSlot == INDEX_NONE) return;
    AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    AMainMenuPreviewStage* Stage = Controller ? Controller->GetPreviewStage() : nullptr;
    if (!Stage) return;
    if (bReplaceActor) Stage->SetPreviewActorForSlot(DetailSlot, PendingClassId);
    if (!Stage->SetPreviewAppearance(DetailSlot, GetAppearanceCatalog(PendingClassId), PendingAppearance)) DetailError->SetText(FText::FromString(TEXT("몸체를 표시할 수 없습니다. 선택을 다시 확인해 주세요.")));
}

void UCharacterCreationWidget::HandleResetBody()
{
    if (!bDetailEditable || DetailSlot == INDEX_NONE) return;
    PendingAppearance.BodyId = NAME_None;
    DetailError->SetText(FText::GetEmpty());
    RefreshBodySelector();
    RefreshDetailPreview(false);
}

FReply UCharacterCreationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Start on the preview backdrop only; form controls keep their normal mouse behavior.
    // 미리보기 배경에서만 회전을 시작하고 설정 입력 요소는 기존 마우스 동작을 유지합니다.
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !InMouseEvent.IsTouchEvent() && IsActivated() && DetailSlot != INDEX_NONE && DetailPanel && DetailPanel->IsVisible() && InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()) && !DetailPanel->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
    {
        if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
        {
            if (Controller->GetPreviewStage())
            {
                PreviewDragUserIndex = InMouseEvent.GetUserIndex();
                return FReply::Handled().CaptureMouse(TakeWidget()).PreventThrottling();
            }
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCharacterCreationWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // A quick second press must also begin a new rotation gesture.
    // 빠르게 두 번째로 누른 우클릭도 새 회전 드래그를 시작합니다.
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton) return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

FReply UCharacterCreationWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (PreviewDragUserIndex != INDEX_NONE && InMouseEvent.GetUserIndex() == PreviewDragUserIndex && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        PreviewDragUserIndex = INDEX_NONE;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UCharacterCreationWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (PreviewDragUserIndex == INDEX_NONE || InMouseEvent.GetUserIndex() != PreviewDragUserIndex) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    if (!IsActivated() || DetailSlot == INDEX_NONE || !InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
    {
        PreviewDragUserIndex = INDEX_NONE;
        return FReply::Handled().ReleaseMouseCapture();
    }
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (AMainMenuPreviewStage* Stage = Controller->GetPreviewStage())
        {
            const FVector2D LocalDelta = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()) - InGeometry.AbsoluteToLocal(InMouseEvent.GetLastScreenSpacePosition());
            Stage->RotateFocusedPreview(LocalDelta.X * PreviewRotationSensitivity);
        }
    }
    return FReply::Handled();
}

void UCharacterCreationWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    PreviewDragUserIndex = INDEX_NONE;
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UCharacterCreationWidget::StopPreviewRotation()
{
    const int32 UserIndex = PreviewDragUserIndex;
    PreviewDragUserIndex = INDEX_NONE;
    // Release only this gesture's cursor capture when the details close or the widget is removed.
    // 상세창을 닫거나 위젯을 제거하면 이 드래그가 소유한 커서 캡처만 해제합니다.
    if (UserIndex != INDEX_NONE && FSlateApplication::IsInitialized() && HasMouseCaptureByUser(UserIndex))
    {
        if (const TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(UserIndex)) SlateUser->ReleaseCursorCapture();
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
    if (!GetAvailablePartyClassIds().IsValidIndex(Index) || !PartyDefinition || !PartyDefinition->ResolveProfession(GetAvailablePartyClassIds()[Index], Definition))
    {
        DetailError->SetText(FText::FromString(TEXT("직업 전투 설정을 확인해 주세요.")));
        return;
    }
    FText AppearanceError;
    if (Definition.AppearanceCatalog && !Definition.AppearanceCatalog->ValidateSelection(PendingAppearance, AppearanceError))
    {
        DetailError->SetText(AppearanceError);
        return;
    }
    SetSlotCharacterName(DetailSlot, FText::FromString(Name));
    SetSlotClass(DetailSlot, GetAvailablePartyClassIds()[Index]);
    PartyDraft.SetAppearance(DetailSlot, PendingAppearance);
    bDetailNewCharacter = false;
    CloseSlotDetails();
}

void UCharacterCreationWidget::CloseSlotDetails()
{
    StopPreviewRotation();
    const int32 PreviousSlot = DetailSlot;
    const bool bDiscardNewCharacter = bDetailNewCharacter;
    DetailSlot = INDEX_NONE;
    bDetailEditable = false;
    bDetailNewCharacter = false;
    PendingAppearance = FCharacterAppearanceSelection();
    PendingClassId = NAME_None;
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (AMainMenuPreviewStage* Stage = Controller->GetPreviewStage()) Stage->ClearPreviewFocus();
    }
    if (DetailPanel)
    {
        DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
        DetailBackdrop->SetVisibility(ESlateVisibility::Collapsed);
        if (PreviousSlot != INDEX_NONE) DetailUnderlyingRoot->SetVisibility(DetailUnderlyingVisibility);
    }
    if (bDiscardNewCharacter) ClearCharacterSlot(PreviousSlot);
    else if (IsSlotCreated(PreviousSlot)) UpdatePreviewStageSlot(PreviousSlot, PartyDraft.GetSlot(PreviousSlot)->ClassId);
}

void UCharacterCreationWidget::NativeOnDeactivated()
{
    CloseSlotDetails();
    for (int32 Index = 0; Index < SlotClassIds.Num(); ++Index)
    {
        ClearPreviewStageSlot(Index);
    }
    Super::NativeOnDeactivated();
}

void UCharacterCreationWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    // A new visit starts a fresh draft even when CommonUI reuses the widget instance.
    // CommonUI가 위젯 인스턴스를 재사용해도 재진입 시 새 파티 초안으로 시작합니다.
    InitializeClassSlots();
    RefreshClassSlotWidgets();
    RefreshPreview();
}

void UCharacterCreationWidget::NativeDestruct()
{
    StopPreviewRotation();
    Super::NativeDestruct();
}
