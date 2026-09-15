#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Containers/Ticker.h"
#include "Scalability.h"
#include "OptionsWidget.generated.h"

class UComboBoxString;
class UCheckBox;
class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UGameViewportClient;
class FViewport;

UCLASS()
class PROJECTA_API UOptionsWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UOptionsWidget();
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    UFUNCTION()
    void ApplyOptions();
    UFUNCTION()
    void CloseOptions();
    UFUNCTION()
    void ConfirmOptions();
    UFUNCTION()
    void RevertOptions();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UTextBlock* AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding);
    UButton* CreateButton(const FName Name, const FText& Text);
    UComboBoxString* AddSelector(UVerticalBox* Parent, const FName Name, const FText& Label);
    void RefreshOptions();
    void RefreshResolutions(FIntPoint PreferredResolution);
    FIntPoint GetSelectedResolution() const;
    FIntPoint GetDesktopResolution() const;
    void HandleViewportClosed(FViewport* Viewport);
    void SetConfirmationVisible(bool bVisible);
    void StopConfirmationTicker();
    void SetFullscreenShortcutsBlocked(bool bBlocked);
    bool TickConfirmation(float DeltaTime);
    void UpdateConfirmationText();

    UFUNCTION()
    void HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> Resolution;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> WindowMode;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> Quality;
    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> VSync;
    UPROPERTY(Transient)
    TObjectPtr<UBorder> SettingsPanel;
    UPROPERTY(Transient)
    TObjectPtr<UBorder> ConfirmationPanel;
    UPROPERTY(Transient)
    TObjectPtr<UButton> ApplyButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> RevertButton;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResolutionHint;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ConfirmationText;

    TArray<FIntPoint> AvailableResolutions;
    FIntPoint PreviousResolution = FIntPoint::ZeroValue;
    EWindowMode::Type PreviousWindowMode = EWindowMode::Windowed;
    Scalability::FQualityLevels PreviousQuality;
    bool bPreviousVSync = false;
    bool bRefreshing = false;
    bool bAwaitingConfirmation = false;
    bool bFullscreenShortcutsBlocked = false;
    bool bPreviousAltEnter = false;
    bool bPreviousF11 = false;
    double ConfirmationDeadline = 0.0;
    FTSTicker::FDelegateHandle ConfirmationTicker;
    TWeakObjectPtr<UGameViewportClient> ObservedViewport;
};
