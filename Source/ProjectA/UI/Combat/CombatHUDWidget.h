#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CombatHUDWidget.generated.h"

class ACombatManager;
class AUnitBase;
class UBorder;
class UButton;
class UGameplayActionButton;
class UHorizontalBox;
class USkillDefinitionDataAsset;
class UTextBlock;

// CommonUI combat screen with native controls for the existing controller actions.
// 기존 컨트롤러 행동을 실행하는 네이티브 컨트롤이 있는 CommonUI 전투 화면입니다.
UCLASS()
class PROJECTA_API UCombatHUDWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // Let CommonUI route combat input to both the HUD and the game viewport.
    // CommonUI가 전투 입력을 HUD와 게임 뷰포트 모두에 전달하도록 합니다.
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    UFUNCTION(BlueprintCallable, Category = "BattleHUD")
    FText GetTurnInfoText() const;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CommandPanelBackground;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Turn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Action;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> SkillList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Move;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_EndTurn;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Cancel;

private:
    void RefreshControls();
    void RebuildSkills(AUnitBase* ActiveUnit);
    void HandleSkillSelected(FName SkillId);

    UFUNCTION()
    void HandleMoveClicked();

    UFUNCTION()
    void HandleItemClicked();

    UPROPERTY(Transient)
    TObjectPtr<UButton> Button_Item;

    UFUNCTION()
    void HandleEndTurnClicked();

    UFUNCTION()
    void HandleCancelClicked();

    UPROPERTY(Transient)
    TWeakObjectPtr<ACombatManager> CachedCombatManager;

    UPROPERTY(Transient)
    TWeakObjectPtr<AUnitBase> DisplayedUnit;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USkillDefinitionDataAsset>> DisplayedLoadout;

    bool bDisplayedPlayerUnit = false;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<USkillDefinitionDataAsset>> DisplayedSkills;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGameplayActionButton>> SkillButtons;
};
