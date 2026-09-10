#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayGameModeBase.generated.h"

class ACombatArena;
class ACombatManager;
class AEncounterManager;
class UPartyDefinitionDataAsset;
class UEncounterDefinitionDataAsset;
class UOpponentSnapshotCatalogDataAsset;

UCLASS()
class PROJECTA_API AGameplayGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGameplayGameModeBase();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    TSubclassOf<ACombatManager> CombatManagerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    TSubclassOf<AEncounterManager> EncounterManagerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    TObjectPtr<UPartyDefinitionDataAsset> PartyDefinition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>> EncounterDefinitions;

    // The command-line Snapshot selection uses this trusted catalog and a separate run checkpoint.
    // 명령줄 Snapshot 선택은 이 신뢰 목록과 별도 진행 체크포인트를 사용합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay|Snapshot")
    TObjectPtr<UOpponentSnapshotCatalogDataAsset> LocalOpponentCatalog;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    FName ArenaTag = TEXT("GameplayArena");

    UFUNCTION(BlueprintPure, Category = "Gameplay")
    AEncounterManager* GetEncounterManager() const { return EncounterManager; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void InitializeGameplay();

    UPROPERTY(Transient)
    TObjectPtr<AEncounterManager> EncounterManager;

    FTimerHandle InitializeTimer;
};
