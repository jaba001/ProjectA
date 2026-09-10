#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "OptionsWidget.generated.h"

class UComboBoxString;
class UCheckBox;

UCLASS()
class PROJECTA_API UOptionsWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void ApplyOptions();
    UFUNCTION()
    void CloseOptions();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnActivated() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> Quality;
    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> VSync;
};
