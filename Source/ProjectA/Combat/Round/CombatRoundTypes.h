#pragma once

#include "CoreMinimal.h"
#include "Types/SkillTypes.h"
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
    Cancelled,
    Recovery
};

UENUM(BlueprintType)
enum class ECombatRoundSkillKind : uint8
{
    Melee,
    Projectile,
    GroundAttack,
    // Preserve the serialized Wait value after removing the former cover entry.
    // 기존 엄호 항목을 제거한 뒤에도 직렬화된 대기 값을 유지합니다.
    Wait = 4
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

    // TargetAndSides covers the aimed unit and one adjacent tile on each side at the same formation depth.
    // TargetAndSides는 대상과 같은 전열·후열에서 양옆 한 칸씩을 포함합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Kind == ECombatRoundSkillKind::Melee"))
    ESkillAreaType MeleeArea = ESkillAreaType::Single;

    // Use a forward box to hit every overlapping enemy without consulting tile coordinates.
    // 타일 좌표를 참조하지 않고 전방 박스와 겹치는 모든 적을 타격합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Kind == ECombatRoundSkillKind::Melee"))
    bool bUseMeleeAreaCollision = false;

    // Box half extents in cm: forward depth, lateral width and height; the rear face starts at the caster.
    // cm 단위 박스 반크기이며 전방 깊이·좌우 폭·높이 순서입니다. 뒷면은 시전자 위치에서 시작합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", ClampMax = "1000.0", EditCondition = "bUseMeleeAreaCollision"))
    FVector MeleeAreaHalfExtent = FVector(75.f, 250.f, 100.f);

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

    // Melee approach and return apply the round speed scale to this authored movement rate.
    // 근접 접근과 복귀는 이 작성 이동 속도에 라운드 속도 배율을 적용합니다.
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
    float Speed = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> SkillIds;

    UPROPERTY(BlueprintReadOnly)
    float HP = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint HomeCoord = FIntPoint::ZeroValue;

    UPROPERTY(BlueprintReadOnly)
    FCombatRoundCommand Command;

    // Movement is a separate SAP reservation executed before every AP action.
    // 이동은 모든 AP 행동보다 먼저 실행하는 별도의 SAP 예약입니다.
    UPROPERTY(BlueprintReadOnly)
    bool bHasMovePlan = false;

    UPROPERTY(BlueprintReadOnly)
    FIntPoint MoveDestinationCoord = FIntPoint::ZeroValue;

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
    PROJECTA_API float StartDelay(float HighestSpeed, float UnitSpeed);
    PROJECTA_API float AttackMoveSpeed(const FCombatRoundSkill& Skill, float RoundSpeed);
    PROJECTA_API bool IsTerminal(ECombatRoundActionPhase Phase);
    PROJECTA_API bool IsOwnTerritory(bool bEnemy, FIntPoint Coord);
    PROJECTA_API bool IsValidSkill(const FCombatRoundSkill& Skill);
}
