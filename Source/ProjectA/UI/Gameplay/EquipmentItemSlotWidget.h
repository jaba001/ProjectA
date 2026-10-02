#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Game/Run/RunItemShopTypes.h"
#include "EquipmentItemSlotWidget.generated.h"

class UBorder;
class UEquipmentDragDropOperation;
class UImage;
class USizeBox;
class UTextBlock;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FEquipmentSlotDropDelegate, const UEquipmentDragDropOperation*, FGameplayTag);
DECLARE_DELEGATE_OneParam(FEquipmentItemSelectedDelegate, int32);

UCLASS()
class PROJECTA_API UEquipmentItemSlotWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void UseListPresentation();
    void SetSelected(bool bSelected);
    void RefreshSlot(FGuid CharacterId, int32 Revision, int32 ItemIndex, const FRunItemDefinition* Item, FGameplayTag TargetSlot, FName EmptyIcon, const FText& SlotLabel, bool bCanDrag);
    FEquipmentSlotDropDelegate CanAcceptDrop;
    FEquipmentSlotDropDelegate ReceiveDrop;
    FEquipmentItemSelectedDelegate ItemSelected;

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    virtual void NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& MouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
    virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;

private:
    void ResetDropHighlight();
    void UpdateListHighlight();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> Card;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> Frame;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> IconSize;

    UPROPERTY(Transient)
    TObjectPtr<UImage> Icon;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SlotText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ItemText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StateText;

    UPROPERTY(Transient)
    FRunItemDefinition DisplayedItem;

    FGuid CharacterId;
    FGameplayTag TargetSlot;
    int32 ItemIndex = INDEX_NONE;
    int32 Revision = INDEX_NONE;
    bool bCanDrag = false;
    bool bListPresentation = false;
    bool bSelected = false;
    bool bListHovered = false;
    bool bDropHighlighted = false;
    bool bDropAccepted = false;
};
