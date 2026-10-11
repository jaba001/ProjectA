#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Components/Button.h"
#include "UI/MainMenu/EncyclopediaCatalog.h"
#include "EncyclopediaWidget.generated.h"

class UEditableTextBox;
class UTextBlock;
class UScrollBox;
class UHorizontalBox;
class UPartyDefinitionDataAsset;
struct FStreamableHandle;

// Relay a native indexed callback while preserving UButton keyboard and mouse behavior.
// UButton의 키보드·마우스 동작을 보존하며 인덱스가 있는 네이티브 콜백을 전달합니다.
UCLASS()
class PROJECTA_API UEncyclopediaChoiceButton : public UButton
{
    GENERATED_BODY()

public:
    FSimpleDelegate Selected;
    void BindSelection();

private:
    UFUNCTION()
    void HandleClicked();
};

UCLASS()
class PROJECTA_API UEncyclopediaWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UEncyclopediaWidget();
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UEncyclopediaChoiceButton* MakeButton(const FText& Text);
    UTextBlock* MakeText(const FText& Text, int32 FontSize, bool bHeading = false);
    void BeginLoad();
    void LoadDefinitions();
    void FinishLoad();
    void StopLoad();
    void SelectCategory(int32 Index);
    void RebuildSubcategories();
    void SelectSubcategory(int32 Index);
    void RefreshEntries();
    void SelectEntry(int32 Index);

    UFUNCTION()
    void HandleSearchChanged(const FText& Text);
    UFUNCTION()
    void HandleBack();

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> Search;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Count;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailTitle;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailBody;
    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> List;
    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> Details;
    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> SubcategoryScroll;
    UPROPERTY(Transient)
    TObjectPtr<UHorizontalBox> SubcategoryBar;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UEncyclopediaChoiceButton>> SubcategoryButtons;
    UPROPERTY(Transient)
    TObjectPtr<UEncyclopediaChoiceButton> Back;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UEncyclopediaChoiceButton>> Tabs;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UEncyclopediaChoiceButton>> Rows;
    UPROPERTY(Transient)
    TObjectPtr<UPartyDefinitionDataAsset> Party;

    TArray<FEncyclopediaEntry> Entries;
    TArray<FSoftObjectPath> SkillPaths;
    TArray<int32> VisibleIndices;
    TArray<FEncyclopediaSubcategory> Subcategories;
    TSharedPtr<FStreamableHandle> LoadHandle;
    FDelegateHandle RegistryLoadedHandle;
    EEncyclopediaCategory ActiveCategory = EEncyclopediaCategory::Equipment;
    int32 SelectedIndex = INDEX_NONE;
    int32 ActiveSubcategory = 0;
    bool bLoading = false;
    bool bLoadedRuleDependencies = false;
};
