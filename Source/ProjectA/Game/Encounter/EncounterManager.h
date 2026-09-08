#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

    FText GetFlowMessage() const { return FlowMessage; }
    ACombatManager* GetCombatManager() const { return CombatManager; }
    const TArray<TObjectPtr<AUnitBase>>& GetSpawnedUnits() const { return SpawnedUnits; }
    FOnEncounterFlowChanged OnFlowChanged;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool SpawnEncounter(UEncounterDefinitionDataAsset* Definition);
    void HandleCombatResult(ECombatResult Result);
    void FinishEncounter();
    void CleanupEncounter();
    bool FailPreparation(const FText& Message);
    void SetPlayerCombatInput(bool bEnabled);

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
    ECombatResult PendingResult = ECombatResult::None;
    FTimerHandle FinishTimer;
    bool bPreparing = false;
};
