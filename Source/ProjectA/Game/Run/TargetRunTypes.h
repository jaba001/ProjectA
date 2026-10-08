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
    float SpeedGrowth = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> GoldChoices;

    void PostSerialize(const FArchive& Ar)
    {
        if (Ar.IsLoading() && AttributeGrowth_DEPRECATED != -MAX_flt)
        {
            SpeedGrowth = AttributeGrowth_DEPRECATED;
            AttributeGrowth_DEPRECATED = -MAX_flt;
        }
    }

private:
    // Carry only the former dexterity growth into speed while preserving the frozen Run table.
    // Run에 고정된 표를 유지하며 기존 공통 성장 중 민첩 성장분만 속도로 이관합니다.
    UPROPERTY(SaveGame)
    float AttributeGrowth_DEPRECATED = -MAX_flt;
};

template<>
struct TStructOpsTypeTraits<FTargetRunGroup> : public TStructOpsTypeTraitsBase2<FTargetRunGroup>
{
    enum
    {
        WithPostSerialize = true
    };
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

    // Version zero keeps saved fixed rotation; new Runs freeze a weighted policy and one original seed.
    // 버전 0은 저장된 고정 순환을 유지하며 새 Run은 가중치 정책과 최초 시드 하나를 고정합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 EncounterSelectionVersion = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 EncounterSeed = 0;

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
