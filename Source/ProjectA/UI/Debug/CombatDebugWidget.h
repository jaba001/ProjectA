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
class UWrapBox;
class UCombatDebugActionButton;
class UCommonActivatableWidgetStack;

enum class ECombatDebugAction : uint8
{
    AddSkill,
    RemoveSkill,
    GrantEquipment,
    RemoveEquipment,
    SelectSkillCategory
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

// Collapsible development tools keep the native combat planner available underneath.
// 접을 수 있는 개발 도구 아래에서 기존 전투 계획 화면을 유지합니다.
UCLASS()
class PROJECTA_API UCombatDebugWidget : public UCommonUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UTextBlock* AddText(UVerticalBox* Box, const FString& Text, int32 Size = 14);
    UButton* AddButton(UVerticalBox* Box, const FString& Text);
    void AddAction(UVerticalBox* Box, const FString& Label, ECombatDebugAction Action, const FSoftObjectPath& Asset = FSoftObjectPath(), int32 Index = INDEX_NONE, FGameplayTag EquipmentSlot = FGameplayTag());
    void RefreshState();
    void RefreshReviveState();
    void RefreshSpawnState();
    void RefreshHealthState(bool bResetInput = false);
    void ApplyHealth(bool bFullHeal);
    bool RefreshUnitOptions();
    void RebuildSpawnOptions();
    void SpawnUnit(bool bEnemy);
    void RebuildLists();
    void RefreshSkillCategories(const TArray<int32>& Counts);
    void HandleAction(UCombatDebugActionButton* Button);

    UFUNCTION()
    void TogglePanel();
    UFUNCTION()
    void ToggleRevivePanel();
    UFUNCTION()
    void ReviveSelectedAlly();
    UFUNCTION()
    void ShowUnitTools();
    UFUNCTION()
    void ApplyUnitHealth();
    UFUNCTION()
    void HealSelectedUnit();
    UFUNCTION()
    void SpawnAlly();
    UFUNCTION()
    void SpawnEnemy();
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
    TObjectPtr<UCommonActivatableWidgetStack> CombatLayer;
    UPROPERTY(Transient)
    TObjectPtr<UBorder> Panel;
    UPROPERTY(Transient)
    TObjectPtr<USizeBox> PanelSize;
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> LoadoutPanel;
    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> UnitToolsPanel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthTarget;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthStatus;
    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> MaxHealthInput;
    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> CurrentHealthInput;
    UPROPERTY(Transient)
    TObjectPtr<UButton> HealthApplyButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> HealButton;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient)
    TObjectPtr<UButton> ReviveToggleButton;
    UPROPERTY(Transient)
    TObjectPtr<UBorder> RevivePanel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ReviveTarget;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ReviveStatus;
    UPROPERTY(Transient)
    TObjectPtr<UButton> ReviveButton;
    UPROPERTY(Transient)
    TObjectPtr<UBorder> SpawnPanel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SpawnCount;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> AllySpawnChoice;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> EnemySpawnChoice;
    UPROPERTY(Transient)
    TObjectPtr<UButton> AllySpawnButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> EnemySpawnButton;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> AllySpawnStatus;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EnemySpawnStatus;
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
    UPROPERTY(Transient)
    TObjectPtr<UWrapBox> SkillCategoryTabs;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UCombatDebugActionButton>> SkillCategoryButtons;

    TMap<FString, int32> UnitOptions;
    TMap<FString, FName> AllySpawnOptions;
    TMap<FString, FName> EnemySpawnOptions;
    int32 SelectedUnitId = INDEX_NONE;
    int32 HealthInputUnitId = INDEX_NONE;
    int32 SelectedSkillCategory = 0;
    FGuid ObservedCombatId;
    int32 ObservedRevision = INDEX_NONE;
    bool bEquipment = false;
    bool bRefreshingUnits = false;
    float RefreshElapsed = 0.f;
    FText ActionMessage;
    FText HealthMessage;
    FText ReviveMessage;
    FText AllySpawnMessage;
    FText EnemySpawnMessage;
};
