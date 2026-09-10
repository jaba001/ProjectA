#pragma once

#include "CoreMinimal.h"
#include "Controller/PartyPlayerController.h"
#include "GameplayPlayerController.generated.h"

class AEncounterManager;
class UGameplayRootWidget;
class URunStateSubsystem;
class AGameplayGameState;

// Reuses grid input while owning only gameplay UI and input routing.
// 그리드 입력을 재사용하고 Gameplay UI와 입력 전환만 소유합니다.
UCLASS()
class PROJECTA_API AGameplayPlayerController : public APartyPlayerController
{
    GENERATED_BODY()

public:
    AGameplayPlayerController();
    void InitializeGameplay(AEncounterManager* InEncounterManager);
    bool CanIssueRunCommands() const;
    void RefreshRunFlowPermissions();

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void RequestStartNode(FName NodeId);

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void RequestContinueRun();

    void RequestRetryCombatCheckpoint();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool ShouldCreateCombatHUD() const override { return false; }

    UPROPERTY(EditDefaultsOnly, Category = "Gameplay|UI")
    TSubclassOf<UGameplayRootWidget> GameplayRootWidgetClass;

private:
    void RefreshGameplayFlow();
    bool CanRetryGameplayRecovery() const;
    void TryBindGameplayState();
    FTimerHandle BindStateTimer;

    UPROPERTY(Transient)
    TObjectPtr<AGameplayGameState> GameplayState;

    UPROPERTY(Transient)
    TObjectPtr<UGameplayRootWidget> GameplayRootWidget;

    UPROPERTY(Transient)
    TObjectPtr<URunStateSubsystem> RunState;

    UPROPERTY(Transient)
    TObjectPtr<AEncounterManager> EncounterManager;
};
