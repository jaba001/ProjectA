#pragma once

#include "CoreMinimal.h"
#include "PartySnapshotTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FPartySnapshotStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    float MaxHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    float CurrentHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 MaxActionPoints = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 MaxSubActionPoints = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 MoveRange = 1;
};

// Stable identifiers describe a build without owning actors or loading arbitrary assets.
// 액터를 소유하거나 임의 에셋을 로드하지 않고 고정 식별자로 빌드를 표현합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FPartySnapshotMember
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FName MemberId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FName ClassId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FPartySnapshotStats Stats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    TArray<FName> SkillIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    TArray<FName> EquipmentIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FName TacticsId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 FormationSlot = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPartySnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 ContentVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FName SnapshotId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    TArray<FPartySnapshotMember> Members;
};
