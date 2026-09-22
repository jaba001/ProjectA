#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Types/CombatResult.h"
#include "EncounterResultWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UGameplayActionButton;
struct FGameplayViewState;

// Reward content can be added here without changing encounter cleanup or map progression.
// 인카운터 정리와 지도 진행을 변경하지 않고 이 화면에 보상 콘텐츠를 추가할 수 있습니다.
UCLASS()
class PROJECTA_API UEncounterResultWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // Results restore menu input so completed combat cannot receive player actions.
    // 결과 화면에서 메뉴 입력을 복원하여 종료한 전투에 플레이어 행동을 전달하지 않습니다.
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    void ShowResult(ECombatResult Result, const FText& Message = FText::GetEmpty());
    void RefreshResult(const FGameplayViewState& View);
    void SetContinueEnabled(bool bEnabled);

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Result;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Continue;

private:
    bool bContinueAllowed = true;
    bool bRewardsComplete = true;
    bool bRewardSelectionAllowed = false;
    ECombatResult DisplayedResult = ECombatResult::None;
    FGuid RewardCharacterId;
    FName RewardNodeId;
    TArray<FName> RewardChoiceIds;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> RewardsContainer;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> RewardsContent;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RewardInstruction;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RewardBalance;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RewardMessage;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGameplayActionButton>> RewardButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> RewardAmounts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> RewardStatuses;

    void HandleRewardSelection(FName ChoiceId);

    UFUNCTION()
    void HandleContinueClicked();
};
