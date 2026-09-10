#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "RunMapWidget.generated.h"

class URunStateSubsystem;
class UTextBlock;
class UVerticalBox;
struct FGameplayViewState;

UCLASS()
class PROJECTA_API URunMapWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // Node selection uses menu input and leaves the mouse cursor visible.
    // 노드 선택에는 메뉴 입력을 사용하고 마우스 커서를 표시합니다.
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    void RefreshRunMap(const URunStateSubsystem* RunState, const FText& FlowMessage);
    void RefreshRunMapView(const FGameplayViewState& View, bool bAllowRunCommands);

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Progress;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Party;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_FlowMessage;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> NodeList;

private:
    bool bRunCommandsAllowed = true;
    void HandleNodeSelected(FName NodeId);
};
