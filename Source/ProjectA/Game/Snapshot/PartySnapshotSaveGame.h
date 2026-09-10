#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Game/Snapshot/PartySnapshotTypes.h"
#include "PartySnapshotSaveGame.generated.h"

UCLASS()
class PROJECTA_API UPartySnapshotSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FPartySnapshot Snapshot;
};
