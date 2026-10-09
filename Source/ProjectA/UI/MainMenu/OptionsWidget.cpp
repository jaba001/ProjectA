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
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/MainMenu/MainMenuRootWidget.h"
#include "UI/Theme/DemonicUITheme.h"
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

FString GetOptionsLanguage()
{
#if WITH_EDITOR
    if (GIsEditor)
    {
        FString SavedLanguage = TEXT("ko");
        if (GConfig) GConfig->GetString(TEXT("Internationalization"), TEXT("Language"), SavedLanguage, GGameUserSettingsIni);
        return SavedLanguage;
    }
#endif
    return UKismetInternationalizationLibrary::GetCurrentLanguage();
}
}

UOptionsWidget::UOptionsWidget()
{
    bIsBackHandler = true;
    bIsModal = true;
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
    Label->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = FontSize;
    Label->SetFont(Font);
    UDemonicUITheme::Get().StyleText(Label, FontSize >= 22);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    return Label;
}

UButton* UOptionsWidget::CreateButton(const FName Name, const FText& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Text);
    Label->SetJustification(ETextJustify::Center);
    Label->SetAutoWrapText(true);
    Label->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
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
    UComboBoxString* Selector = WidgetTree->ConstructWidget<UDemonicComboBoxString>(UDemonicComboBoxString::StaticClass(), Name);
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
    UDemonicUITheme::Get().StyleBackdrop(Background);
    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(760.0f);
    UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    Fit->SetContent(Size);
    UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Fit);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    ContentSlot->SetPadding(FMargin(24.0f));
    SettingsPanel = WidgetTree->ConstructWidget<UBorder>();
    UDemonicUITheme::Get().StylePanel(SettingsPanel);
    SettingsPanel->SetPadding(FMargin(32.0f));
    Size->SetContent(SettingsPanel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    SettingsPanel->SetContent(Content);
    AddText(Content, NSLOCTEXT("Options", "Title", "설정"), 32, 8.0f);
    AddText(Content, NSLOCTEXT("Options", "LanguageSubtitle", "언어·화면·그래픽을 조정합니다. 변경한 값은 적용 후 저장됩니다."), 16, 24.0f);
    UDemonicUITheme::Get().AddDivider(WidgetTree, Content);
    Language = AddSelector(Content, TEXT("LanguageSelect"), NSLOCTEXT("Options", "Language", "언어"));
    Language->AddOption(TEXT("한국어"));
    Language->AddOption(TEXT("English"));
    AddText(Content, NSLOCTEXT("Options", "LanguageHint", "언어는 적용 및 저장 시 변경됩니다. 화면을 함께 바꾸면 유지 및 저장 후 적용됩니다."), 14, 16.0f);
    AddText(Content, NSLOCTEXT("Options", "DisplaySection", "화면"), 22, 6.0f);
    WindowMode = AddSelector(Content, TEXT("WindowModeSelect"), NSLOCTEXT("Options", "WindowMode", "화면 모드"));
    WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &UOptionsWidget::HandleWindowModeChanged);
    Resolution = AddSelector(Content, TEXT("ResolutionSelect"), NSLOCTEXT("Options", "Resolution", "해상도"));
    ResolutionHint = AddText(Content, FText::GetEmpty(), 14, 24.0f);
    AddText(Content, NSLOCTEXT("Options", "GraphicsSection", "그래픽"), 22, 6.0f);
    Quality = AddSelector(Content, TEXT("QualitySelect"), NSLOCTEXT("Options", "Quality", "그래픽 품질"));
    RefreshLocalizedSelectors();
    AddText(Content, NSLOCTEXT("Options", "QualityHint", "품질이 높을수록 성능 부담이 커집니다. 사용자 지정은 현재 세부 품질을 유지합니다."), 14, 12.0f);
    VSync = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("VSyncCheck"));
    UTextBlock* VSyncLabel = WidgetTree->ConstructWidget<UTextBlock>();
    VSyncLabel->SetText(NSLOCTEXT("Options", "VSync", "수직 동기화"));
    VSync->AddChild(VSyncLabel);
    VSync->SetToolTipText(NSLOCTEXT("Options", "VSyncHint", "화면 찢어짐을 줄이도록 화면 갱신에 맞춥니다. 환경에 따라 입력 반응이 느려질 수 있습니다."));
    Content->AddChildToVerticalBox(VSync)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));
    Status = AddText(Content, FText::GetEmpty(), 16, 16.0f);
    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Actions);
    ApplyButton = CreateButton(TEXT("ApplyOptionsButton"), NSLOCTEXT("Options", "Apply", "적용 및 저장"));
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
    ConfirmationPanel->SetHorizontalAlignment(HAlign_Center);
    ConfirmationPanel->SetVerticalAlignment(VAlign_Center);
    USizeBox* ConfirmationSize = WidgetTree->ConstructWidget<USizeBox>();
    ConfirmationSize->SetWidthOverride(620.0f);
    UScaleBox* ConfirmationFit = WidgetTree->ConstructWidget<UScaleBox>();
    ConfirmationFit->SetStretch(EStretch::ScaleToFit);
    ConfirmationFit->SetStretchDirection(EStretchDirection::DownOnly);
    ConfirmationFit->SetContent(ConfirmationSize);
    ConfirmationPanel->SetHorizontalAlignment(HAlign_Fill);
    ConfirmationPanel->SetVerticalAlignment(VAlign_Fill);
    ConfirmationPanel->SetContent(ConfirmationFit);
    UVerticalBox* ConfirmationContent = WidgetTree->ConstructWidget<UVerticalBox>();
    UBorder* ConfirmationFrame = WidgetTree->ConstructWidget<UBorder>();
    UDemonicUITheme::Get().StylePanel(ConfirmationFrame);
    ConfirmationFrame->SetPadding(FMargin(32.0f));
    ConfirmationSize->SetContent(ConfirmationFrame);
    ConfirmationFrame->SetContent(ConfirmationContent);
    AddText(ConfirmationContent, NSLOCTEXT("Options", "ConfirmTitle", "변경한 화면 설정을 유지할까요?"), 26, 20.0f);
    ConfirmationText = AddText(ConfirmationContent, FText::GetEmpty(), 18, 24.0f);
    UButton* Confirm = CreateButton(TEXT("ConfirmOptionsButton"), NSLOCTEXT("Options", "Keep", "유지 및 저장"));
    Confirm->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::ConfirmOptions);
    ConfirmationContent->AddChildToVerticalBox(Confirm)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
    RevertButton = CreateButton(TEXT("RevertOptionsButton"), NSLOCTEXT("Options", "Revert", "이전 설정으로 복구"));
    RevertButton->OnClicked.AddUniqueDynamic(this, &UOptionsWidget::RevertOptions);
    ConfirmationContent->AddChildToVerticalBox(RevertButton);
    UDemonicUITheme::Get().ApplyControls(WidgetTree);
    UDemonicUITheme::Get().StyleButton(ApplyButton, true);
    UDemonicUITheme::Get().StyleButton(Confirm, true);
    SetConfirmationVisible(false);
}

void UOptionsWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    StopObservingLocalization();
    LocalizationChangedHandle = FTextLocalizationManager::Get().OnTextRevisionChangedEvent.AddUObject(this, &UOptionsWidget::HandleLocalizationChanged);
    RefreshLocalizedSelectors();
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
    Language->SetSelectedIndex(GetOptionsLanguage().StartsWith(TEXT("en"), ESearchCase::IgnoreCase) ? 1 : 0);
    WindowMode->SetSelectedIndex(static_cast<int32>(Settings->GetFullscreenMode()));
    Quality->SetSelectedIndex(GetQualityPreset(Settings->ScalabilityQuality));
    VSync->SetIsChecked(Settings->IsVSyncEnabled());
    RefreshResolutions(Settings->GetScreenResolution());
    bRefreshing = false;
}

void UOptionsWidget::RefreshLocalizedSelectors()
{
    if (!WindowMode || !Quality) return;
    // Combo boxes store strings, so rebuild their labels only after localized text resources have changed.
    // 콤보박스는 문자열을 저장하므로 번역 리소스가 갱신된 뒤 표시 문구를 다시 구성합니다.
    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    const int32 WindowModeIndex = WindowMode->GetSelectedIndex();
    const int32 QualityIndex = Quality->GetSelectedIndex();
    WindowMode->ClearOptions();
    for (const FText& Label : { NSLOCTEXT("Options", "ModeFullscreen", "전체화면"), NSLOCTEXT("Options", "ModeBorderless", "테두리 없는 전체화면"), NSLOCTEXT("Options", "ModeWindowed", "창 모드") }) WindowMode->AddOption(Label.ToString());
    WindowMode->SetSelectedIndex(WindowModeIndex);
    Quality->ClearOptions();
    for (const FText& Label : { NSLOCTEXT("Options", "QualityLow", "낮음"), NSLOCTEXT("Options", "QualityMedium", "중간"), NSLOCTEXT("Options", "QualityHigh", "높음"), NSLOCTEXT("Options", "QualityEpic", "최고"), NSLOCTEXT("Options", "QualityCinematic", "시네마틱"), NSLOCTEXT("Options", "QualityCustom", "사용자 지정 (현재 값 유지)") }) Quality->AddOption(Label.ToString());
    Quality->SetSelectedIndex(QualityIndex);
}

void UOptionsWidget::HandleLocalizationChanged()
{
    RefreshLocalizedSelectors();
    if (bAwaitingConfirmation) UpdateConfirmationText();
}

void UOptionsWidget::StopObservingLocalization()
{
    FTextLocalizationManager::Get().OnTextRevisionChangedEvent.Remove(LocalizationChangedHandle);
    LocalizationChangedHandle.Reset();
}

bool UOptionsWidget::ApplyPendingLanguage()
{
    const FString LanguageToApply = MoveTemp(PendingLanguage);
    PendingLanguage.Reset();
    if (LanguageToApply != TEXT("ko") && LanguageToApply != TEXT("en")) return false;
#if WITH_EDITOR
    if (GIsEditor)
    {
        FTextLocalizationManager& Localization = FTextLocalizationManager::Get();
        if (!GConfig || !FInternationalization::Get().GetCulture(LanguageToApply).IsValid() || Localization.GetNativeCultureName(ELocalizedTextSourceCategory::Game).IsEmpty()) return false;
        // Preview game resources without changing the editor language or its localization preference.
        // 에디터 언어와 번역 설정을 바꾸지 않고 게임 리소스에만 선택한 언어를 적용합니다.
        Localization.EnableGameLocalizationPreview(LanguageToApply);
        GConfig->SetString(TEXT("Internationalization"), TEXT("Language"), *LanguageToApply, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        return true;
    }
#endif
    return UKismetInternationalizationLibrary::SetCurrentLanguage(LanguageToApply, true);
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
    const int32 LanguageIndex = Language->GetSelectedIndex();
    if (!IsValidResolution(SelectedResolution) || ModeIndex < 0 || ModeIndex > 2 || QualityIndex < 0 || QualityIndex > CustomQualityIndex || LanguageIndex < 0 || LanguageIndex > 1) return;
    PendingLanguage = LanguageIndex == 1 ? TEXT("en") : TEXT("ko");
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
        const bool bLanguageApplied = ApplyPendingLanguage();
        RefreshOptions();
        Status->SetText(bLanguageApplied ? NSLOCTEXT("Options", "Saved", "설정을 적용하고 저장했습니다.") : NSLOCTEXT("Options", "LanguageFailed", "화면·그래픽 설정은 저장했지만 언어를 적용하지 못했습니다. 언어 리소스를 확인한 뒤 다시 시도해 주세요."));
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
    bool bLanguageApplied = false;
    if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
    {
        Settings->ApplyResolutionSettings(false);
        Settings->ConfirmVideoMode();
        Settings->SaveSettings();
        bLanguageApplied = ApplyPendingLanguage();
    }
    PendingLanguage.Reset();
    bAwaitingConfirmation = false;
    StopConfirmationTicker();
    SetFullscreenShortcutsBlocked(false);
    SetConfirmationVisible(false);
    RefreshOptions();
    Status->SetText(bLanguageApplied ? NSLOCTEXT("Options", "Saved", "설정을 적용하고 저장했습니다.") : NSLOCTEXT("Options", "LanguageFailed", "화면·그래픽 설정은 저장했지만 언어를 적용하지 못했습니다. 언어 리소스를 확인한 뒤 다시 시도해 주세요."));
    ApplyButton->SetFocus();
}

void UOptionsWidget::RevertOptions()
{
    if (!bAwaitingConfirmation) return;
    bAwaitingConfirmation = false;
    PendingLanguage.Reset();
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
    Status->SetText(NSLOCTEXT("Options", "RestoredWithLanguage", "변경을 취소했습니다. 화면·그래픽 설정을 복구하고 기존 언어를 유지합니다."));
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
    RequestBack();
    return true;
}

void UOptionsWidget::RequestBack()
{
    if (bAwaitingConfirmation)
    {
        RevertOptions();
    }
    else
    {
        CloseOptions();
    }
}

UWidget* UOptionsWidget::NativeGetDesiredFocusTarget() const
{
    return bAwaitingConfirmation ? static_cast<UWidget*>(RevertButton) : static_cast<UWidget*>(WindowMode);
}

void UOptionsWidget::NativeOnDeactivated()
{
    StopObservingLocalization();
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
    StopObservingLocalization();
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
