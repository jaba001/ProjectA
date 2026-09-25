#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Game/Run/RunTypes.h"
#include "CharacterEquipmentPanel.generated.h"

class UTextBlock;
class UEquipmentDragDropOperation;
class UEquipmentItemSlotWidget;
class UUniformGridPanel;
class UVerticalBox;
struct FGameplayViewState;

UCLASS()
class PROJECTA_API UCharacterEquipmentPanel : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void RefreshEquipment(const FGameplayViewState& View, FGuid CharacterId);

protected:
    virtual void NativeOnInitialized() override;

private:
    UTextBlock* AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding = 8.0f);
    void AddEquipmentSlot(FGameplayTag SlotTag, FName SlotId, const FText& Label, int32 Row, int32 Column);
    bool CanAcceptDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot) const;
    bool HandleDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot);

    UPROPERTY(Transient)
    FRunPartyMember DisplayedMember;

    bool bCanChangeEquipment = false;
    TArray<FGameplayTag> SlotTags;
    TArray<FName> SlotIcons;
    TArray<FText> SlotLabels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UEquipmentItemSlotWidget>> SlotWidgets;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CharacterText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HintText;

    UPROPERTY(Transient)
    TObjectPtr<UUniformGridPanel> EquipmentSlots;
};
