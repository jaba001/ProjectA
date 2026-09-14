#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Components/ComboBoxString.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "CombatRoundPlanningWidget.generated.h"

class ACombatRoundCoordinator;
class UButton;
class UTextBlock;
class UVerticalBox;

// Native planning controls require no generated Widget Blueprint assets.
// 생성된 Widget Blueprint 에셋이 필요하지 않은 native 계획 입력 화면입니다.
UCLASS()
class PROJECTA_API UCombatRoundPlanningWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UTextBlock* AddText(UVerticalBox* Box, const FString& Text, int32 FontSize = 15);
    UButton* AddButton(UVerticalBox* Box, const FString& Text);
    UComboBoxString* AddCombo(UVerticalBox* Box, const FString& Label);
    void RefreshView();
    bool RefreshOptions(const ACombatRoundCoordinator* Coordinator, int32 OwnerSlot);
    void LoadSelectedCommand();
    void RefreshSkillDescription();
    void RefreshDestinationOptions();
    const FCombatRoundSkill* GetSelectedSkill() const;
    int32 GetSelectedUnitId() const;
    FCombatRoundCommand BuildSelectedCommand() const;
    bool HasUnappliedChanges() const;

    UFUNCTION()
    void HandleUnitChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleSkillChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleApplyPlan();

    UFUNCTION()
    void HandleReady();

    UFUNCTION()
    void HandleUnready();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Header;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Roster;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SkillDescription;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> UnitChoice;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> SkillChoice;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> TargetChoice;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> TargetTileChoice;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> DestinationChoice;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ApplyButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ReadyButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> UnreadyButton;

    TArray<int32> OwnUnitIds;
    TArray<int32> TargetUnitIds;
    TArray<FName> SkillIds;
    TArray<FIntPoint> TargetCoords;
    TArray<FIntPoint> DestinationCoords;
    FGuid ObservedCombatId;
    int32 ObservedRound = INDEX_NONE;
    float RefreshElapsed = 0.f;
    bool bUpdatingOptions = false;
};
