#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Game/Run/RunTypes.h"
#include "GameplayRootWidget.generated.h"

class UCommonActivatableWidgetStack;
class UCombatHUDWidget;
class UEncounterResultWidget;
class URunMapWidget;
class URunStateSubsystem;

UCLASS()
class PROJECTA_API UGameplayRootWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void RefreshFlow(const URunStateSubsystem* RunState, const FText& FlowMessage);

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
    TSubclassOf<URunMapWidget> RunMapWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
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
    ERunPhase DisplayedPhase = ERunPhase::None;
    bool bHasDisplayedPhase = false;

    UPROPERTY(Transient)
    TObjectPtr<URunMapWidget> RunMapWidget;

    UPROPERTY(Transient)
    TObjectPtr<UEncounterResultWidget> ResultWidget;
};
