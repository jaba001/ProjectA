#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameModeSelectionWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

// Selects the existing solo or local multiplayer flow before entering character creation or a room.
// 캐릭터 생성이나 방에 진입하기 전에 기존 싱글 또는 로컬 멀티플레이 흐름을 선택합니다.
UCLASS()
class PROJECTA_API UGameModeSelectionWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UGameModeSelectionWidget();
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnActivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UButton* AddButton(UVerticalBox* Parent, FName Name, const FText& Text);
    void RefreshAvailability();

    UFUNCTION()
    void HandleSinglePlayer();
    UFUNCTION()
    void HandleMultiplayer();
    UFUNCTION()
    void HandleBack();

    UPROPERTY(Transient)
    TObjectPtr<UButton> SinglePlayerButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> MultiplayerButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> BackButton;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MultiplayerNotice;
};
