#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Game/Run/RunTypes.h"
#include "Game/Run/RunParticipationTypes.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Types/CombatResult.h"
#include "RunSaveGame.generated.h"

UCLASS()
class PROJECTA_API URunSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 Version = 2;
    UPROPERTY()
    FRunIdentityData Identity;
    UPROPERTY()
    FRunParticipationData Participation;
    UPROPERTY()
    TArray<FRunPartyMember> Party;
    UPROPERTY()
    TArray<FRunNodeDefinition> Nodes;
    UPROPERTY()
    TArray<FName> CompletedNodes;
    UPROPERTY()
    FName CurrentNode;
    UPROPERTY()
    FName CurrentEncounter;
    UPROPERTY()
    ERunPhase Phase = ERunPhase::None;
    UPROPERTY()
    ECombatResult Result = ECombatResult::None;
    UPROPERTY()
    FSoftObjectPath Catalog;
    UPROPERTY()
    FCombatCheckpointData CombatCheckpoint;
};
