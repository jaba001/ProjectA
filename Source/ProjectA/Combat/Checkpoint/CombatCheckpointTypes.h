#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunIdentityTypes.h"
#include "Game/Snapshot/PartySnapshotTypes.h"
#include "Unit/UnitBase.h"
#include "CombatCheckpointTypes.generated.h"

// Checkpoint IDs describe saved units and are never reused as live command IDs.
// 체크포인트 ID는 저장 유닛을 나타내며 실행 중 명령 ID로 재사용하지 않습니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatCheckpointUnit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FGuid UnitId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FGuid CharacterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FRunAccountId OwnerAccountId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 PartySlot = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    ETeam Team = ETeam::Player;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FSoftObjectPath UnitClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FText CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    float HP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    float MaxHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 AP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 MaxAP = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 SubAP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 MaxSubAP = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 MoveRange = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 HealingItemCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    float HealingItemAmount = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    bool bDead = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    bool bHasTile = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FIntPoint GridCoord = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FTransform Transform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    TArray<FSoftObjectPath> Skills;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FSoftObjectPath DefaultAttackAbility;
};

// Store an idle boundary before the next turn starts; array order is the turn order.
// 다음 턴 시작 전 유휴 경계를 저장하며 배열 순서가 턴 순서입니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatCheckpointData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 ContentVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FGuid AttemptId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int64 Revision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FRunIdentityData Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FName NodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FName EncounterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 CompletedTurnSerial = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 NextTurnIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    TArray<FCombatCheckpointUnit> Units;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    bool bHasOpponentSnapshot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FPartySnapshot OpponentSnapshot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FSoftObjectPath OpponentCatalog;
};
