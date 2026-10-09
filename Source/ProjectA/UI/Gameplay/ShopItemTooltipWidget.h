#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunWeaponSkillTypes.h"
#include "ShopItemTooltipWidget.generated.h"

class UImage;
class USizeBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class PROJECTA_API UShopItemTooltipWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void ConfigureItem(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities, const FText& Status);
    void ConfigureInventory(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities, const FText& State);
    void ConfigureReward(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities, const FText& State);

protected:
    virtual void NativeOnInitialized() override;

private:
    void ConfigureDetails(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities, const FText& Summary, const FText& Hint);
    UTextBlock* AddText(UVerticalBox* Parent, int32 FontSize, float WrapWidth, bool bHeading = false);
    void AddSkillRow();
    void UpdateMaximumHeight();

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> TooltipBounds;

    UPROPERTY(Transient)
    TObjectPtr<UImage> ItemIcon;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ItemName;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PriceStatus;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FooterHint;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EquipmentText;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> SkillList;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EmptySkills;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UVerticalBox>> SkillRows;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SkillNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SkillDescriptions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SkillStats;
};
