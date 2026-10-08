#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunLevelDesignTypes.generated.h"

// Freeze monster identity, authored tags and combat stats without owning live actors.
// 실행 액터를 소유하지 않고 몬스터 식별자·제작 태그·전투 능력치를 고정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunMonsterDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FName MonsterId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FSoftClassPath UnitClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FGameplayTagContainer Tags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float MaxHP = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 AP = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 SAP = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float Speed = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 MoveRange = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FSoftObjectPath Skill;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float Weight = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunLevelRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FGameplayTagQuery EnemyQuery;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FGameplayTagQuery LeaderQuery;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 EnemyCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float HPScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float SpeedScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 GoldMin = 5;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 GoldMax = 10;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float MaxHPGrowth = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float SpeedGrowth = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float RestHP = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    int32 SnapshotCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float SnapshotHP = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float SnapshotSpeed = 10.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunLevelDesignState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 SchemaVersion = 0;
    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 Seed = 0;
    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 PartySize = 0;
    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FRunMonsterDefinition> Catalog;
    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FRunLevelRule> Rules;
};
