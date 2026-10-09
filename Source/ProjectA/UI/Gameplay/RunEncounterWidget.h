#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "RunEncounterWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UGameplayActionButton;
class UBorder;
class UImage;
class USizeBox;
class UCharacterEquipmentPanel;
class UCharacterInventoryPanel;
class UShopItemTooltipWidget;
class UScaleBox;
struct FGameplayViewState;

UCLASS()
class PROJECTA_API URunEncounterWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    void RefreshEncounter(const FGameplayViewState& View, bool bAllowRunCommands, bool bWorldPresentation = false);
    void FocusInventory();

protected:
    virtual void NativeOnInitialized() override;

private:
    void HandleSelection(FName EncounterId);
    void HandleLeave(FName ActionId);
    void HandlePurchase(FName OfferId);
    void HandleInventory(FName ActionId);
    bool bRunCommandsAllowed = false;
    FGuid BuyerCharacterId;
    int32 ShopRevision = INDEX_NONE;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> Backdrop;

    UPROPERTY(Transient)
    TObjectPtr<UScaleBox> ContentFit;

    UPROPERTY(Transient)
    TObjectPtr<UScaleBox> DungeonChoiceFit;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DungeonChoiceTitle;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DungeonChoiceMessage;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGameplayActionButton>> DungeonChoiceButtons;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Title;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Message;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ShopBalance;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ShopHint;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> EquipmentSize;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> InventorySize;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> NpcFocusGap;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> MerchantSize;

    UPROPERTY(Transient)
    TObjectPtr<UCharacterEquipmentPanel> EquipmentPanel;

    UPROPERTY(Transient)
    TObjectPtr<UCharacterInventoryPanel> InventoryPanel;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> ShopActions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGameplayActionButton>> ShopButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> ShopCards;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UShopItemTooltipWidget>> ShopTooltips;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UImage>> ShopIcons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> ShopNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> ShopPrices;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayActionButton> RecoveryButton;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayActionButton> RerollButton;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> Actions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGameplayActionButton>> ChoiceButtons;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayActionButton> LeaveButton;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayActionButton> InventoryButton;
};
