#include "UI/MainMenu/MainMenuScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/MainMenu/OptionsWidget.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

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
    if (WidgetTree && WidgetTree->RootWidget)
    {
        UWidget* PreviousRoot = WidgetTree->RootWidget;
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
        WidgetTree->RootWidget = Root;
        UOverlaySlot* PreviousSlot = Root->AddChildToOverlay(PreviousRoot);
        PreviousSlot->SetHorizontalAlignment(HAlign_Fill);
        PreviousSlot->SetVerticalAlignment(VAlign_Fill);
        SaveStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SaveStatus"));
        SaveStatus->SetAutoWrapText(true);
        UOverlaySlot* StatusSlot = Root->AddChildToOverlay(SaveStatus);
        StatusSlot->SetVerticalAlignment(VAlign_Bottom);
        StatusSlot->SetPadding(FMargin(24.0f));
        // Extend the existing Designer menu only when an explicitly selected cooperative record is available.
        // 명시적으로 선택한 협동 기록이 있을 때만 기존 Designer 메뉴에 이어가기 영역을 표시합니다.
        ManagedResumePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ManagedResumePanel"));
        ManagedResumePanel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.95f));
        ManagedResumePanel->SetPadding(FMargin(20.0f));
        ManagedResumePanel->SetVisibility(ESlateVisibility::Collapsed);
        UOverlaySlot* ResumeSlot = Root->AddChildToOverlay(ManagedResumePanel);
        ResumeSlot->SetHorizontalAlignment(HAlign_Right);
        ResumeSlot->SetVerticalAlignment(VAlign_Center);
        ResumeSlot->SetPadding(FMargin(24.0f));
        USizeBox* ResumeSize = WidgetTree->ConstructWidget<USizeBox>();
        ResumeSize->SetWidthOverride(320.0f);
        ManagedResumePanel->SetContent(ResumeSize);
        UVerticalBox* ResumeContent = WidgetTree->ConstructWidget<UVerticalBox>();
        ResumeSize->SetContent(ResumeContent);
        ManagedResumeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ManagedResume"));
        ManagedResumeText->SetAutoWrapText(true);
        ResumeContent->AddChildToVerticalBox(ManagedResumeText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
        ConvertToSoloButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_ConvertToSolo"));
        CreateButtonText(ConvertToSoloButton, NSLOCTEXT("ManagedRunMenu", "Convert", "싱글로 전환하기"));
        ResumeContent->AddChildToVerticalBox(ConvertToSoloButton)->SetPadding(FMargin(0.0f, 4.0f));
        ConvertToSoloButton->OnClicked.AddUniqueDynamic(this, &UMainMenuScreenWidget::HandleConvertToSoloClicked);
        ResumeSoloButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_ResumeSolo"));
        CreateButtonText(ResumeSoloButton, NSLOCTEXT("ManagedRunMenu", "Resume", "싱글 진행 이어하기"));
        ResumeContent->AddChildToVerticalBox(ResumeSoloButton)->SetPadding(FMargin(0.0f, 4.0f));
        ResumeSoloButton->OnClicked.AddUniqueDynamic(this, &UMainMenuScreenWidget::HandleResumeSoloClicked);
    }
}

void UMainMenuScreenWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    FText Error;
    const bool bCanContinue = GetGameInstance()->GetSubsystem<URunStateSubsystem>()->CanContinueStandaloneSavedRun(Error);
    if (Button_Continue)
    {
        Button_Continue->SetIsEnabled(bCanContinue);
    }
    if (SaveStatus)
    {
        SaveStatus->SetText(bCanContinue ? FText::FromString(TEXT("이어하기: 마지막 체크포인트에서 복원합니다. 새 게임을 시작하면 기존 저장을 교체합니다.")) : Error);
    }
    URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    Run->OnRunStateChanged.RemoveAll(this);
    Run->OnRunStateChanged.AddUObject(this, &UMainMenuScreenWidget::RefreshResumeActions);
    RefreshResumeActions();
}

void UMainMenuScreenWidget::NativeOnDeactivated()
{
    if (UGameInstance* Instance = GetGameInstance())
    {
        if (URunStateSubsystem* Run = Instance->GetSubsystem<URunStateSubsystem>()) Run->OnRunStateChanged.RemoveAll(this);
    }
    Super::NativeOnDeactivated();
}

void UMainMenuScreenWidget::RefreshResumeActions()
{
    if (!ManagedResumePanel || !ManagedResumeText || !ConvertToSoloButton || !ResumeSoloButton) return;
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const bool bSelected = Run && Run->GetManagedResumeTarget().IsValid();
    ManagedResumePanel->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ConvertToSoloButton->SetIsEnabled(false);
    ResumeSoloButton->SetIsEnabled(false);
    ConvertToSoloButton->SetVisibility(ESlateVisibility::Collapsed);
    ResumeSoloButton->SetVisibility(ESlateVisibility::Collapsed);
    if (!bSelected) return;
    const AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    FManagedRunPreview Preview;
    FText Error;
    if (!Controller || !Controller->GetManagedResumePreview(Preview, Error))
    {
        ManagedResumeText->SetText(Error);
        return;
    }
    if (Preview.Phase == ERunPhase::Complete || Preview.Phase == ERunPhase::Defeat)
    {
        ManagedResumeText->SetText(NSLOCTEXT("ManagedRunMenu", "Finished", "종료된 파티 진행입니다."));
        return;
    }
    if (!Preview.Participation.HumanParticipants.Contains(Run->GetLocalCaller()))
    {
        ManagedResumeText->SetText(NSLOCTEXT("ManagedRunMenu", "AlreadyAI", "본인 캐릭터가 이미 AI로 전환된 진행입니다. 해당 Run에서는 인간 조작으로 복귀할 수 없습니다."));
        return;
    }
    const FRunParticipantData* Participant = Preview.Identity.OriginalParticipants.FindByPredicate([Run](const FRunParticipantData& Entry) { return Entry.AccountId == Run->GetLocalCaller(); });
    if (!Participant) return;
    const bool bConvert = Preview.Participation.HumanParticipants.Num() > 1;
    ManagedResumeText->SetText(FText::Format(bConvert ? NSLOCTEXT("ManagedRunMenu", "ConvertSummary", "파티 저장 · 본인 {0}번\n\n본인이 Host가 되어 이어갑니다. 나머지 캐릭터는 이 Run이 끝날 때까지 AI로 유지되며, 돌아와도 직접 조작할 수 없습니다.") : NSLOCTEXT("ManagedRunMenu", "ResumeSummary", "파티 저장 · 본인 {0}번\n\n본인 캐릭터로 이어갑니다. 나머지 캐릭터는 계속 AI가 조작합니다."), FText::AsNumber(Participant->JoinOrdinal)));
    ConvertToSoloButton->SetVisibility(bConvert ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ConvertToSoloButton->SetIsEnabled(bConvert);
    ResumeSoloButton->SetVisibility(bConvert ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ResumeSoloButton->SetIsEnabled(!bConvert);
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
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        FText Error;
        if (!Controller->ContinueSavedGame(Error) && SaveStatus)
        {
            SaveStatus->SetText(Error);
        }
    }
}

void UMainMenuScreenWidget::HandleConvertToSoloClicked()
{
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        FText Error;
        if (!Controller->ConvertManagedRunToSolo(Error))
        {
            RefreshResumeActions();
            if (ManagedResumeText) ManagedResumeText->SetText(Error);
        }
    }
}

void UMainMenuScreenWidget::HandleResumeSoloClicked()
{
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        FText Error;
        if (!Controller->ContinueManagedSoloRun(Error))
        {
            RefreshResumeActions();
            if (ManagedResumeText) ManagedResumeText->SetText(Error);
        }
    }
}

void UMainMenuScreenWidget::HandleOptionsClicked()
{
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        Controller->GetMainMenuRootWidget()->PushMenuScreen(UOptionsWidget::StaticClass());
    }
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
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
