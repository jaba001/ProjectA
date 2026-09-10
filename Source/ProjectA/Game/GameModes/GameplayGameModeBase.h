#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/Run/RunIdentityTypes.h"
#include "GameplayGameModeBase.generated.h"

class ACombatArena;
class ACombatManager;
class AEncounterManager;
class UPartyDefinitionDataAsset;
class UEncounterDefinitionDataAsset;
class UOpponentSnapshotCatalogDataAsset;
class APartyPlayerController;
class UCombatActionAuthority;

UCLASS()
class PROJECTA_API AGameplayGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGameplayGameModeBase();
    // Trusted server integration point, never exposed as a client account-claim RPC.
    // 신뢰된 서버 연동 지점이며 클라이언트 계정 주장 RPC로 노출하지 않습니다.
    bool AssignRunParticipant(APartyPlayerController* Controller, const FRunAccountId& AccountId);
    bool ApplyCombatParticipantBindings(UCombatActionAuthority* Authority);

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
    virtual void PostLogin(APlayerController* NewPlayer) override;

private:
    void InitializeGameplay();
    TMap<TWeakObjectPtr<APartyPlayerController>, FRunAccountId> RunParticipants;

    UPROPERTY(Transient)
    TObjectPtr<AEncounterManager> EncounterManager;

    FTimerHandle InitializeTimer;
};
