#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "DevelopmentCoopWidget.generated.h"

class ADevelopmentCoopLobby;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class PROJECTA_API UDevelopmentCoopWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UDevelopmentCoopWidget();
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    void RefreshLobby(ADevelopmentCoopLobby* Lobby);

protected:
    virtual void NativeOnInitialized() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    UButton* AddButton(UVerticalBox* Box, const FName Name, const FText& Text);
    UFUNCTION()
    void HandleHost();
    UFUNCTION()
    void HandleJoin();
    UFUNCTION()
    void HandleBack();
    UFUNCTION()
    void HandleReady();
    UFUNCTION()
    void HandleStart();
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> Capacity;
    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> Address;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Roster;
    UPROPERTY(Transient)
    TObjectPtr<UButton> HostButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> JoinButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> ReadyButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> StartButton;
    bool bLocalReady = false;
    bool bShowLocalError = false;
};
