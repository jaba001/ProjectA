#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Game/Run/RunTypes.h"
#include "CharacterInventoryPanel.generated.h"

class USkillDefinitionDataAsset;
class UEquipmentDragDropOperation;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
struct FGameplayViewState;
struct FRunItemDefinition;

UCLASS()
class PROJECTA_API UCharacterInventoryPanel : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void RefreshInventory(const FGameplayViewState& View, FGuid CharacterId);

protected:
    virtual void NativeOnInitialized() override;
    virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;

private:
    UTextBlock* AddText(UVerticalBox* Parent, const FText& Text, int32 FontSize, float BottomPadding = 8.0f);
    void AddItem(const FRunItemDefinition& Item, int32 ItemIndex, int32 DisplayIndex);
    void AddSkill(const USkillDefinitionDataAsset* Skill);
    bool CanAcceptDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot) const;
    bool HandleDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot);

    UPROPERTY(Transient)
    FRunPartyMember DisplayedMember;

    bool bCanChangeEquipment = false;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> GoldText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ItemCountText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EmptyItemsText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SkillStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UUniformGridPanel> ItemGrid;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> SkillList;
};
