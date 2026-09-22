#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventoryWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class USkillDefinitionDataAsset;
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
    UTextBlock* AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding = 8.0f);
    void AddSkill(const USkillDefinitionDataAsset* Skill);

    UFUNCTION()
    void HandleClose();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CharacterText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> GoldText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> SkillList;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CloseButton;
};
