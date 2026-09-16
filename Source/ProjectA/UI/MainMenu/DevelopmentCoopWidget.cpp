#include "UI/MainMenu/DevelopmentCoopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Engine/GameInstance.h"
#include "Game/Development/DevelopmentCoopLobby.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "Game/GameState/GameplayGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/Theme/DemonicUITheme.h"

TOptional<FUIInputConfig> UDevelopmentCoopWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UButton* UDevelopmentCoopWidget::AddButton(UVerticalBox* Box, const FName Name, const FText& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Button->SetContent(Label);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 6.f));
    return Button;
}

void UDevelopmentCoopWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    UDemonicUITheme::Get().StyleBackdrop(Background);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(620.f);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Size);
    ContentSlot->SetHorizontalAlignment(HAlign_Center);
    ContentSlot->SetVerticalAlignment(VAlign_Center);
    ContentSlot->SetPadding(FMargin(24.0f));
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
    UDemonicUITheme::Get().StylePanel(Frame);
    Frame->SetPadding(FMargin(32.0f));
    Size->SetContent(Frame);
    Frame->SetContent(Box);
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("개발용 협동")));
    Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    UTextBlock* Notice = WidgetTree->ConstructWidget<UTextBlock>();
    Notice->SetAutoWrapText(true);
    Notice->SetText(FText::FromString(TEXT("같은 PC 또는 LAN에서 새 전투를 확인하는 개발용 방입니다.\n각자 궁수 1명을 조작합니다. 저장 이어하기·Steam 초대는 지원하지 않습니다.")));
    Box->AddChildToVerticalBox(Notice)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    if (Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        Capacity = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), TEXT("Combo_DevCoopCapacity"));
        for (const FString& Value : { FString(TEXT("2")), FString(TEXT("3")), FString(TEXT("4")) }) Capacity->AddOption(Value);
        Capacity->SetSelectedOption(TEXT("2"));
        Box->AddChildToVerticalBox(Capacity);
        HostButton = AddButton(Box, TEXT("Button_DevCoopHost"), FText::FromString(TEXT("선택 인원으로 방 만들기 · Host 1번")));
        HostButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleHost);
        Address = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_DevCoopAddress"));
        Address->SetText(FText::FromString(TEXT("127.0.0.1:7777")));
        Address->SetHintText(FText::FromString(TEXT("Host IPv4:포트")));
        Box->AddChildToVerticalBox(Address)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
        JoinButton = AddButton(Box, TEXT("Button_DevCoopJoin"), FText::FromString(TEXT("주소로 참가")));
        JoinButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleJoin);
    }
    else
    {
        Roster = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_DevCoopRoster"));
        Box->AddChildToVerticalBox(Roster);
        ReadyButton = AddButton(Box, TEXT("Button_DevCoopReady"), FText::FromString(TEXT("준비 상태 변경")));
        ReadyButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleReady);
        StartButton = AddButton(Box, TEXT("Button_DevCoopStart"), FText::FromString(TEXT("전원 준비 후 시작 · Host 전용")));
        StartButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleStart);
    }
    Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_DevCoopStatus"));
    Status->SetAutoWrapText(true);
    Box->AddChildToVerticalBox(Status)->SetPadding(FMargin(0.f, 12.f));
    AddButton(Box, TEXT("Button_DevCoopBack"), FText::FromString(TEXT("취소 / 메뉴로 돌아가기")))->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleBack);
    UDemonicUITheme::Get().ApplyControls(WidgetTree);
    UDemonicUITheme::Get().StyleText(Title, true, 28);
    UDemonicUITheme::Get().StyleButton(HostButton ? HostButton.Get() : StartButton.Get(), true);
}

void UDevelopmentCoopWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!HostButton)
    {
        if (AGameplayGameState* State = GetWorld()->GetGameState<AGameplayGameState>()) RefreshLobby(State->GetDevelopmentLobby());
        return;
    }
    const UDevelopmentCoopSubsystem* Session = GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>();
    HostButton->SetIsEnabled(!Session->IsPending());
    JoinButton->SetIsEnabled(!Session->IsPending());
    Capacity->SetIsEnabled(!Session->IsPending());
    Address->SetIsEnabled(!Session->IsPending());
    if (!bShowLocalError) Status->SetText(Session->GetStatus());
}

void UDevelopmentCoopWidget::RefreshLobby(ADevelopmentCoopLobby* Lobby)
{
    if (!Lobby || !Roster || !ReadyButton || !StartButton) return;
    const AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer());
    const int32 PlayerId = Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
    FString Text;
    bool bFound = false;
    const TArray<FDevelopmentCoopMember>& Members = Lobby->GetMembers();
    for (int32 Index = 0; Index < Members.Num(); ++Index)
    {
        const FDevelopmentCoopMember& Member = Members[Index];
        const bool bSelf = Member.bConnected && PlayerId != INDEX_NONE && Member.PlayerId == PlayerId;
        if (bSelf)
        {
            bFound = true;
            bLocalReady = Member.bReady;
        }
        Text += FString::Printf(TEXT("%d번%s%s · %s\n"), Index + 1, Index == 0 ? TEXT(" Host") : TEXT(""), bSelf ? TEXT(" (나)") : TEXT(""), !Member.bAssigned ? TEXT("참가 대기") : !Member.bConnected ? TEXT("연결 끊김") : Member.bReady ? TEXT("준비 완료") : TEXT("준비 전"));
    }
    Roster->SetText(FText::FromString(Text));
    Status->SetText(Lobby->GetMessage());
    ReadyButton->SetIsEnabled(bFound && !Lobby->HasStarted() && !Lobby->IsClosed());
    if (UTextBlock* Label = Cast<UTextBlock>(ReadyButton->GetContent())) Label->SetText(FText::FromString(bLocalReady ? TEXT("준비 취소") : TEXT("준비 완료")));
    StartButton->SetIsEnabled(Controller && Controller->HasAuthority() && Lobby->CanStart());
}

void UDevelopmentCoopWidget::HandleHost()
{
    FText Error;
    bShowLocalError = !GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>()->Host(GetOwningPlayer(), FCString::Atoi(*Capacity->GetSelectedOption()), Error);
    if (bShowLocalError) Status->SetText(Error);
}

void UDevelopmentCoopWidget::HandleJoin()
{
    FText Error;
    bShowLocalError = !GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>()->Join(GetOwningPlayer(), Address->GetText().ToString(), Error);
    if (bShowLocalError) Status->SetText(Error);
}

void UDevelopmentCoopWidget::HandleBack()
{
    UDevelopmentCoopSubsystem* Session = GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>();
    AMainMenuPlayerController* Menu = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    if (Menu && !Session->IsPending()) Menu->GetMainMenuRootWidget()->ClearMenuStack();
    else Session->Leave(GetOwningPlayer());
}

void UDevelopmentCoopWidget::HandleReady()
{
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer())) Controller->ServerSetDevelopmentReady(!bLocalReady);
}

void UDevelopmentCoopWidget::HandleStart()
{
    if (AGameplayPlayerController* Controller = Cast<AGameplayPlayerController>(GetOwningPlayer())) Controller->RequestStartDevelopmentCoop();
}
