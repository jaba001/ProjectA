#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/GameState/GameplayViewTypes.h"
#include "GameplayGameState.generated.h"

class ACombatArena;
class ACombatManager;
class AEncounterManager;
class URunStateSubsystem;

DECLARE_MULTICAST_DELEGATE(FOnGameplayViewChanged);

// GameState distributes server Run presentation while the GameInstance keeps persistence private.
// GameState는 서버 Run의 표시 정보를 배포하며 GameInstance는 저장 책임을 유지합니다.
UCLASS()
class PROJECTA_API AGameplayGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    void InitializeServerView(AEncounterManager* Encounter, ACombatArena* Arena);
    const FGameplayViewState& GetViewState() const { return ViewState; }
    ACombatManager* GetCombatManager() const { return CombatManager; }
    ACombatArena* GetArena() const { return CombatArena; }
    FOnGameplayViewChanged OnGameplayViewChanged;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void RefreshServerView();

    UFUNCTION()
    void OnRep_GameplayView();

    UPROPERTY(ReplicatedUsing = OnRep_GameplayView)
    FGameplayViewState ViewState;

    UPROPERTY(ReplicatedUsing = OnRep_GameplayView)
    TObjectPtr<ACombatManager> CombatManager;

    UPROPERTY(ReplicatedUsing = OnRep_GameplayView)
    TObjectPtr<ACombatArena> CombatArena;

    UPROPERTY(Transient)
    TObjectPtr<URunStateSubsystem> RunState;

    UPROPERTY(Transient)
    TObjectPtr<AEncounterManager> EncounterManager;
};
