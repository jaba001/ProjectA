#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatDebugGameMode.generated.h"

class ACombatArena;
class ACombatManager;
class AUnitBase;
class UEncounterDefinitionDataAsset;
class UPartyDefinitionDataAsset;

// Own an isolated combat session without reading or writing a Run's party or checkpoint.
// Run의 파티나 체크포인트를 읽고 쓰지 않는 독립 전투 세션을 소유합니다.
UCLASS()
class PROJECTA_API ACombatDebugGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACombatDebugGameMode();
    static bool IsDebugWorld(const UWorld* World);
    ACombatManager* GetCombatManager() const { return CombatManager; }
    const FText& GetStatusMessage() const { return StatusMessage; }
    bool RestartCombat();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DebugCombat")
    TObjectPtr<UPartyDefinitionDataAsset> PartyDefinition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DebugCombat")
    TObjectPtr<UEncounterDefinitionDataAsset> EnemyDefinition;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void InitializeDebugCombat();
    bool SpawnDebugUnits();
    void CleanupDebugCombat();

    UPROPERTY(Transient)
    TObjectPtr<ACombatArena> Arena;

    UPROPERTY(Transient)
    TObjectPtr<ACombatManager> CombatManager;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AUnitBase>> SpawnedUnits;

    FText StatusMessage;
    FTimerHandle InitializeTimer;
};
