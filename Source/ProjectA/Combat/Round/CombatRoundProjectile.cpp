#include "Combat/Round/CombatRoundProjectile.h"
#include "Combat/Library/CombatCollisionPolicy.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Unit/UnitBase.h"

namespace
{
    struct FProjectileCandidate
    {
        AUnitBase* Unit = nullptr;
        FVector Location = FVector::ZeroVector;
        float Time = 0.0f;
    };
}

ACombatRoundProjectile::ACombatRoundProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
    SourceTeam = ETeam::Player;

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    SetRootComponent(ProjectileMesh);
    ProjectileMesh->SetMobility(EComponentMobility::Movable);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMesh->SetGenerateOverlapEvents(false);
    ProjectileMesh->SetCanEverAffectNavigation(false);
    ProjectileMesh->SetCastShadow(false);

    // Reference the engine mesh for temporary presentation without modifying the asset.
    // 에셋을 수정하지 않고 엔진 메시를 임시 표현에 참조합니다.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        ProjectileMesh->SetStaticMesh(SphereMesh.Object);
    }
    OnRep_VisualRadius();
}

void ACombatRoundProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatRoundProjectile, VisualRadius);
}

void ACombatRoundProjectile::SetAllowedTargets(const TArray<AUnitBase*>& Targets)
{
    if (!HasAuthority() || bInitialized || bResolved) return;
    bRestrictTargets = true;
    AllowedTargets.Reset();
    for (AUnitBase* Unit : Targets)
    {
        if (IsValid(Unit) && Unit->GetWorld() == GetWorld()) AllowedTargets.Add(Unit);
    }
}

void ACombatRoundProjectile::InitializeProjectile(AUnitBase* Source, AUnitBase* Target, FVector AimPoint, float Speed, float Damage, float Radius, float Lifetime, bool bHoming, bool bTargetOnly)
{
    if (!HasAuthority() || bInitialized || bResolved)
    {
        return;
    }
    bInitialized = true;
    if (!IsValid(Source) || Source->GetWorld() != GetWorld() || !Source->IsUnitAlive() || AimPoint.ContainsNaN() || !FMath::IsFinite(Speed) || Speed <= 0.0f || !FMath::IsFinite(Damage) || Damage < 0.0f || !FMath::IsFinite(Radius) || Radius <= 0.0f || !FMath::IsFinite(Lifetime) || Lifetime <= 0.0f)
    {
        ResolveProjectile();
        return;
    }
    SourceUnit = Source;
    SourceTeam = Source->GetTeam();
    TargetUnit = IsValid(Target) && Target->GetWorld() == GetWorld() ? Target : nullptr;
    bOnlyTarget = bTargetOnly;
    if (bOnlyTarget && !IsEligibleTarget(TargetUnit.Get()))
    {
        ResolveProjectile();
        return;
    }
    TargetPoint = AimPoint;
    FlightSpeed = Speed;
    DamageAmount = Damage;
    CollisionRadius = Radius;
    VisualRadius = Radius;
    RemainingLifetime = Lifetime;
    bTrackTarget = bHoming;
    OnRep_VisualRadius();
    ForceNetUpdate();
}

bool ACombatRoundProjectile::IsEligibleTarget(AUnitBase* Unit) const
{
    if (!CombatCollisionPolicy::IsLivingEnemy(GetWorld(), SourceUnit.Get(), SourceTeam, Unit) || (bOnlyTarget && Unit != TargetUnit.Get()) || (bRestrictTargets && !AllowedTargets.Contains(TWeakObjectPtr<AUnitBase>(Unit)))) return false;
    return CombatSkillExecution::MatchesOwnedTags(Unit, TargetTagQuery, TargetRequiredTags, TargetBlockedTags);
}

void ACombatRoundProjectile::SetTargetTagConditions(const FGameplayTagQuery& Query, const FGameplayTagContainer& RequiredTags, const FGameplayTagContainer& BlockedTags)
{
    if (!HasAuthority() || bInitialized || bResolved) return;
    TargetTagQuery = Query;
    TargetRequiredTags = RequiredTags;
    TargetBlockedTags = BlockedTags;
}

void ACombatRoundProjectile::AdvanceProjectile(float DeltaSeconds)
{
    if (!HasAuthority() || !bInitialized || bResolved || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return;
    }
    if (!SourceUnit.IsValid() || !GetWorld())
    {
        ResolveProjectile();
        return;
    }

    // A dead caster does not cancel a launched projectile; a dead target leaves the last aim point.
    // 사망한 시전자는 발사체를 취소하지 않으며 대상 사망 후에는 마지막 조준점을 유지합니다.
    if (bTrackTarget && IsEligibleTarget(TargetUnit.Get()))
    {
        TargetPoint = TargetUnit->GetActorLocation();
    }
    const FVector Start = GetActorLocation();
    if (Start.ContainsNaN() || TargetPoint.ContainsNaN())
    {
        ResolveProjectile();
        return;
    }
    const float StepSeconds = FMath::Min(DeltaSeconds, RemainingLifetime);
    const FVector Offset = TargetPoint - Start;
    const double Distance = Offset.Size();
    if (!FMath::IsFinite(Distance))
    {
        ResolveProjectile();
        return;
    }
    const double Travel = FMath::Min(static_cast<double>(FlightSpeed) * StepSeconds, Distance);
    const FVector End = Distance > UE_SMALL_NUMBER ? Start + Offset * (Travel / Distance) : Start;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CombatRoundProjectile), false, this);
    QueryParams.bFindInitialOverlaps = true;
    const FCollisionQueryParams WorldQueryParams = CombatCollisionPolicy::WorldQuery(GetWorld(), this);
    const FCollisionResponseParams WorldResponses = CombatCollisionPolicy::WorldResponses();
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    const FCollisionShape Shape = FCollisionShape::MakeSphere(CollisionRadius);
    int32 CandidateLimit = 0;

    // Ignore ineligible pawns before the sweep so they cannot hide a valid hit behind them.
    // 부적격 폰이 뒤쪽 유효 피격을 가리지 않도록 스윕 전에 제외합니다.
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        if (IsEligibleTarget(Cast<AUnitBase>(*It)))
        {
            ++CandidateLimit;
        }
        else
        {
            QueryParams.AddIgnoredActor(*It);
        }
    }

    // World geometry must block WorldDynamic; visibility-only grid boxes and overlap sensors are not walls.
    // 월드 지형은 WorldDynamic을 Block해야 하며 Visibility 전용 그리드 박스와 Overlap 센서는 벽이 아닙니다.
    if (GetWorld()->OverlapBlockingTestByChannel(Start, FQuat::Identity, ECC_WorldDynamic, Shape, WorldQueryParams, WorldResponses))
    {
        ResolveProjectile();
        return;
    }
    FHitResult WorldHit;
    const bool bHitWorld = GetWorld()->SweepSingleByChannel(WorldHit, Start, End, FQuat::Identity, ECC_WorldDynamic, Shape, WorldQueryParams, WorldResponses);

    // Only the combat capsule receives hits; character meshes and attached sensors do not expand the target.
    // 전투 캡슐만 피격되며 캐릭터 메시와 부착 센서는 대상의 피격 범위를 넓히지 않습니다.
    TArray<FProjectileCandidate> Candidates;
    TArray<FOverlapResult> InitialOverlaps;
    GetWorld()->OverlapMultiByObjectType(InitialOverlaps, Start, FQuat::Identity, ObjectParams, Shape, QueryParams);
    for (const FOverlapResult& Overlap : InitialOverlaps)
    {
        AUnitBase* Unit = Cast<AUnitBase>(Overlap.GetActor());
        if (IsEligibleTarget(Unit) && Overlap.GetComponent() == Unit->GetCapsuleComponent())
        {
            Candidates.Add({Unit, Start, 0.0f});
            QueryParams.AddIgnoredActor(Unit);
        }
    }

    // Collect contacts before resolving and ignore each accepted unit in subsequent bounded queries.
    // 종료 전에 접촉을 수집하고 유닛 수로 제한한 후속 조회에서 이미 수집한 유닛을 제외합니다.
    for (int32 QueryIndex = 0; QueryIndex < CandidateLimit; ++QueryIndex)
    {
        TArray<FHitResult> Hits;
        GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams, Shape, QueryParams);
        bool bFoundCandidate = false;
        for (const FHitResult& Hit : Hits)
        {
            AUnitBase* Unit = Cast<AUnitBase>(Hit.GetActor());
            if (IsEligibleTarget(Unit) && Hit.GetComponent() == Unit->GetCapsuleComponent())
            {
                Candidates.Add({Unit, Hit.bStartPenetrating ? Start : FVector(Hit.Location), Hit.bStartPenetrating ? 0.0f : Hit.Time});
                QueryParams.AddIgnoredActor(Unit);
                bFoundCandidate = true;
            }
        }
        if (!bFoundCandidate)
        {
            break;
        }
    }
    Candidates.Sort([](const FProjectileCandidate& Left, const FProjectileCandidate& Right)
    {
        if (Left.Time != Right.Time)
        {
            return Left.Time < Right.Time;
        }
        if (Left.Unit->UnitIndex != Right.Unit->UnitIndex)
        {
            return Left.Unit->UnitIndex < Right.Unit->UnitIndex;
        }
        return Left.Unit->GetFName().LexicalLess(Right.Unit->GetFName());
    });
    // A blocking wall wins an equal-time contact so an overlapping capsule cannot receive damage through it.
    // 차단 벽은 동시 접촉에서 우선하므로 겹친 캡슐이 벽 너머 피해를 받지 않습니다.
    const float WorldHitTime = WorldHit.bStartPenetrating ? 0.0f : WorldHit.Time;
    if (bHitWorld && (Candidates.IsEmpty() || CombatCollisionPolicy::IsBlockedByWorld(bHitWorld, WorldHitTime, Candidates[0].Time)))
    {
        SetActorLocation(WorldHit.bStartPenetrating ? Start : FVector(WorldHit.Location), false, nullptr, ETeleportType::TeleportPhysics);
        ResolveProjectile();
        return;
    }
    if (!Candidates.IsEmpty())
    {
        const FProjectileCandidate& Hit = Candidates[0];
        SetActorLocation(Hit.Location, false, nullptr, ETeleportType::TeleportPhysics);
        ResolveProjectile(Hit.Unit);
        return;
    }

    SetActorLocation(End, false, nullptr, ETeleportType::TeleportPhysics);
    RemainingLifetime = FMath::Max(0.0f, RemainingLifetime - StepSeconds);
    if (Travel >= Distance || RemainingLifetime <= 0.0f)
    {
        ResolveProjectile();
    }
}

void ACombatRoundProjectile::ResolveProjectile(AUnitBase* HitUnit, bool bDestroyActor)
{
    if (!HasAuthority() || bResolved)
    {
        return;
    }

    // Mark terminal state first because damage and completion listeners can trigger reentrant cleanup.
    // 피해와 완료 수신자가 정리를 재진입할 수 있으므로 종료 상태를 먼저 기록합니다.
    bResolved = true;
    if (SourceUnit.IsValid() && IsEligibleTarget(HitUnit))
    {
        OnImpact.Broadcast(SourceUnit.Get(), HitUnit, DamageAmount);
    }
    OnImpact.Clear();
    OnResolved.Broadcast(this);
    OnResolved.Clear();
    if (bDestroyActor && !IsActorBeingDestroyed())
    {
        Destroy();
    }
}

void ACombatRoundProjectile::OnRep_VisualRadius()
{
    if (ProjectileMesh)
    {
        ProjectileMesh->SetRelativeScale3D(FVector(VisualRadius / 50.0f));
    }
}

void ACombatRoundProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResolveProjectile(nullptr, false);
    Super::EndPlay(EndPlayReason);
}
