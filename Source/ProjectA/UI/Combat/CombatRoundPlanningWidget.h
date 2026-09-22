#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "CombatRoundPlanningWidget.generated.h"

class ACombatRoundCoordinator;
class ACombatRoundPlayerController;
class ACombatGridTile;
class UBorder;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UWrapBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatRoundSkillPicked, FName);

// Each equipped skill button carries its stable identifier instead of a display-name lookup.
// 장착 스킬 버튼은 표시 이름 검색 대신 고유 식별자를 전달합니다.
UCLASS()
class PROJECTA_API UCombatRoundSkillButton : public UButton
{
    GENERATED_BODY()

public:
    void InitializeSkill(FName InSkillId);
    FName GetSkillId() const { return SkillId; }
    FOnCombatRoundSkillPicked OnSkillPicked;

private:
    UFUNCTION()
    void HandleClicked();

    FName SkillId;
};

// World target selection and equipped skills share the existing authoritative round planner.
// 전장 대상 선택과 장착 스킬은 기존 서버 권위 라운드 계획을 함께 사용합니다.
UCLASS()
class PROJECTA_API UCombatRoundPlanningWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UTextBlock* AddText(UVerticalBox* Box, const FString& Text, int32 FontSize = 15);
    UButton* AddButton(UVerticalBox* Box, const FString& Text);
    void RefreshView();
    void RefreshPartyCards(const ACombatRoundCoordinator* Coordinator, int32 OwnerSlot);
    bool RefreshOptions(const ACombatRoundCoordinator* Coordinator, int32 OwnerSlot);
    void LoadSelectedCommand();
    void RefreshHighlights();
    void ClearHighlights();
    void UnbindWorldInput();
    bool CanEdit() const;
    const FCombatRoundSkill* GetSelectedSkill() const;
    int32 GetSelectedUnitId() const;
    FCombatRoundCommand BuildCommand(FName SkillId) const;
    bool CanReadyPlans(FText& OutError) const;
    void HandleWorldUnitClicked(int32 UnitId);
    void HandleWorldTileClicked(FIntPoint Coord);
    void HandleSkillPicked(FName SkillId);

    UFUNCTION()
    void HandleUnitChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleMove();

    UFUNCTION()
    void HandleCancelMovePlan();

    UFUNCTION()
    void HandleCancelSkillPlan();

    UFUNCTION()
    void HandleReady();

    UFUNCTION()
    void HandleUnready();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Header;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Roster;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EnemyRoster;

    UPROPERTY(Transient)
    TObjectPtr<UHorizontalBox> PartyList;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> PartyCards;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> PartyNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> PartyDetails;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> UnitDetails;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MovePlanDetails;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TargetDetails;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SkillDescription;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> UnitChoice;

    UPROPERTY(Transient)
    TObjectPtr<UWrapBox> SkillList;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCombatRoundSkillButton>> SkillButtons;

    UPROPERTY(Transient)
    TObjectPtr<UButton> MoveButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CancelMovePlanButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CancelSkillPlanButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ReadyButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> UnreadyButton;

    TWeakObjectPtr<ACombatRoundPlayerController> BoundController;
    TArray<TWeakObjectPtr<ACombatGridTile>> HighlightedTiles;
    TArray<int32> OwnUnitIds;
    TArray<int32> PartyUnitIds;
    TArray<FName> SkillIds;
    FName SelectedSkillId;
    int32 SelectedTargetId = INDEX_NONE;
    FIntPoint SelectedTargetCoord = FIntPoint::ZeroValue;
    bool bHasTargetTile = false;
    bool bChoosingMove = false;
    FText LocalStatus;
    FGuid ObservedCombatId;
    int32 ObservedRound = INDEX_NONE;
    float RefreshElapsed = 0.f;
    bool bUpdatingOptions = false;
};
