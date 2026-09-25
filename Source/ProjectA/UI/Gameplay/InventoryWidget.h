#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventoryWidget.generated.h"

class UButton;
class UCharacterEquipmentPanel;
class UCharacterInventoryPanel;
struct FGameplayViewState;

UCLASS()
class PROJECTA_API UInventoryWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UInventoryWidget();
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    void RefreshInventory(const FGameplayViewState& View, FGuid CharacterId);

protected:
    virtual void NativeOnInitialized() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
    UFUNCTION()
    void HandleClose();

    UPROPERTY(Transient)
    TObjectPtr<UCharacterEquipmentPanel> EquipmentPanel;

    UPROPERTY(Transient)
    TObjectPtr<UCharacterInventoryPanel> InventoryPanel;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CloseButton;
};
