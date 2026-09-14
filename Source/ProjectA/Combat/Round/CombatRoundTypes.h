#pragma once

#include "CoreMinimal.h"
#include "CombatRoundTypes.generated.h"

class AUnitBase;

UENUM(BlueprintType)
enum class ECombatRoundPhase : uint8
{
    WaitingForPlayers,
    Planning,
    Resolving,
    Finished,
    Suspended
};

UENUM(BlueprintType)
enum class ECombatRoundActionPhase : uint8
{
    Planned,
    Waiting,
    Approaching,
    Casting,
    Returning,
    Complete,
    Cancelled
};

UENUM(BlueprintType)
enum class ECombatRoundSkillKind : uint8
{
    Melee,
    Projectile,
    GroundAttack,
    Guard,
    Wait
};

UENUM(BlueprintType)
enum class ECombatRoundApproach : uint8
{
    None,
    Unit,
    Tile
};

UENUM(BlueprintType)
enum class ECombatRoundTargetLoss : uint8
{
    Cancel,
    KeepLocation,
    NearestEnemy
};

// Prototype skill values are independent of the existing GAS skill catalogue.
// 프로토타입 스킬 수치는 기존 GAS 스킬 카탈로그와 별도로 관리합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatRoundSkill
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECombatRoundSkillKind Kind = ECombatRoundSkillKind::Melee;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECombatRoundApproach Approach = ECombatRoundApproach::Unit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECombatRoundTargetLoss TargetLoss = ECombatRoundTargetLoss::Cancel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bRemainAtDestination = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHoming = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bTargetOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float WindupSeconds = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Power = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float HitRange = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MoveSpeed = 700.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ProjectileSpeed = 700.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ProjectileRadius = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ProjectileLifetime = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ActionPointCost = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SubActionPointCost = 0;
};

// Requests carry identifiers and coordinates, never client-authored damage or timing.
// 요청은 식별자와 좌표만 전달하며 클라이언트가 피해나 시각을 지정하지 않습니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatRoundCommand
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 UnitId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    FName SkillId;

    UPROPERTY(BlueprintReadOnly)
    int32 TargetUnitId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint TargetCoord = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint DestinationCoord = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FCombatRoundUnitView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 UnitId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    int32 OwnerSlot = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bEnemy = false;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AUnitBase> Unit = nullptr;

    UPROPERTY(BlueprintReadOnly)
    int32 Speed = 20;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> SkillIds;

    UPROPERTY(BlueprintReadOnly)
    float HP = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Guard = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint HomeCoord = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadOnly)
    FCombatRoundCommand Command;

    UPROPERTY(BlueprintReadOnly)
    bool bReady = false;

    UPROPERTY(BlueprintReadOnly)
    ECombatRoundActionPhase ActionPhase = ECombatRoundActionPhase::Planned;

    UPROPERTY(BlueprintReadOnly)
    float StartDelay = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FText Status;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FCombatRoundView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid CombatId;

    UPROPERTY(BlueprintReadOnly)
    int32 RoundNumber = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 PlanRevision = 0;

    UPROPERTY(BlueprintReadOnly)
    ECombatRoundPhase Phase = ECombatRoundPhase::WaitingForPlayers;

    UPROPERTY(BlueprintReadOnly)
    float ElapsedSeconds = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 PendingProjectiles = 0;

    UPROPERTY(BlueprintReadOnly)
    FText Message;

    UPROPERTY(BlueprintReadOnly)
    TArray<FCombatRoundUnitView> Units;
};

namespace CombatRoundRules
{
    PROJECTA_API float StartDelay(int32 HighestSpeed, int32 UnitSpeed);
    PROJECTA_API bool IsTerminal(ECombatRoundActionPhase Phase);
    PROJECTA_API bool IsOwnTerritory(bool bEnemy, FIntPoint Coord);
    PROJECTA_API bool IsValidSkill(const FCombatRoundSkill& Skill);
}
