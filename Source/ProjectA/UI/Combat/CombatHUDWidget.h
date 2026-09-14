#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CombatHUDWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;

// Serialization shell for existing WBP_CombatHUDWidget assets; Gameplay uses round planning.
// 기존 WBP_CombatHUDWidget 에셋의 직렬화 호환 셸이며 Gameplay는 라운드 계획을 사용합니다.
UCLASS()
class PROJECTA_API UCombatHUDWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "BattleHUD")
    FText GetTurnInfoText() const;

protected:
    virtual void NativeOnInitialized() override;

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
};
