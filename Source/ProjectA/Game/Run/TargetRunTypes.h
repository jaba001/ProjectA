#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Game/Run/RunEncounterTypes.h"
#include "Game/Run/RunRecoveryTypes.h"
#include "Game/Snapshot/PartySnapshotTypes.h"
#include "TargetRunTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FTargetRunGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagContainer Tags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FSoftClassPath> EnemyClasses;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FPartySnapshot Opponent;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MaxHPGrowth = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float AttributeGrowth = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> GoldChoices;
};

// Freeze the trial table and chosen encounters in the Run, independent of external sample saves.
// 외부 샘플 저장과 분리하여 시험 수치와 선택한 인카운터를 Run에 고정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunTargetState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 SchemaVersion = 0;
    UPROPERTY(BlueprintReadOnly)
    TArray<FTargetRunGroup> Groups;
    UPROPERTY(BlueprintReadOnly)
    TArray<FRunEncounterOffer> EncounterPool;
    UPROPERTY(BlueprintReadOnly)
    FGameplayTagQuery EncounterQuery;
    UPROPERTY(BlueprintReadOnly)
    TArray<FName> CompletedEncounterChoices;
    UPROPERTY(BlueprintReadOnly)
    FSoftObjectPath OpponentCatalog;
    UPROPERTY()
    TMap<FName, FSoftClassPath> SnapshotClasses;
    UPROPERTY()
    TMap<FName, FSoftObjectPath> SnapshotSkills;
    UPROPERTY(BlueprintReadOnly)
    FRunRecoveryState Recovery;
};
