#include "UI/MainMenu/OptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controller/MainMenuPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr int32 CustomQualityIndex = 5;
constexpr double ConfirmationSeconds = 15.0;

bool IsValidResolution(FIntPoint Value)
{
    return Value.X > 0 && Value.Y > 0;
}

int32 GetQualityPreset(const Scalability::FQualityLevels& Quality)
{
    const int32 Level = Quality.GetSingleQualityLevel();
    if (Level < 0 || Level >= CustomQualityIndex) return CustomQualityIndex;
    Scalability::FQualityLevels Preset;
    Preset.SetFromSingleQualityLevel(Level);
    return Quality == Preset ? Level : CustomQualityIndex;
}
}

UOptionsWidget::UOptionsWidget()
{
    bIsBackHandler = true;
}

TOptional<FUIInputConfig> UOptionsWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UTextBlock* UOptionsWidget::AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetAutoWrapText(true);
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = FontSize;
    Label->SetFont(Font);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    return Label;
}

UButton* UOptionsWidget::CreateButton(const FName Name, const FText& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetBackgroundColor(FLinearColor(0.16f, 0.2f, 0.27f));
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetJustification(ETextJustify::Center);
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = 18;
    Label->SetFont(Font);
    CastChecked<UButtonSlot>(Button->AddChild(Label))->SetPadding(FMargin(20.0f, 12.0f));
    return Button;
}

UComboBoxString* UOptionsWidget::AddSelector(UVerticalBox* Parent, const FName Name, const FText& Label)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Parent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 6.0f));
    USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
    LabelSize->SetWidthOverride(180.0f);
    UTextBlock* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>();
    LabelWidget->SetText(Label);
    FSlateFontInfo Font = LabelWidget->GetFont();
    Font.Size = 18;
    LabelWidget->SetFont(Font);
    LabelSize->SetContent(LabelWidget);
    Row->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);
    UComboBoxString* Selector = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), Name);
    Selector->SetContentPadding(FMargin(14.0f, 10.0f));
    Selector->SetMaxListHeight(260.0f);
    Row->AddChildToHorizontalBox(Selector)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    return Selector;
}

void UOptionsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.015f, 0.022f, 0.035f, 1.0f));
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(Scale);
    ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
    ScaleSlot->SetVerticalAlignment(VAlign_Fill);
    ScaleSlot->SetPadding(FMargin(24.0f));
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(760.0f);
    Scale->SetContent(Size);
    SettingsPanel = WidgetTree->ConstructWidget<UBorder>();
    SettingsPanel->SetBrushColor(FLinearColor(0.035f, 0.05f, 0.075f, 1.0f));
    SettingsPanel->SetPadding(FMargin(32.0f));
    Size->SetContent(SettingsPanel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    SettingsPanel->SetContent(Content);
    AddText(Content, NSLOCTEXT("Options", "Title", "설정"), 32, 8.0f);
    AddText(Content, NSLOCTEXT("Options", "Subtitle", "화면과 그래픽을 조정합니다. 변경한 값은 적용 후 저장됩니다."), 16, 24.0f);
    AddText(Content, NSLOCTEXT("Options", "DisplaySection", "화면"), 22, 6.0f);
    WindowMode = AddSelector(Content, TEXT("WindowModeSelect"), NSLOCTEXT("Options", "WindowMode", "화면 모드"));
    for (const TCHAR* Label : { TEXT("전체화면"), TEXT("테두리 없는 전체화면"), TEXT("창 모드") })
    {
        WindowMode->AddOption(Label);
    }
    WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &UOptionsWidget::HandleWindowModeChanged);
    Resolution = AddSelector(Content, TEXT("ResolutionSelect"), NSLOCTEXT("Options", "Resolution", "해상도"));
    ResolutionHint = AddText(Content, FText::GetEmpty(), 14, 24.0f);
    AddText(Content, NSLOCTEXT("Options", "GraphicsSection", "그래픽"), 22, 6.0f);
    Quality = AddSelector(Content, TEXT("QualitySelect"), NSLOCTEXT("Options", "Quality", "그래픽 품질"));
    for (const TCHAR* Label : { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("최고"), TEXT("시네마틱"), TEXT("사용자 지정 (현재 값 유지)") })
    {
        Quality->AddOption(Label);
    }
    AddText(Content, NSLOCTEXT("Options", "QualityHint", "품질이 높을수록 성능 부담이 커집니다. 사용자 지정은 현재 세부 품질을 유지합니다."), 14, 12.0f);
    VSync = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("VSyncCheck"));
    UTextBlock* VSyncLabel = WidgetTree->ConstructWidget<UTextBlock>();
    VSyncLabel->SetText(NSLOCTEXT("Options", "VSync", "수직 동기화"));
    VSync->AddChild(VSyncLabel);
    Content->AddChildToVerticalBox(VSync)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));
    Status = AddText(Content, FText::GetEmpty(), 16, 16.0f);
    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Actions);
    ApplyButton = CreateButton(TEXT("ApplyOptionsButton"), NSLOCTEXT("Options", "Apply", "적용 및 저장"));
    ApplyButton->SetBackgroundColor(FLinearColor(0.12f, 0.4f, 0.65f));
    ApplyButton->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::ApplyOptions);
    UHorizontalBoxSlot* ApplySlot = Actions->AddChildToHorizontalBox(ApplyButton);
    ApplySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ApplySlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
    UButton* Close = CreateButton(TEXT("CloseOptionsButton"), NSLOCTEXT("Options", "Close", "뒤로 / 적용 전 취소"));
    Close->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::CloseOptions);
    Actions->AddChildToHorizontalBox(Close)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    ConfirmationPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VideoConfirmationPanel"));
    ConfirmationPanel->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.025f, 0.96f));
    ConfirmationPanel->SetPadding(FMargin(24.0f));
    UOverlaySlot* ConfirmationSlot = Root->AddChildToOverlay(ConfirmationPanel);
    ConfirmationSlot->SetHorizontalAlignment(HAlign_Fill);
    ConfirmationSlot->SetVerticalAlignment(VAlign_Fill);
    UScaleBox* ConfirmationScale = WidgetTree->ConstructWidget<UScaleBox>();
    ConfirmationScale->SetStretch(EStretch::ScaleToFit);
    ConfirmationScale->SetStretchDirection(EStretchDirection::DownOnly);
    ConfirmationPanel->SetContent(ConfirmationScale);
    USizeBox* ConfirmationSize = WidgetTree->ConstructWidget<USizeBox>();
    ConfirmationSize->SetWidthOverride(620.0f);
    ConfirmationScale->SetContent(ConfirmationSize);
    UVerticalBox* ConfirmationContent = WidgetTree->ConstructWidget<UVerticalBox>();
    ConfirmationSize->SetContent(ConfirmationContent);
    AddText(ConfirmationContent, NSLOCTEXT("Options", "ConfirmTitle", "변경한 화면 설정을 유지할까요?"), 26, 20.0f);
    ConfirmationText = AddText(ConfirmationContent, FText::GetEmpty(), 18, 24.0f);
    UButton* Confirm = CreateButton(TEXT("ConfirmOptionsButton"), NSLOCTEXT("Options", "Keep", "유지 및 저장"));
    Confirm->SetBackgroundColor(FLinearColor(0.12f, 0.4f, 0.65f));
    Confirm->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::ConfirmOptions);
    ConfirmationContent->AddChildToVerticalBox(Confirm)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    RevertButton = CreateButton(TEXT("RevertOptionsButton"), NSLOCTEXT("Options", "Revert", "이전 설정으로 복구"));
    RevertButton->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::RevertOptions);
    ConfirmationContent->AddChildToVerticalBox(RevertButton);
    SetConfirmationVisible(false);
}

void UOptionsWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    Status->SetText(NSLOCTEXT("Options", "Ready", "화면 모드나 해상도를 바꾸면 15초 동안 확인 후 저장합니다."));
    RefreshOptions();
    if (GetWorld() && GetWorld()->GetGameViewport())
    {
        ObservedViewport = GetWorld()->GetGameViewport();
        ObservedViewport->OnCloseRequested().RemoveAll(this);
        ObservedViewport->OnCloseRequested().AddUObject(this, &UOptionsWidget::HandleViewportClosed);
    }
}

void UOptionsWidget::RefreshOptions()
{
    UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
    ApplyButton->SetIsEnabled(Settings != nullptr);
    if (!Settings)
    {
        Status->SetText(NSLOCTEXT("Options", "Unavailable", "설정을 불러올 수 없습니다. 메뉴로 돌아간 뒤 다시 시도해 주세요."));
        return;
    }
    bRefreshing = true;
    WindowMode->SetSelectedIndex(static_cast<int32>(Settings->GetFullscreenMode()));
    Quality->SetSelectedIndex(GetQualityPreset(Settings->ScalabilityQuality));
    VSync->SetIsChecked(Settings->IsVSyncEnabled());
    RefreshResolutions(Settings->GetScreenResolution());
    bRefreshing = false;
}

void UOptionsWidget::RefreshResolutions(FIntPoint PreferredResolution)
{
    UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
    if (!Settings) return;
    AvailableResolutions.Reset();
    Resolution->ClearOptions();
    const EWindowMode::Type Mode = static_cast<EWindowMode::Type>(WindowMode->GetSelectedIndex());
    const FIntPoint Desktop = GetDesktopResolution();
    const FIntPoint Current = Settings->GetScreenResolution();
    const bool bBorderless = Mode == EWindowMode::WindowedFullscreen;
    if (bBorderless)
    {
        PreferredResolution = IsValidResolution(Desktop) ? Desktop : Current;
        AvailableResolutions.Add(PreferredResolution);
    }
    else
    {
        if (Mode == EWindowMode::Fullscreen)
        {
            UKismetSystemLibrary::GetSupportedFullscreenResolutions(AvailableResolutions);
        }
        else
        {
            UKismetSystemLibrary::GetConvenientWindowedResolutions(AvailableResolutions);
        }
        // Keep a valid current size when enumeration omits a manually resized window.
        // 수동으로 조정한 창 크기가 열거되지 않아도 유효한 현재 크기를 유지합니다.
        if (Mode == Settings->GetFullscreenMode() && IsValidResolution(Current)) AvailableResolutions.AddUnique(Current);
        if (AvailableResolutions.IsEmpty()) AvailableResolutions.Add(IsValidResolution(Current) ? Current : Desktop);
    }
    AvailableResolutions.RemoveAll([](FIntPoint Value) { return !IsValidResolution(Value); });
    AvailableResolutions.Sort([](FIntPoint Left, FIntPoint Right) { return Left.X == Right.X ? Left.Y < Right.Y : Left.X < Right.X; });
    for (int32 Index = AvailableResolutions.Num() - 1; Index > 0; --Index)
    {
        if (AvailableResolutions[Index] == AvailableResolutions[Index - 1]) AvailableResolutions.RemoveAt(Index);
    }
    for (FIntPoint Value : AvailableResolutions) Resolution->AddOption(FString::Printf(TEXT("%d × %d"), Value.X, Value.Y));
    int32 SelectedIndex = AvailableResolutions.IndexOfByKey(PreferredResolution);
    if (SelectedIndex == INDEX_NONE) SelectedIndex = AvailableResolutions.IndexOfByKey(Desktop);
    if (SelectedIndex == INDEX_NONE && !AvailableResolutions.IsEmpty()) SelectedIndex = AvailableResolutions.Num() - 1;
    Resolution->SetSelectedIndex(SelectedIndex);
    Resolution->SetIsEnabled(!bBorderless && !AvailableResolutions.IsEmpty());
    ResolutionHint->SetText(bBorderless ? NSLOCTEXT("Options", "BorderlessHint", "테두리 없는 전체화면은 데스크톱 해상도를 사용합니다.") : NSLOCTEXT("Options", "ResolutionHint", "모니터가 지원하는 해상도 또는 창 크기를 선택합니다."));
    ApplyButton->SetIsEnabled(!AvailableResolutions.IsEmpty());
}

FIntPoint UOptionsWidget::GetSelectedResolution() const
{
    const int32 Index = Resolution->GetSelectedIndex();
    return AvailableResolutions.IsValidIndex(Index) ? AvailableResolutions[Index] : FIntPoint::ZeroValue;
}

FIntPoint UOptionsWidget::GetDesktopResolution() const
{
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (Viewport && Viewport->GetWindow() && FSlateApplication::IsInitialized())
    {
        const FSlateRect WorkArea = FSlateApplication::Get().GetWorkArea(Viewport->GetWindow()->GetRectInScreen());
        FDisplayMetrics Metrics;
        FSlateApplication::Get().GetCachedDisplayMetrics(Metrics);
        for (const FMonitorInfo& Monitor : Metrics.MonitorInfo)
        {
            if (WorkArea.GetTopLeft() == FVector2D(Monitor.WorkArea.Left, Monitor.WorkArea.Top))
            {
                return FIntPoint(Monitor.DisplayRect.Right - Monitor.DisplayRect.Left, Monitor.DisplayRect.Bottom - Monitor.DisplayRect.Top);
            }
        }
    }
    const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
    return Settings ? Settings->GetDesktopResolution() : FIntPoint::ZeroValue;
}

void UOptionsWidget::HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!bRefreshing && !bAwaitingConfirmation) RefreshResolutions(GetSelectedResolution());
}

void UOptionsWidget::ApplyOptions()
{
    if (bAwaitingConfirmation) return;
    UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
    if (!Settings) return;
    const FIntPoint SelectedResolution = GetSelectedResolution();
    const int32 ModeIndex = WindowMode->GetSelectedIndex();
    const int32 QualityIndex = Quality->GetSelectedIndex();
    if (!IsValidResolution(SelectedResolution) || ModeIndex < 0 || ModeIndex > 2 || QualityIndex < 0 || QualityIndex > CustomQualityIndex) return;
    const EWindowMode::Type SelectedMode = static_cast<EWindowMode::Type>(ModeIndex);
    PreviousResolution = Settings->GetScreenResolution();
    PreviousWindowMode = Settings->GetFullscreenMode();
    PreviousQuality = Settings->ScalabilityQuality;
    bPreviousVSync = Settings->IsVSyncEnabled();
    const bool bVideoChanged = SelectedMode != PreviousWindowMode || (SelectedMode != EWindowMode::WindowedFullscreen && SelectedResolution != PreviousResolution);
    if (bVideoChanged)
    {
        Settings->SetFullscreenMode(SelectedMode);
        Settings->SetScreenResolution(SelectedResolution);
    }
    // Video setters adjust resolution scale; preserve every custom quality value unless a new preset was selected.
    // 화면 설정 함수가 렌더링 비율을 조정하므로 새 프리셋을 선택하지 않으면 모든 기존 품질 값을 유지합니다.
    Settings->ScalabilityQuality = PreviousQuality;
    if (QualityIndex < CustomQualityIndex && QualityIndex != GetQualityPreset(PreviousQuality)) Settings->SetOverallScalabilityLevel(QualityIndex);
    Settings->SetVSyncEnabled(VSync->IsChecked());
    if (!bVideoChanged)
    {
        Settings->ApplyNonResolutionSettings();
        Settings->SaveSettings();
        RefreshOptions();
        Status->SetText(NSLOCTEXT("Options", "Saved", "설정을 적용하고 저장했습니다."));
        return;
    }
    // ApplySettings saves immediately; preview without writing the unconfirmed video mode to disk.
    // ApplySettings는 즉시 저장하므로 확인 전 화면 모드를 파일에 기록하지 않고 시험 적용합니다.
    bAwaitingConfirmation = true;
    SetFullscreenShortcutsBlocked(true);
    // A direct request also leaves the preferred Alt+Enter fullscreen mode unchanged until confirmation.
    // 직접 요청하여 확인 전까지 Alt+Enter의 기본 전체화면 모드도 변경하지 않습니다.
    UGameUserSettings::RequestResolutionChange(SelectedResolution.X, SelectedResolution.Y, SelectedMode, false);
    Settings->ApplyNonResolutionSettings();
    ConfirmationDeadline = FPlatformTime::Seconds() + ConfirmationSeconds;
    ConfirmationTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UOptionsWidget::TickConfirmation), 0.1f);
    UpdateConfirmationText();
    SetConfirmationVisible(true);
    RevertButton->SetFocus();
}

void UOptionsWidget::ConfirmOptions()
{
    if (!bAwaitingConfirmation) return;
    if (FPlatformTime::Seconds() >= ConfirmationDeadline)
    {
        RevertOptions();
        return;
    }
    if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
    {
        Settings->ApplyResolutionSettings(false);
        Settings->ConfirmVideoMode();
        Settings->SaveSettings();
    }
    bAwaitingConfirmation = false;
    StopConfirmationTicker();
    SetFullscreenShortcutsBlocked(false);
    SetConfirmationVisible(false);
    RefreshOptions();
    Status->SetText(NSLOCTEXT("Options", "Saved", "설정을 적용하고 저장했습니다."));
    ApplyButton->SetFocus();
}

void UOptionsWidget::RevertOptions()
{
    if (!bAwaitingConfirmation) return;
    bAwaitingConfirmation = false;
    StopConfirmationTicker();
    SetFullscreenShortcutsBlocked(false);
    if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
    {
        // Window resizing can update the engine's last-confirmed mode during preview; use our own snapshot.
        // 시험 적용 중 창 크기 변경이 엔진의 마지막 확인 값을 갱신할 수 있으므로 별도 스냅샷으로 복구합니다.
        Settings->SetFullscreenMode(PreviousWindowMode);
        Settings->SetScreenResolution(PreviousResolution);
        Settings->ScalabilityQuality = PreviousQuality;
        Settings->SetVSyncEnabled(bPreviousVSync);
        Settings->ApplyResolutionSettings(false);
        Settings->ApplyNonResolutionSettings();
        Settings->ConfirmVideoMode();
        Settings->SaveSettings();
    }
    SetConfirmationVisible(false);
    RefreshOptions();
    Status->SetText(NSLOCTEXT("Options", "Restored", "변경을 취소하고 이전 화면·그래픽 설정으로 복구했습니다."));
    if (IsActivated()) ApplyButton->SetFocus();
}

void UOptionsWidget::SetConfirmationVisible(bool bVisible)
{
    SettingsPanel->SetIsEnabled(!bVisible);
    ConfirmationPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UOptionsWidget::StopConfirmationTicker()
{
    FTSTicker::GetCoreTicker().RemoveTicker(ConfirmationTicker);
    ConfirmationTicker.Reset();
}

void UOptionsWidget::SetFullscreenShortcutsBlocked(bool bBlocked)
{
    if (bFullscreenShortcutsBlocked == bBlocked) return;
    UInputSettings* Input = GetMutableDefault<UInputSettings>();
    if (bBlocked)
    {
        bPreviousAltEnter = Input->bAltEnterTogglesFullscreen;
        bPreviousF11 = Input->bF11TogglesFullscreen;
    }
    // Viewport fullscreen shortcuts save settings before CommonUI receives input; suspend them only during preview.
    // 뷰포트 전체화면 단축키는 CommonUI 입력 전에 설정을 저장하므로 시험 적용 중에만 중지합니다.
    Input->bAltEnterTogglesFullscreen = bBlocked ? false : bPreviousAltEnter;
    Input->bF11TogglesFullscreen = bBlocked ? false : bPreviousF11;
    bFullscreenShortcutsBlocked = bBlocked;
}

bool UOptionsWidget::TickConfirmation(float DeltaTime)
{
    if (!bAwaitingConfirmation) return false;
    if (FPlatformTime::Seconds() >= ConfirmationDeadline)
    {
        RevertOptions();
        return false;
    }
    UpdateConfirmationText();
    return true;
}

void UOptionsWidget::UpdateConfirmationText()
{
    const int32 SecondsLeft = FMath::Max(0, FMath::CeilToInt(ConfirmationDeadline - FPlatformTime::Seconds()));
    ConfirmationText->SetText(FText::Format(NSLOCTEXT("Options", "Countdown", "{0}초 안에 확인하지 않으면 이전 설정으로 복구합니다.\n화면이 정상적으로 보일 때만 유지 및 저장을 선택하세요."), FText::AsNumber(SecondsLeft)));
}

bool UOptionsWidget::NativeOnHandleBackAction()
{
    if (bAwaitingConfirmation)
    {
        RevertOptions();
    }
    else
    {
        CloseOptions();
    }
    return true;
}

UWidget* UOptionsWidget::NativeGetDesiredFocusTarget() const
{
    return bAwaitingConfirmation ? static_cast<UWidget*>(RevertButton) : static_cast<UWidget*>(WindowMode);
}

void UOptionsWidget::NativeOnDeactivated()
{
    RevertOptions();
    StopConfirmationTicker();
    if (ObservedViewport.IsValid()) ObservedViewport->OnCloseRequested().RemoveAll(this);
    ObservedViewport.Reset();
    if (AMainMenuPlayerController* Controller = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
    {
        if (UMainMenuRootWidget* Root = Controller->GetMainMenuRootWidget()) Root->SetMainStackHiddenByMenu(false);
    }
    Super::NativeOnDeactivated();
}

void UOptionsWidget::NativeDestruct()
{
    RevertOptions();
    StopConfirmationTicker();
    if (ObservedViewport.IsValid()) ObservedViewport->OnCloseRequested().RemoveAll(this);
    ObservedViewport.Reset();
    Super::NativeDestruct();
}

void UOptionsWidget::HandleViewportClosed(FViewport* Viewport)
{
    RevertOptions();
}

void UOptionsWidget::CloseOptions()
{
    RevertOptions();
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
