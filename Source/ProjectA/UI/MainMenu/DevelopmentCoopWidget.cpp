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
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/GameplayPlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Engine/GameInstance.h"
#include "Game/Development/DevelopmentCoopLobby.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "Game/Online/SteamDevelopmentLobby.h"
#include "Game/Online/SteamDevelopmentSubsystem.h"
#include "Game/GameState/GameplayGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/Theme/DemonicUITheme.h"

UDevelopmentCoopWidget::UDevelopmentCoopWidget()
{
    bIsBackHandler = true;
    bAutoRestoreFocus = true;
}

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
    Title->SetText(FText::FromString(TEXT("멀티플레이")));
    Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    UTextBlock* Notice = WidgetTree->ConstructWidget<UTextBlock>();
    Notice->SetAutoWrapText(true);
    Notice->SetText(FText::FromString(TEXT("같은 PC 또는 LAN에서 2~4명이 함께 플레이합니다.\n각자 궁수 1명을 조작합니다. 현재 저장 이어하기·Steam 초대는 지원하지 않습니다.")));
    Box->AddChildToVerticalBox(Notice)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    if (USteamDevelopmentSubsystem::IsRequested())
    {
        UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        Frame->SetContent(Scroll);
        Scroll->AddChild(Box);
        Size->SetMaxDesiredHeight(640.f);
        Title->SetText(FText::FromString(TEXT("Steam 480 연결 확인")));
        Notice->SetText(FText::FromString(TEXT("친구 전용 Steam 연결과 서버 인증을 확인합니다.\n캐릭터 생성·게임 Run·관리 저장·관전·MMR은 연결하지 않습니다.")));
        BuildSteamControls(Box);
        UDemonicUITheme::Get().ApplyControls(WidgetTree);
        UDemonicUITheme::Get().StyleText(Title, true, 28);
        UDemonicUITheme::Get().StyleButton(HostButton, true);
        return;
    }
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
    AddButton(Box, TEXT("Button_DevCoopBack"), FText::FromString(HostButton ? TEXT("뒤로가기 / 연결 취소") : TEXT("메뉴로 돌아가기")))->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleBack);
    UDemonicUITheme::Get().ApplyControls(WidgetTree);
    UDemonicUITheme::Get().StyleText(Title, true, 28);
    UDemonicUITheme::Get().StyleButton(HostButton ? HostButton.Get() : StartButton.Get(), true);
}

bool UDevelopmentCoopWidget::NativeOnHandleBackAction()
{
    HandleBack();
    return true;
}

UWidget* UDevelopmentCoopWidget::NativeGetDesiredFocusTarget() const
{
    return HostButton ? HostButton.Get() : ReadyButton.Get();
}

void UDevelopmentCoopWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (USteamDevelopmentSubsystem::IsRequested())
    {
        RefreshSteam();
        return;
    }
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
    if (USteamDevelopmentSubsystem::IsRequested())
    {
        USteamDevelopmentSubsystem* Steam = GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>();
        if (Steam->HasSession() || Steam->IsBusy() || !Cast<AMainMenuPlayerController>(GetOwningPlayer())) Steam->Leave();
        else DeactivateWidget();
        return;
    }
    UDevelopmentCoopSubsystem* Session = GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>();
    AMainMenuPlayerController* Menu = Cast<AMainMenuPlayerController>(GetOwningPlayer());
    if (Menu && !Session->IsPending()) DeactivateWidget();
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

void UDevelopmentCoopWidget::BuildSteamControls(UVerticalBox* Box)
{
    Capacity = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), TEXT("Combo_SteamProbeCapacity"));
    for (const FString& Value : { FString(TEXT("2")), FString(TEXT("3")), FString(TEXT("4")) }) Capacity->AddOption(Value);
    Capacity->SetSelectedOption(TEXT("2"));
    Box->AddChildToVerticalBox(Capacity);
    HostButton = AddButton(Box, TEXT("Button_SteamProbeHost"), FText::FromString(TEXT("친구 전용 연결 확인 방 만들기")));
    HostButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleSteamHost);
    SteamFindButton = AddButton(Box, TEXT("Button_SteamProbeFind"), FText::FromString(TEXT("Steam 친구의 연결 확인 방 검색")));
    SteamFindButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleSteamFind);
    SteamResults = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), TEXT("Combo_SteamProbeResults"));
    Box->AddChildToVerticalBox(SteamResults);
    JoinButton = AddButton(Box, TEXT("Button_SteamProbeJoin"), FText::FromString(TEXT("선택한 연결 확인 방 참가")));
    JoinButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleSteamJoin);
    SteamInviteButton = AddButton(Box, TEXT("Button_SteamProbeInvite"), FText::FromString(TEXT("Steam 친구 초대 창")));
    SteamInviteButton->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleSteamInvite);
    Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_SteamProbeStatus"));
    Status->SetAutoWrapText(true);
    Box->AddChildToVerticalBox(Status)->SetPadding(FMargin(0.f, 12.f));
    AddButton(Box, TEXT("Button_SteamProbeLeave"), FText::FromString(TEXT("나가기 / 뒤로가기")))->OnClicked.AddDynamic(this, &UDevelopmentCoopWidget::HandleBack);
}

void UDevelopmentCoopWidget::RefreshSteam()
{
    USteamDevelopmentSubsystem* Steam = GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>();
    if (!Steam || !SteamResults || !Status) return;
    const bool bMenu = Cast<AMainMenuPlayerController>(GetOwningPlayer()) != nullptr;
    const bool bCanStart = bMenu && Steam->IsReady() && !Steam->IsBusy() && !Steam->HasSession();
    HostButton->SetIsEnabled(bCanStart);
    SteamFindButton->SetIsEnabled(bCanStart);
    Capacity->SetIsEnabled(bCanStart);
    if (SteamResultLabels != Steam->GetResultLabels())
    {
        SteamResultLabels = Steam->GetResultLabels();
        SteamResults->ClearOptions();
        for (int32 Index = 0; Index < SteamResultLabels.Num(); ++Index) SteamResults->AddOption(FString::Printf(TEXT("%d. %s"), Index + 1, *SteamResultLabels[Index]));
        if (!SteamResultLabels.IsEmpty()) SteamResults->SetSelectedIndex(0);
    }
    JoinButton->SetIsEnabled(bCanStart && SteamResults->GetSelectedIndex() != INDEX_NONE);
    SteamInviteButton->SetIsEnabled(Steam->IsReady() && !Steam->IsBusy() && Steam->HasSession() && GetWorld()->GetNetMode() == NM_ListenServer);
    const ASteamDevelopmentPlayerController* ProbeController = Cast<ASteamDevelopmentPlayerController>(GetOwningPlayer());
    Status->SetText(ProbeController ? FText::FromString(ProbeController->GetProbeStatus().ToString() + TEXT("\n") + Steam->GetStatus().ToString()) : Steam->GetStatus());
}

void UDevelopmentCoopWidget::HandleSteamHost()
{
    GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>()->Host(GetOwningPlayer(), FCString::Atoi(*Capacity->GetSelectedOption()));
}

void UDevelopmentCoopWidget::HandleSteamFind()
{
    GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>()->Find(GetOwningPlayer());
}

void UDevelopmentCoopWidget::HandleSteamJoin()
{
    GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>()->Join(GetOwningPlayer(), SteamResults->GetSelectedIndex());
}

void UDevelopmentCoopWidget::HandleSteamInvite()
{
    GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>()->ShowInvites();
}
