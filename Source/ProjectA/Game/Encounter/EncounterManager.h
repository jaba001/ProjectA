#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Run/RunIdentityTypes.h"
#include "Types/CombatResult.h"
#include "EncounterManager.generated.h"

class ACombatArena;
class ACombatManager;
class AUnitBase;
class UPartyDefinitionDataAsset;
class UEncounterDefinitionDataAsset;
class URunStateSubsystem;

DECLARE_MULTICAST_DELEGATE(FOnEncounterFlowChanged);

UCLASS()
class PROJECTA_API AEncounterManager : public AActor
{
    GENERATED_BODY()

public:
    AEncounterManager();
    void InitializeEncounter(ACombatArena* InArena, ACombatManager* InCombatManager, UPartyDefinitionDataAsset* InPartyDefinition, const TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>>& InDefinitions);

    UFUNCTION(BlueprintCallable, Category = "Run")
    bool RequestStartNode(FName NodeId);

    UFUNCTION(BlueprintCallable, Category = "Run")
    bool ContinueRun();
    bool SelectRunEncounter(FName EncounterId);
    bool LeaveRunEncounter();

    // Legacy combat-save entry points reject unsupported timed-round recovery.
    // 기존 전투 저장 진입점은 미지원 시간 기반 라운드 복구를 명시적으로 거절합니다.
    bool RestoreSavedCombat(const FRunAccountId& HostAccount, FText& OutError);
    bool ResumeManagedGameplay(FText& OutError);
    bool RetryCombatCheckpoint(FText& OutError);
    bool CanRetryCombatCheckpoint() const;
    void SuspendForDisconnectedParticipant();
    void ShutdownGameplay();

    FText GetFlowMessage() const { return FlowMessage; }
    ACombatManager* GetCombatManager() const { return CombatManager; }
    const TArray<TObjectPtr<AUnitBase>>& GetSpawnedUnits() const { return SpawnedUnits; }
    FOnEncounterFlowChanged OnFlowChanged;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FEncounterPreparationRetryTest;
    friend class FEncounterContinueRetryTest;
#endif

    bool SpawnEncounter(UEncounterDefinitionDataAsset* Definition);
    void HandleCombatResult(ECombatResult Result);
    void FinishEncounter();
    void CleanupEncounter();
    bool FailPreparation(const FText& Message);
    bool TryAbortPreparation();
    void SetPlayerCombatInput(bool bEnabled);
    bool ConfigureCombatParticipants(FText& OutError);
    bool ValidateManagedExecution(FText& OutError, bool bAllowResumePending = false) const;

    UPROPERTY(Transient)
    TObjectPtr<URunStateSubsystem> RunState;

    UPROPERTY(Transient)
    TObjectPtr<ACombatArena> Arena;

    UPROPERTY(Transient)
    TObjectPtr<ACombatManager> CombatManager;

    UPROPERTY(Transient)
    TObjectPtr<UPartyDefinitionDataAsset> PartyDefinition;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>> Definitions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AUnitBase>> SpawnedUnits;

    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<AUnitBase>> PartyActors;

    FText FlowMessage;
    FText PreparationFailureMessage;
    ECombatResult PendingResult = ECombatResult::None;
    FTimerHandle FinishTimer;
    bool bPreparing = false;
    bool bPreparationAbortPending = false;
    bool bShuttingDown = false;
};
