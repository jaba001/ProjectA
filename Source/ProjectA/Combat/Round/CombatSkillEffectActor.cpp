#include "Combat/Round/CombatSkillEffectActor.h"
#include "Combat/Library/CombatCollisionPolicy.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "Combat/Round/CombatSkillPresentation.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "Unit/UnitBase.h"

namespace
{
    struct FEffectContact
    {
        TWeakObjectPtr<AUnitBase> Unit;
        FVector Point = FVector::ZeroVector;
        float Time = 0.f;
        int32 UnitId = INDEX_NONE;
    };
}

ACombatSkillEffectActor::ACombatSkillEffectActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EffectOrigin")));
}

void ACombatSkillEffectActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatSkillEffectActor, Visual);
}

void ACombatSkillEffectActor::InitializeEffect(AUnitBase* Source, AUnitBase* Target, FVector AimLocation, const FCombatRoundSkill& Skill, const TArray<FCombatRoundUnitView>& Units)
{
    if (!HasAuthority() || bInitialized || bResolved) return;
    bInitialized = true;
    if (!IsValid(Source) || Source->GetWorld() != GetWorld() || !Source->IsUnitAlive() || !Skill.bUseEffectCollision || AimLocation.ContainsNaN() || Skill.EffectHalfExtent.ContainsNaN() || Skill.EffectHalfExtent.GetMin() <= 0.f || Skill.EffectOffset.ContainsNaN() || Skill.EffectTravel.ContainsNaN() || !FMath::IsFinite(Skill.EffectDuration) || Skill.EffectDuration <= 0.f || !FMath::IsFinite(Skill.Power) || Skill.Power < 0.f)
    {
        ResolveEffect();
        return;
    }
    Definition = Skill;
    SourceUnit = Source;
    const FVector SourceLocation = Source->GetCapsuleComponent()->GetComponentLocation();
    const bool bUnitTarget = Skill.TargetRule == ESkillTargetRule::EnemyUnit || Skill.TargetRule == ESkillTargetRule::AllyUnit || Skill.TargetRule == ESkillTargetRule::AnyUnit;
    TargetUnit = Target;
    bOnlyTarget = bUnitTarget && Skill.bTargetOnly;
    if (bOnlyTarget && !CombatSkillExecution::IsValidEffectTarget(Source, Target, Skill))
    {
        ResolveEffect();
        return;
    }
    const FVector AimPoint = bUnitTarget && IsValid(Target) && Target->GetWorld() == GetWorld() ? Target->GetCapsuleComponent()->GetComponentLocation() : AimLocation;
    FVector Direction = AimPoint - SourceLocation;
    Direction.Z = 0.f;
    const FQuat Rotation = Direction.IsNearlyZero() ? Source->GetActorQuat() : Direction.Rotation().Quaternion();
    const FVector Origin = Skill.Kind == ECombatRoundSkillKind::GroundAttack ? AimPoint : SourceLocation;
    StartLocation = Origin + Rotation.RotateVector(Skill.EffectOffset);
    EndLocation = StartLocation + Rotation.RotateVector(Skill.EffectTravel);
    if (StartLocation.ContainsNaN() || EndLocation.ContainsNaN())
    {
        ResolveEffect();
        return;
    }
    const FCollisionQueryParams WorldQuery = CombatCollisionPolicy::WorldQuery(GetWorld(), this);
    // Melee occlusion starts at the captured caster origin, independently of the volume center offset.
    // 근접 차폐는 볼륨 중심 오프셋과 별개로 저장한 시전자 초기 원점에서 시작합니다.
    InitialOcclusionOrigin = Skill.Kind == ECombatRoundSkillKind::Melee ? SourceLocation : StartLocation;
    if (Skill.Kind != ECombatRoundSkillKind::Melee && GetWorld()->LineTraceTestByChannel(SourceLocation, StartLocation, ECC_WorldDynamic, WorldQuery, CombatCollisionPolicy::WorldResponses()))
    {
        ResolveEffect();
        return;
    }
    SetActorLocationAndRotation(StartLocation, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    for (const FCombatRoundUnitView& Entry : Units)
    {
        if (IsValid(Entry.Unit) && Entry.Unit->GetWorld() == GetWorld()) AllowedTargets.AddUnique(Entry.Unit);
    }
    Visual = Skill.Vfx;
    OnRep_Visual();
    ForceNetUpdate();
    // Resolve initial overlaps once so stationary areas and support effects require no artificial movement.
    // 정지 영역과 지원 효과에 불필요한 이동이 필요하지 않도록 최초 겹침도 한 번 검사합니다.
    AdvanceEffect(0.f);
}

void ACombatSkillEffectActor::AdvanceEffect(float DeltaSeconds)
{
    if (!HasAuthority() || !bInitialized || bResolved || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) return;
    AUnitBase* Source = SourceUnit.Get();
    UWorld* World = GetWorld();
    if (!IsValid(Source) || !Source->IsUnitAlive() || !World)
    {
        ResolveEffect();
        return;
    }
    const float NextElapsed = FMath::Min(ElapsedSeconds + DeltaSeconds, Definition.EffectDuration);
    const FVector Start = GetActorLocation();
    FVector End = FMath::Lerp(StartLocation, EndLocation, NextElapsed / Definition.EffectDuration);
    const FQuat Rotation = GetActorQuat();
    const FCollisionShape Shape = Definition.bEffectSphere ? FCollisionShape::MakeSphere(Definition.EffectHalfExtent.X) : FCollisionShape::MakeBox(Definition.EffectHalfExtent);
    const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, this);
    const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
    const FVector OcclusionStart = InitialOcclusionOrigin + Start - StartLocation;
    const FVector OcclusionEnd = InitialOcclusionOrigin + End - StartLocation;
    if (World->OverlapBlockingTestByChannel(OcclusionStart, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses))
    {
        ResolveEffect();
        return;
    }
    // Clip travel at the occlusion origin while the authored volume and VFX retain the same displacement.
    // 차폐 원점에서 이동을 벽에 제한하며 작성된 볼륨과 VFX는 같은 변위를 유지합니다.
    FHitResult WallHit;
    const bool bHitWall = World->SweepSingleByChannel(WallHit, OcclusionStart, OcclusionEnd, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses);
    if (bHitWall) End = WallHit.bStartPenetrating ? Start : Start + FVector(WallHit.Location) - OcclusionStart;
    TArray<FEffectContact> Contacts;
    for (const TWeakObjectPtr<AUnitBase>& WeakTarget : AllowedTargets)
    {
        AUnitBase* Target = WeakTarget.Get();
        if (HitOnceUnits.Contains(WeakTarget) || (bOnlyTarget && Target != TargetUnit.Get()) || !CombatSkillExecution::IsValidEffectTarget(Source, Target, Definition) || !Target->GetActorEnableCollision()) continue;
        UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
        if (!IsValid(Capsule) || !Capsule->IsQueryCollisionEnabled()) continue;
        FHitResult Hit;
        const bool bOverlap = Capsule->OverlapComponent(Start, Rotation, Shape);
        if (!bOverlap && !Capsule->SweepComponent(Hit, Start, End, Rotation, Shape)) continue;
        const float HitTime = bOverlap || Hit.bStartPenetrating ? 0.f : Hit.Time;
        const FVector ContactOrigin = OcclusionStart + (End - Start) * HitTime;
        FVector Contact;
        if (Capsule->GetClosestPointOnCollision(ContactOrigin, Contact) < 0.f) continue;
        if (World->LineTraceTestByChannel(ContactOrigin, Contact, ECC_WorldDynamic, Params, Responses)) continue;
        Contacts.Add({WeakTarget, Contact, HitTime, Target->UnitIndex});
    }
    Contacts.Sort([](const FEffectContact& Left, const FEffectContact& Right)
    {
        return CombatCollisionPolicy::IsEarlierContact(Left.Time, Left.UnitId, Right.Time, Right.UnitId);
    });
    SetActorLocation(End, false, nullptr, ETeleportType::TeleportPhysics);
    ElapsedSeconds = NextElapsed;
    for (const FEffectContact& Contact : Contacts)
    {
        if (bResolved || IsActorBeingDestroyed()) return;
        AUnitBase* Target = Contact.Unit.Get();
        if (!SourceUnit.IsValid() || !SourceUnit->IsUnitAlive())
        {
            ResolveEffect();
            return;
        }
        if (!CombatSkillExecution::IsValidEffectTarget(SourceUnit.Get(), Target, Definition)) continue;
        // Record the unit before callbacks because effect application may reenter combat cleanup.
        // 효과 적용이 전투 정리에 재진입할 수 있으므로 콜백 전에 피격 유닛을 기록합니다.
        HitOnceUnits.Add(Contact.Unit);
        if (!Definition.ImpactVfx.Niagara.IsNull() || !Definition.ImpactVfx.Cascade.IsNull()) MulticastImpact(Definition.ImpactVfx, FTransform(Rotation, Contact.Point));
        OnImpact.Broadcast(SourceUnit.Get(), Target, Definition.Power);
    }
    if (bHitWall || ElapsedSeconds >= Definition.EffectDuration) ResolveEffect();
}

void ACombatSkillEffectActor::ResolveEffect(bool bDestroyActor)
{
    if (!HasAuthority() || bResolved) return;
    bResolved = true;
    OnImpact.Clear();
    OnResolved.Broadcast(this);
    OnResolved.Clear();
    if (bDestroyActor && !IsActorBeingDestroyed()) Destroy();
}

void ACombatSkillEffectActor::OnRep_Visual()
{
    if (!IsActorBeingDestroyed()) CombatSkillPresentation::Attach(this, Visual, VisualComponents);
}

void ACombatSkillEffectActor::MulticastImpact_Implementation(const FCombatSkillVfx& ImpactVisual, const FTransform& Transform)
{
    CombatSkillPresentation::Impact(GetWorld(), ImpactVisual, Transform);
}

void ACombatSkillEffectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResolveEffect(false);
    CombatSkillPresentation::Destroy(VisualComponents);
    Super::EndPlay(EndPlayReason);
}
