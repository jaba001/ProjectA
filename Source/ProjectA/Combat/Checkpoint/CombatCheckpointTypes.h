#pragma once

#include "CoreMinimal.h"
#include "Combat/AI/PartyControlTypes.h"
#include "Combat/Round/CombatRoundTypes.h"
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
    int32 RoundUnitId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FGuid CharacterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FRunAccountId OwnerAccountId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 PartySlot = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    ETeam Team = ETeam::Player;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    EPartyControlMode PartyControlMode = EPartyControlMode::Human;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FSoftObjectPath UnitClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FText CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FCharacterAppearanceSelection Appearance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    float HP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    float MaxHP = 100.0f;

    // Checkpoints predating primary stats retain the original speed baseline.
    // 기본 능력치 도입 이전 체크포인트는 기존 속도 기준값을 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    float Speed = 10.0f;

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
    TArray<FRunConsumableStack> Consumables;

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

    void PostSerialize(const FArchive& Ar)
    {
        if (Ar.IsLoading() && Dexterity_DEPRECATED != -MAX_flt)
        {
            Speed = Dexterity_DEPRECATED;
            Dexterity_DEPRECATED = -MAX_flt;
        }
    }

private:
    // Preserve old checkpoint speed without serializing retired stats into new saves.
    // 기존 체크포인트의 속도를 보존하며 새 저장에는 폐기된 능력치를 기록하지 않습니다.
    UPROPERTY(SaveGame)
    float Dexterity_DEPRECATED = -MAX_flt;
};

template<>
struct TStructOpsTypeTraits<FCombatCheckpointUnit> : public TStructOpsTypeTraitsBase2<FCombatCheckpointUnit>
{
    enum
    {
        WithPostSerialize = true
    };
};

// Commands reference round-local identifiers instead of live actors or network authority tokens.
// 명령은 실행 액터나 네트워크 권위 토큰 대신 라운드 내부 식별자를 참조합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatCheckpointRoundPlan
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 UnitId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FCombatRoundCommand Command;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    bool bHasMovePlan = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FIntPoint MoveDestinationCoord = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    bool bReady = false;
};

// Schema 3 stores a planning boundary before costs; older sequential schemas remain identifiable.
// 스키마 3은 비용 차감 전 계획 경계를 저장하며 이전 순차 턴 스키마와 구분합니다.
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
    int32 RoundNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    int32 PlanRevision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    TArray<FCombatCheckpointRoundPlan> RoundPlans;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    bool bHasOpponentSnapshot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FPartySnapshot OpponentSnapshot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Combat|Checkpoint")
    FSoftObjectPath OpponentCatalog;
};
