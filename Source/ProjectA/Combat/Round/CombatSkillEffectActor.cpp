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
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EffectOrigin")));
}

void ACombatSkillEffectActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !bResolved) return;
    // Auto-destroyed components signal actual completion, including deferred Niagara activation.
    // 자동 삭제된 컴포넌트로 지연된 Niagara 활성화를 포함한 실제 재생 완료를 확인합니다.
    if (GetNetMode() == NM_Standalone && !CombatSkillPresentation::HasActiveAudio(VisualAudio) && !VisualComponents.ContainsByPredicate([](const UFXSystemComponent* Component) { return IsValid(Component); })) Destroy();
}

void ACombatSkillEffectActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatSkillEffectActor, Visual);
    DOREPLIFETIME(ACombatSkillEffectActor, VisualSourcePosition);
    DOREPLIFETIME(ACombatSkillEffectActor, VisualTargetPosition);
    DOREPLIFETIME(ACombatSkillEffectActor, bPresentationReady);
}

void ACombatSkillEffectActor::InitializeEffect(AUnitBase* Source, AUnitBase* Target, FVector AimLocation, const FCombatRoundSkill& Skill, const TArray<FCombatRoundUnitView>& Units, double PresentationTime)
{
    if (!HasAuthority() || bInitialized || bResolved) return;
    bInitialized = true;
    if (!IsValid(Source) || Source->GetWorld() != GetWorld() || !Source->IsUnitAlive() || !Skill.bUseEffectCollision || AimLocation.ContainsNaN() || Skill.EffectHalfExtent.ContainsNaN() || Skill.EffectHalfExtent.GetMin() <= 0.f || Skill.EffectOffset.ContainsNaN() || Skill.EffectTravel.ContainsNaN() || !FMath::IsFinite(Skill.EffectHitDelaySeconds) || Skill.EffectHitDelaySeconds < 0.f || Skill.EffectHitDelaySeconds > 10.f || !FMath::IsFinite(Skill.EffectDuration) || Skill.EffectDuration <= 0.f || Skill.EffectDuration > 10.f || !FMath::IsFinite(Skill.Power) || Skill.Power < 0.f)
    {
        ResolveEffect();
        return;
    }
    Definition = Skill;
    SourceUnit = Source;
    const FVector SourceLocation = Source->GetCapsuleComponent()->GetComponentLocation();
    InitialSourceLocation = SourceLocation;
    PresentationStartedAt = FMath::IsFinite(PresentationTime) && PresentationTime >= 0.0 ? PresentationTime : -1.0;
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
    // Melee occlusion starts at the captured caster origin, independently of the volume center offset.
    // 근접 차폐는 볼륨 중심 오프셋과 별개로 저장한 시전자 초기 원점에서 시작합니다.
    InitialOcclusionOrigin = Skill.Kind == ECombatRoundSkillKind::Melee ? SourceLocation : StartLocation;
    SetActorLocationAndRotation(StartLocation, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    for (const FCombatRoundUnitView& Entry : Units)
    {
        if (IsValid(Entry.Unit) && Entry.Unit->GetWorld() == GetWorld()) AllowedTargets.AddUnique(Entry.Unit);
    }
    Visual = Skill.Vfx;
    VisualSourcePosition = SourceLocation;
    VisualTargetPosition = AimPoint;
    bPresentationReady = true;
    OnRep_Visual();
    if (IsValid(GetOwner())) GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ACombatSkillEffectActor::HandleOwnerDestroyed);
    ForceNetUpdate();
    // Zero-delay effects retain their initial overlap; delayed effects show VFX before enabling any contacts.
    // 지연이 없으면 최초 겹침을 유지하며 지연 효과는 모든 접촉 판정에 앞서 VFX를 표시합니다.
    if (Skill.EffectHitDelaySeconds == 0.f) AdvanceEffect(0.f, PresentationTime);
}

void ACombatSkillEffectActor::AdvanceEffect(float DeltaSeconds, double PresentationTime)
{
    if (!HasAuthority() || !bInitialized || bResolved || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) return;
    AUnitBase* Source = SourceUnit.Get();
    UWorld* World = GetWorld();
    if (!IsValid(Source) || !Source->IsUnitAlive() || !World)
    {
        ResolveEffect();
        return;
    }
    const double TotalDuration = static_cast<double>(Definition.EffectHitDelaySeconds) + Definition.EffectDuration;
    double StepSeconds = FMath::Min(static_cast<double>(DeltaSeconds), TotalDuration - ElapsedSeconds);
    const bool bHasPresentationClock = PresentationStartedAt >= 0.0 && FMath::IsFinite(PresentationTime) && PresentationTime >= 0.0;
    // Old simulation debt cannot consume the visual lead-in or active window before presentation time advances.
    // 표현 시간이 진행되기 전에 누적된 시뮬레이션 시간이 연출 선행 구간이나 활성 판정 구간을 소진하지 못하게 합니다.
    if (bHasPresentationClock) StepSeconds = FMath::Min(StepSeconds, FMath::Max(0.0, PresentationTime - PresentationStartedAt - ElapsedSeconds));
    if (StepSeconds <= 0.0 && ((bHitWindowStarted && bHasPresentationClock) || ElapsedSeconds < Definition.EffectHitDelaySeconds)) return;
    ElapsedSeconds = FMath::Min(ElapsedSeconds + StepSeconds, TotalDuration);
    if (ElapsedSeconds < Definition.EffectHitDelaySeconds) return;
    const double ActiveSeconds = ElapsedSeconds - Definition.EffectHitDelaySeconds;
    const FVector Start = GetActorLocation();
    FVector End = FMath::Lerp(StartLocation, EndLocation, ActiveSeconds / Definition.EffectDuration);
    const FQuat Rotation = GetActorQuat();
    const FCollisionShape Shape = Definition.bEffectSphere ? FCollisionShape::MakeSphere(Definition.EffectHalfExtent.X) : FCollisionShape::MakeBox(Definition.EffectHalfExtent);
    const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, this);
    const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
    if (!bHitWindowStarted)
    {
        bHitWindowStarted = true;
        // Check the current world at activation, retaining the captured launch origin and no pre-delay sweep.
        // 지연 전 궤적을 검사하지 않고 저장한 발동 원점에서 활성화 시점의 현재 월드를 검사합니다.
        if (Definition.Kind != ECombatRoundSkillKind::Melee && World->LineTraceTestByChannel(InitialSourceLocation, StartLocation, ECC_WorldDynamic, Params, Responses))
        {
            ResolveEffect();
            return;
        }
    }
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
        if (!Definition.ImpactVfx.Niagara.IsNull() || !Definition.ImpactVfx.Cascade.IsNull() || !Definition.ImpactVfx.Sound.IsNull()) MulticastImpact(Definition.ImpactVfx, FTransform(Rotation, Contact.Point));
        OnImpact.Broadcast(SourceUnit.Get(), Target, Definition.Power);
    }
    if (bHitWall || ElapsedSeconds >= TotalDuration) ResolveEffect();
}

void ACombatSkillEffectActor::ResolveEffect(bool bDestroyActor)
{
    if (!HasAuthority() || bResolved) return;
    bResolved = true;
    OnImpact.Clear();
    OnResolved.Broadcast(this);
    OnResolved.Clear();
    if (bDestroyActor && !IsActorBeingDestroyed())
    {
        // Collision completion must not cut off the authored effect before its first rendered frame.
        // 충돌 판정 완료가 작성된 효과의 첫 렌더링 프레임 이전에 표현을 끊지 않도록 합니다.
        if (Visual.Niagara.IsNull() && Visual.Cascade.IsNull() && Visual.Sound.IsNull()) Destroy();
        else
        {
            // Natural completion releases one-shot systems; cap imported loops without extending damage or round locks.
            // 단발 시스템은 자연 완료로 해제하며 피해나 라운드 잠금을 연장하지 않고 임포트한 반복 효과를 제한합니다.
            SetLifeSpan(Visual.Sound.IsNull() ? 5.f : FMath::Max(5.f, FMath::Clamp(Visual.SoundMaxDuration, 0.01f, 60.f)));
            SetActorTickEnabled(GetNetMode() == NM_Standalone);
        }
    }
}

void ACombatSkillEffectActor::OnRep_Visual()
{
    // Activate once after all launch data exists, even when replicated configuration arrived in an earlier frame.
    // 복제된 설정이 앞선 프레임에 도착했더라도 모든 발동 데이터가 준비된 뒤 한 번만 활성화합니다.
    if (IsActorBeingDestroyed() || !bPresentationReady || bPresentationAttached) return;
    bPresentationAttached = true;
    const CombatSkillPresentation::FEndpointParameters Endpoints{VisualSourcePosition, VisualTargetPosition};
    CombatSkillPresentation::Attach(this, Visual, VisualComponents, VisualAudio, Endpoints, true);
}

void ACombatSkillEffectActor::HandleOwnerDestroyed(AActor* DestroyedActor)
{
    // Resolved visuals are no longer in the active hit list, so explicitly clean them up with their battle owner.
    // 종료된 표현은 활성 판정 목록에서 빠지므로 소유 전투가 제거될 때 명시적으로 함께 정리합니다.
    Destroy();
}

void ACombatSkillEffectActor::MulticastImpact_Implementation(const FCombatSkillVfx& ImpactVisual, const FTransform& Transform)
{
    CombatSkillPresentation::Impact(GetWorld(), ImpactVisual, Transform);
}

void ACombatSkillEffectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResolveEffect(false);
    if (IsValid(GetOwner())) GetOwner()->OnDestroyed.RemoveDynamic(this, &ACombatSkillEffectActor::HandleOwnerDestroyed);
    CombatSkillPresentation::Destroy(VisualComponents);
    CombatSkillPresentation::StopAudio(VisualAudio);
    Super::EndPlay(EndPlayReason);
}
