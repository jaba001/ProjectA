#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "CombatActionTypes.generated.h"

UENUM(BlueprintType)
enum class ECombatActionKind : uint8
{
    Move,
    Skill,
    HealingItem,
    EndTurn
};

UENUM(BlueprintType)
enum class ECombatRequestResult : uint8
{
    Accepted,
    Pending,
    InvalidContext,
    InvalidRequest,
    DuplicateRequest,
    UnboundParticipant,
    NotOwner,
    UnavailableUnit,
    InvalidSkill,
    InvalidTarget,
    InsufficientResources
};

// Commands carry intent and stable values, never caller-supplied accounts, actors or costs.
// 명령은 의도와 값만 전달하며 호출자가 지정한 계정, 액터 또는 비용을 포함하지 않습니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatActionRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FGuid RunId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    int32 HostEpoch = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FGuid CombatInstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FGuid ParticipantBindingId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    int32 TurnSerial = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    int64 RequestSequence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FGuid UnitId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    ECombatActionKind Kind = ECombatActionKind::Move;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FPrimaryAssetId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FIntPoint TargetCoord = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Request")
    FGuid TargetUnitId;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FCombatActionResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Request")
    FGuid CombatInstanceId;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Request")
    int64 RequestSequence = 0;

    // Accepted confirms dispatch, not the eventual success of an asynchronous unit action.
    // Accepted는 전달 완료이며 비동기 유닛 행동의 최종 성공을 의미하지 않습니다.
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Request")
    ECombatRequestResult Result = ECombatRequestResult::InvalidRequest;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Request")
    FText Message;
};
