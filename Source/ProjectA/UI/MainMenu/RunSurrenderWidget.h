#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "RunSurrenderWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

// Confirms surrender against the saved Run identified when the modal was opened.
// 모달을 열 때 확인한 저장 여정에 대해서만 항복을 확정합니다.
UCLASS()
class PROJECTA_API URunSurrenderWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    URunSurrenderWidget();
    void ConfigureConfirmation(const FString& Token);
    FSimpleMulticastDelegate OnSurrendered;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
    UButton* AddButton(UVerticalBox* Parent, FName Name, const FText& Text);
    void ResetConfirmation();

    UFUNCTION()
    void HandleConfirm();
    UFUNCTION()
    void HandleCancel();

    UPROPERTY(Transient)
    TObjectPtr<UButton> ConfirmButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> CancelButton;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ErrorText;

    FString ConfirmationToken;
    bool bSubmitting = false;
};
