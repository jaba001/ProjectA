#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "GameplayTagContainer.h"
#include "CombatDebugWidget.generated.h"

class UVerticalBox;
class UScrollBox;
class UTextBlock;
class UEditableTextBox;
class UComboBoxString;
class USizeBox;
class UBorder;
class UCombatDebugActionButton;

enum class ECombatDebugAction : uint8
{
    AddSkill,
    RemoveSkill,
    GrantEquipment,
    RemoveEquipment
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatDebugAction, UCombatDebugActionButton*);

UCLASS()
class PROJECTA_API UCombatDebugActionButton : public UButton
{
    GENERATED_BODY()

public:
    void Initialize(ECombatDebugAction InAction, const FSoftObjectPath& InAsset, int32 InIndex, FGameplayTag InSlot);
    ECombatDebugAction Action = ECombatDebugAction::AddSkill;
    FSoftObjectPath Asset;
    int32 Index = INDEX_NONE;
    FGameplayTag EquipmentSlot;
    FOnCombatDebugAction OnAction;

private:
    UFUNCTION()
    void HandleClicked();
};

// A compact overlay keeps the native combat planner active underneath the development tools.
// 작은 오버레이 아래에서 기존 전투 계획 화면을 유지합니다.
UCLASS()
class PROJECTA_API UCombatDebugWidget : public UCommonUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UTextBlock* AddText(UVerticalBox* Box, const FString& Text, int32 Size = 14);
    UButton* AddButton(UVerticalBox* Box, const FString& Text);
    void AddAction(UVerticalBox* Box, const FString& Label, ECombatDebugAction Action, const FSoftObjectPath& Asset = FSoftObjectPath(), int32 Index = INDEX_NONE, FGameplayTag EquipmentSlot = FGameplayTag());
    void RefreshState();
    void RebuildLists();
    void HandleAction(UCombatDebugActionButton* Button);

    UFUNCTION()
    void TogglePanel();
    UFUNCTION()
    void ShowSkills();
    UFUNCTION()
    void ShowEquipment();
    UFUNCTION()
    void RestartCombat();
    UFUNCTION()
    void HandleSearch(const FText& Text);
    UFUNCTION()
    void HandleUnit(FString Value, ESelectInfo::Type SelectionType);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> Panel;
    UPROPERTY(Transient)
    TObjectPtr<USizeBox> PanelSize;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CatalogTitle;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> UnitChoice;
    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> Search;
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> OwnedList;
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> CatalogList;
    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> CatalogScroll;

    TMap<FString, int32> UnitOptions;
    int32 SelectedUnitId = INDEX_NONE;
    FGuid ObservedCombatId;
    int32 ObservedRevision = INDEX_NONE;
    bool bEquipment = false;
    bool bRefreshingUnits = false;
    float RefreshElapsed = 0.f;
    FText ActionMessage;
};
