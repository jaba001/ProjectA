#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Components/Button.h"
#include "Game/Run/RunTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"
#include "CharacterInventoryPanel.generated.h"

class USkillDefinitionDataAsset;
class UEquipmentDragDropOperation;
class UEquipmentItemSlotWidget;
class UBorder;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
struct FGameplayViewState;
struct FRunItemDefinition;

DECLARE_DELEGATE_OneParam(FInventoryCategorySelectedDelegate, int32);

UCLASS()
class PROJECTA_API UInventoryCategoryButton : public UButton
{
    GENERATED_BODY()

public:
    void InitializeCategory(int32 InCategoryIndex);
    FInventoryCategorySelectedDelegate CategorySelected;

private:
    UFUNCTION()
    void HandleClicked();

    int32 CategoryIndex = INDEX_NONE;
};

// Display groups query existing content tags without changing item or equipment data.
// 표시 분류는 아이템과 장비 데이터를 변경하지 않고 기존 콘텐츠 태그를 조회합니다.
struct FInventoryDisplayCategory
{
    FText Label;
    FGameplayTagContainer IconTags;
    FGameplayTagQuery Query;
    bool bSkills = false;
};

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
    void InitializeCategories();
    void AddItem(const FRunItemDefinition& Item, int32 ItemIndex);
    void AddSkill(const USkillDefinitionDataAsset* Skill);
    void SelectCategory(int32 CategoryIndex);
    void SelectItem(int32 ItemIndex);
    void RebuildList();
    void RefreshSelectedItem();
    void ResolveDisplayedSkills();
    bool CanAcceptDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot) const;
    bool HandleDrop(const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot);

    UPROPERTY(Transient)
    FRunPartyMember DisplayedMember;

    UPROPERTY(Transient)
    TArray<FRunWeaponRarityRule> DisplayedRarities;

    bool bCanChangeEquipment = false;
    bool bSkillsUnavailable = false;
    int32 SelectedCategoryIndex = 0;
    int32 SelectedItemIndex = INDEX_NONE;
    TArray<FInventoryDisplayCategory> Categories;

    UPROPERTY(Transient)
    FRunItemDefinition SelectedItem;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInventoryCategoryButton>> CategoryButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> CategoryCounts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UEquipmentItemSlotWidget>> ItemRows;

    TArray<int32> VisibleItemIndices;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USkillDefinitionDataAsset>> DisplayedSkills;

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
    TObjectPtr<UScrollBox> ListScroll;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> ItemList;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> SkillList;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> DetailsPanel;

    UPROPERTY(Transient)
    TObjectPtr<UImage> DetailsIcon;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailsName;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailsSummary;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailsHint;
};
