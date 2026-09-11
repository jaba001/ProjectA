#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "RunEncounterWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UGameplayActionButton;
struct FGameplayViewState;

UCLASS()
class PROJECTA_API URunEncounterWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    void RefreshEncounter(const FGameplayViewState& View, bool bAllowRunCommands);

protected:
    virtual void NativeOnInitialized() override;

private:
    void HandleSelection(FName EncounterId);
    void HandleLeave(FName ActionId);
    bool bRunCommandsAllowed = false;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Title;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Message;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> Actions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGameplayActionButton>> ChoiceButtons;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayActionButton> LeaveButton;
};
