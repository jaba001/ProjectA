#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "Unit/CharacterAppearanceTypes.h"
#include "PartySnapshotTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FPartySnapshotStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    float MaxHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    float CurrentHP = 100.0f;

    // Snapshots predating primary stats retain the original speed baseline.
    // 기본 능력치 도입 이전 스냅샷은 기존 속도 기준값을 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    float Speed = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 MaxActionPoints = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 MaxSubActionPoints = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 MoveRange = 1;

    void PostSerialize(const FArchive& Ar)
    {
        if (Ar.IsLoading() && Dexterity_DEPRECATED != -MAX_flt)
        {
            Speed = Dexterity_DEPRECATED;
            Dexterity_DEPRECATED = -MAX_flt;
        }
    }

private:
    // Load legacy values even when cooked SaveGame archives do not apply property redirects; never write them again.
    // cooked SaveGame이 프로퍼티 리디렉션을 적용하지 않아도 기존 값을 읽으며 새 저장에는 기록하지 않습니다.
    UPROPERTY(SaveGame)
    float Dexterity_DEPRECATED = -MAX_flt;
};

template<>
struct TStructOpsTypeTraits<FPartySnapshotStats> : public TStructOpsTypeTraitsBase2<FPartySnapshotStats>
{
    enum
    {
        WithPostSerialize = true
    };
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

    // Cosmetics are independent of equipment effects and use trusted catalog identifiers.
    // 외형은 장비 효과와 별개이며 신뢰된 목록의 식별자를 사용합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FCharacterAppearanceSelection Appearance;

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
