#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Game/Run/RunTypes.h"
#include "GameplayRootWidget.generated.h"

class UCommonActivatableWidgetStack;
class UCombatHUDWidget;
class UEncounterResultWidget;
class URunMapWidget;
class URunEncounterWidget;
class URunStateSubsystem;
class UBorder;
class UButton;
class UTextBlock;
struct FGameplayViewState;
class ADevelopmentCoopLobby;
class UDevelopmentCoopWidget;

UCLASS()
class PROJECTA_API UGameplayRootWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void RefreshFlow(const URunStateSubsystem* RunState, const FText& FlowMessage);
    void RefreshFlowView(const FGameplayViewState& View, bool bAllowRunCommands, bool bCanRetryCheckpoint = false);
    void RefreshDevelopmentLobby(ADevelopmentCoopLobby* Lobby);

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
    TSubclassOf<URunMapWidget> RunMapWidgetClass;

    // Retained only to deserialize authored root widgets; active combat uses the native round screen.
    // 작성된 루트 위젯 역직렬화용으로만 보존하며 실제 전투는 native 라운드 화면을 사용합니다.
    UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Combat uses the native round planning screen."))
    TSubclassOf<UCombatHUDWidget> CombatHUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
    TSubclassOf<UEncounterResultWidget> ResultWidgetClass;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> RunLayer;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> CombatLayer;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActivatableWidgetStack> ModalLayer;

private:
    UFUNCTION()
    void HandleLeaveDevelopmentCoop();
    UPROPERTY(Transient)
    TObjectPtr<UDevelopmentCoopWidget> DevelopmentWidget;
    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidgetStack> DevelopmentLayer;
    UPROPERTY(Transient)
    TObjectPtr<UBorder> DevelopmentBar;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DevelopmentMessage;
    UFUNCTION()
    void HandleRetryCheckpoint();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> CheckpointNotice;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CheckpointMessage;

    UPROPERTY(Transient)
    TObjectPtr<UButton> RetryCheckpointButton;

    ERunPhase DisplayedPhase = ERunPhase::None;
    bool bHasDisplayedPhase = false;

    UPROPERTY(Transient)
    TObjectPtr<URunMapWidget> RunMapWidget;

    UPROPERTY(Transient)
    TObjectPtr<UEncounterResultWidget> ResultWidget;

    UPROPERTY(Transient)
    TObjectPtr<URunEncounterWidget> RunEncounterWidget;
};
