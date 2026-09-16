#pragma once

#include "CoreMinimal.h"
#include "CombatRoundTypes.generated.h"

class AUnitBase;
class UAnimMontage;

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

// Round execution values and optional presentation are resolved without activating legacy abilities.
// 기존 어빌리티를 활성화하지 않고 라운드 실행 수치와 선택적 표현을 해석합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatRoundSkill
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Name;

    // Presentation does not control authoritative release timing or damage.
    // 표현은 서버의 발동 시점이나 피해 판정을 제어하지 않습니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TObjectPtr<UAnimMontage> CastMontage = nullptr;

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

    // Restrict projectile collisions to the aimed unit only when the skill explicitly requires it.
    // 스킬이 명시적으로 요구하는 경우에만 투사체 충돌을 조준 유닛으로 제한합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Kind == ECombatRoundSkillKind::Projectile"))
    bool bTargetOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float WindupSeconds = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Power = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float HitRange = 150.f;

    // The melee sweep radius is capped at half the forward reach to keep its volume in front of the caster.
    // 근접 스윕 반경은 전방 사거리의 절반으로 제한하여 판정 범위를 시전자 앞에 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", ClampMax = "1000.0", EditCondition = "Kind == ECombatRoundSkillKind::Melee"))
    float MeleeRadius = 35.f;

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
