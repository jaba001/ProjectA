#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Run/TargetRunTypes.h"
#include "TargetRunDefinitionDataAsset.generated.h"

struct FProfessionDefinition;
struct FRunProgressView;

UCLASS(BlueprintType)
class PROJECTA_API UTargetRunDefinitionDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UTargetRunDefinitionDataAsset();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FTargetRunGroup> Groups;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FRunEncounterOffer> EncounterPool;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTagQuery EncounterQuery;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FSoftObjectPath OpponentCatalog;

    bool BuildState(FRunTargetState& OutState, FText& OutError) const;
    static bool Validate(const FRunTargetState& State, const FRunProgressView& Progress, const FRunEncounterProgress& Encounter, FText& OutError);
    static bool BuildOffers(const FRunTargetState& State, int32 CombatIndex, int32 VisitIndex, TArray<FRunEncounterOffer>& OutOffers);
    static void ApplyGrowth(const FRunTargetState& State, int32 CompletedCombats, FProfessionDefinition& Profession);
};
