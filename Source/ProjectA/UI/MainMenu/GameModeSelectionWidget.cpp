#include "UI/MainMenu/GameModeSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"
#include "Engine/GameInstance.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "UI/MainMenu/DevelopmentCoopWidget.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/Theme/DemonicUITheme.h"

UGameModeSelectionWidget::UGameModeSelectionWidget()
{
    bIsBackHandler = true;
    bAutoRestoreFocus = true;
}

TOptional<FUIInputConfig> UGameModeSelectionWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UButton* UGameModeSelectionWidget::AddButton(UVerticalBox* Parent, FName Name, const FText& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetJustification(ETextJustify::Center);
    CastChecked<UButtonSlot>(Button->AddChild(Label))->SetPadding(FMargin(24.0f, 16.0f));
    Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 6.0f));
    return Button;
}

void UGameModeSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    const UDemonicUITheme& Theme = UDemonicUITheme::Get();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Theme.StyleBackdrop(Background);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(620.0f);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Size);
    ContentSlot->SetHorizontalAlignment(HAlign_Center);
    ContentSlot->SetVerticalAlignment(VAlign_Center);
    ContentSlot->SetPadding(FMargin(24.0f));
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Theme.StylePanel(Panel);
    Panel->SetPadding(FMargin(40.0f));
    Size->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("GameModeSelection", "Title", "게임 모드 선택"));
    Title->SetJustification(ETextJustify::Center);
    Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
    SinglePlayerButton = AddButton(Content, TEXT("Button_SinglePlayer"), NSLOCTEXT("GameModeSelection", "SinglePlayer", "싱글플레이"));
    SinglePlayerButton->OnClicked.AddUniqueDynamic(this, &UGameModeSelectionWidget::HandleSinglePlayer);
    UTextBlock* SinglePlayerNotice = WidgetTree->ConstructWidget<UTextBlock>();
    SinglePlayerNotice->SetText(NSLOCTEXT("GameModeSelection", "SinglePlayerNotice", "파티를 만들고 혼자 여정을 시작합니다."));
    SinglePlayerNotice->SetAutoWrapText(true);
    SinglePlayerNotice->SetJustification(ETextJustify::Center);
    Content->AddChildToVerticalBox(SinglePlayerNotice)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 16.0f));

    MultiplayerButton = AddButton(Content, TEXT("Button_Multiplayer"), NSLOCTEXT("GameModeSelection", "Multiplayer", "멀티플레이"));
    MultiplayerButton->OnClicked.AddUniqueDynamic(this, &UGameModeSelectionWidget::HandleMultiplayer);
    MultiplayerNotice = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_MultiplayerAvailability"));
    MultiplayerNotice->SetAutoWrapText(true);
    MultiplayerNotice->SetJustification(ETextJustify::Center);
    Content->AddChildToVerticalBox(MultiplayerNotice)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 20.0f));
    Theme.AddDivider(WidgetTree, Content);
    BackButton = AddButton(Content, TEXT("Button_GameModeBack"), NSLOCTEXT("GameModeSelection", "Back", "뒤로가기"));
    BackButton->OnClicked.AddUniqueDynamic(this, &UGameModeSelectionWidget::HandleBack);

    Theme.ApplyControls(WidgetTree);
    Theme.StyleText(Title, true, 30);
    Theme.StyleText(SinglePlayerNotice, false, 16);
    Theme.StyleText(MultiplayerNotice, false, 16);
    Theme.StyleButton(SinglePlayerButton, true);
    RefreshAvailability();
}

void UGameModeSelectionWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        // A child screen may have restored the first menu while leaving this selection in the stack.
        // 하위 화면이 이 선택 화면을 스택에 남긴 채 첫 메뉴를 복원했을 수 있습니다.
        if (UMainMenuRootWidget* Root = Controller->GetMainMenuRootWidget()) Root->SetMainStackHiddenByMenu(true);
    }
    RefreshAvailability();
}

void UGameModeSelectionWidget::RefreshAvailability()
{
    const AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    const bool bHasMenu = Controller && Controller->GetMainMenuRootWidget();
    const UDevelopmentCoopSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>() : nullptr;
    const bool bMultiplayerAvailable = bHasMenu && UDevelopmentCoopSubsystem::IsAvailable() && Session && Controller->IsLocalController() && Controller->GetNetMode() == NM_Standalone;
    SinglePlayerButton->SetIsEnabled(bHasMenu);
    MultiplayerButton->SetIsEnabled(bMultiplayerAvailable);
    MultiplayerNotice->SetText(bMultiplayerAvailable ? NSLOCTEXT("GameModeSelection", "MultiplayerNotice", "같은 PC 또는 LAN에서 2~4명이 함께 플레이합니다.\nSteam 연결은 아직 지원하지 않습니다.") : NSLOCTEXT("GameModeSelection", "MultiplayerUnavailable", "현재 환경에서는 멀티플레이를 이용할 수 없습니다.\n같은 PC 또는 LAN의 2~4인 방을 지원하며, Steam 연결은 아직 지원하지 않습니다."));
}

UWidget* UGameModeSelectionWidget::NativeGetDesiredFocusTarget() const
{
    if (SinglePlayerButton && SinglePlayerButton->GetIsEnabled()) return SinglePlayerButton;
    return BackButton;
}

bool UGameModeSelectionWidget::NativeOnHandleBackAction()
{
    HandleBack();
    return true;
}

void UGameModeSelectionWidget::HandleSinglePlayer()
{
    if (!IsActivated()) return;
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer())) Controller->ShowCharacterCreationScreen();
}

void UGameModeSelectionWidget::HandleMultiplayer()
{
    if (!IsActivated()) return;
    RefreshAvailability();
    if (!MultiplayerButton->GetIsEnabled()) return;
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (UMainMenuRootWidget* Root = Controller->GetMainMenuRootWidget()) Root->PushMenuScreen(UDevelopmentCoopWidget::StaticClass());
    }
}

void UGameModeSelectionWidget::HandleBack()
{
    if (!IsActivated()) return;
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (UMainMenuRootWidget* Root = Controller->GetMainMenuRootWidget())
        {
            Root->ClearMenuStack();
            return;
        }
    }
    DeactivateWidget();
}
