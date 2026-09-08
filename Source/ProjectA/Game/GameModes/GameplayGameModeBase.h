#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayGameModeBase.generated.h"

class ACombatArena;
class ACombatManager;
class AEncounterManager;
class UPartyDefinitionDataAsset;
class UEncounterDefinitionDataAsset;

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
