#include "Combat/Round/CombatChainEffectActor.h"
#include "Combat/Library/CombatCollisionPolicy.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "Unit/UnitBase.h"

namespace
{
    float NaturalPresentationLimit(const FCombatSkillVfx& Visual)
    {
        return Visual.Sound.IsNull() ? 5.f : FMath::Max(5.f, FMath::Clamp(Visual.SoundMaxDuration, 0.01f, 60.f));
    }
}

ACombatChainEffectActor::ACombatChainEffectActor()
{
    SetReplicateMovement(false);
}

void ACombatChainEffectActor::Tick(float DeltaSeconds)
{
    // The base Tick inspects its single visual; this actor owns independent segment presentations.
    // 기본 Tick은 단일 연출을 검사하며 이 액터는 독립적인 구간 연출을 소유합니다.
    AActor::Tick(DeltaSeconds);
    OnRep_ChainPresentation();
    UpdatePresentations();
    if (HasAuthority() && HasResolved() && Presentations.IsEmpty() && GetNetMode() == NM_Standalone) Destroy();
}

void ACombatChainEffectActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatChainEffectActor, ChainPresentation);
    DOREPLIFETIME(ACombatChainEffectActor, Segments);
}

void ACombatChainEffectActor::InitializeEffect(AUnitBase* Source, AUnitBase* Target, FVector AimLocation, const FCombatRoundSkill& Skill, const TArray<FCombatRoundUnitView>& Units, double PresentationTime)
{
    if (!HasAuthority() || bChainInitialized || HasResolved()) return;
    bChainInitialized = true;
    if (!IsValid(Source) || Source->GetWorld() != GetWorld() || !Source->IsUnitAlive() || !CombatRoundRules::UsesChain(Skill) || Skill.Chain.MaxTargets <= 1 || !CombatRoundRules::IsValidSkill(Skill) || !CombatSkillExecution::IsValidEffectTarget(Source, Target, Skill) || AimLocation.ContainsNaN())
    {
        ResolveChain();
        return;
    }
    ChainDefinition = Skill;
    for (const FCombatRoundUnitView& Entry : Units)
    {
        if (!IsValid(Entry.Unit) || Entry.Unit->GetWorld() != GetWorld()) continue;
        if (Entry.UnitId == INDEX_NONE || RegisteredUnits.Contains(Entry.UnitId))
        {
            ResolveChain();
            return;
        }
        RegisteredUnits.Add(Entry.UnitId, Entry.Unit);
        if (Entry.Unit == Source) Runtime.SourceUnitId = Entry.UnitId;
        if (Entry.Unit == Target) Runtime.TargetUnitId = Entry.UnitId;
    }
    UCapsuleComponent* SourceCapsule = Source->GetCapsuleComponent();
    UCapsuleComponent* TargetCapsule = nullptr;
    if (!IsValid(SourceCapsule) || Runtime.SourceUnitId == INDEX_NONE || Runtime.TargetUnitId == INDEX_NONE || !IsEligibleTarget(Target, TargetCapsule))
    {
        ResolveChain();
        return;
    }
    const FVector SourcePosition = SourceCapsule->GetComponentLocation();
    const FVector TargetPosition = TargetCapsule->GetComponentLocation();
    if (SourcePosition.ContainsNaN() || TargetPosition.ContainsNaN())
    {
        ResolveChain();
        return;
    }
    FVector Direction = TargetPosition - SourcePosition;
    Direction.Z = 0.f;
    Runtime.InitialRotation = Direction.IsNearlyZero() ? Source->GetActorQuat() : Direction.Rotation().Quaternion();
    if (Runtime.InitialRotation.ContainsNaN() || !Runtime.InitialRotation.IsNormalized())
    {
        ResolveChain();
        return;
    }
    ChainPresentation.Vfx = Skill.Vfx;
    ChainPresentation.ImpactVfx = Skill.ImpactVfx;
    ChainPresentation.EffectOffset = Skill.EffectOffset;
    // Keep a bounded natural tail for the final segment and separate audio; each jump retires earlier particles.
    // 마지막 구간과 별도 사운드의 자연 완료 시간을 제한하며 각 점프에서 이전 파티클을 제거합니다.
    ChainPresentation.VisualLifetime = FMath::Max(Skill.EffectHitDelaySeconds + Skill.EffectDuration, Skill.Chain.JumpIntervalSeconds + Skill.EffectDuration) + NaturalPresentationLimit(Skill.Vfx);
    ChainPresentation.MaxTargets = Skill.Chain.MaxTargets;
    ChainPresentation.bReady = true;
    SetActorLocationAndRotation(SourcePosition, Runtime.InitialRotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (IsValid(GetOwner())) GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ACombatChainEffectActor::HandleChainOwnerDestroyed);
    SetActorTickEnabled(true);
    BeginSegment(Runtime.SourceUnitId, Runtime.TargetUnitId, SourcePosition, PresentationTime);
    if (Skill.EffectHitDelaySeconds == 0.f) AdvanceEffect(0.f, PresentationTime);
}

AUnitBase* ACombatChainEffectActor::ResolveRegisteredUnit(int32 UnitId) const
{
    const TWeakObjectPtr<AUnitBase>* Unit = RegisteredUnits.Find(UnitId);
    return Unit ? Unit->Get() : nullptr;
}

AUnitBase* ACombatChainEffectActor::ResolveVisualUnit(int32 UnitId) const
{
    if (HasAuthority()) return ResolveRegisteredUnit(UnitId);
    const ACombatRoundCoordinator* Coordinator = Cast<ACombatRoundCoordinator>(GetOwner());
    if (!IsValid(Coordinator)) return nullptr;
    const FCombatRoundUnitView* Entry = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Unit) { return Unit.UnitId == UnitId; });
    return Entry ? Entry->Unit.Get() : nullptr;
}

bool ACombatChainEffectActor::IsEligibleTarget(AUnitBase* Target, UCapsuleComponent*& OutCapsule) const
{
    OutCapsule = nullptr;
    const AUnitBase* Source = ResolveRegisteredUnit(Runtime.SourceUnitId);
    if (!CombatSkillExecution::IsValidEffectTarget(Source, Target, ChainDefinition) || !Target->GetActorEnableCollision()) return false;
    OutCapsule = Target->GetCapsuleComponent();
    return IsValid(OutCapsule) && OutCapsule->IsQueryCollisionEnabled() && !OutCapsule->GetComponentLocation().ContainsNaN();
}

bool ACombatChainEffectActor::IsUnblockedContact(FVector Origin, UCapsuleComponent* Capsule, FVector& OutContact, bool* bOriginBlocked) const
{
    if (bOriginBlocked) *bOriginBlocked = false;
    UWorld* World = GetWorld();
    if (!World || Origin.ContainsNaN() || !IsValid(Capsule)) return false;
    const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, this);
    const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
    if (World->OverlapBlockingTestByChannel(Origin, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses))
    {
        if (bOriginBlocked) *bOriginBlocked = true;
        return false;
    }
    if (Capsule->GetClosestPointOnCollision(Origin, OutContact) < 0.f || OutContact.ContainsNaN()) return false;
    return !World->LineTraceTestByChannel(Origin, OutContact, ECC_WorldDynamic, Params, Responses);
}

ACombatChainEffectActor::EContactResult ACombatChainEffectActor::CheckCurrentContact(AUnitBase*& OutTarget, FVector& OutContact) const
{
    OutTarget = ResolveRegisteredUnit(Runtime.TargetUnitId);
    UCapsuleComponent* Capsule = nullptr;
    if (Runtime.HitUnitIds.Contains(Runtime.TargetUnitId) || !IsEligibleTarget(OutTarget, Capsule)) return EContactResult::Invalid;
    if (Runtime.HopIndex == 0)
    {
        const FVector Center = Runtime.SegmentSourcePosition + Runtime.InitialRotation.RotateVector(ChainDefinition.EffectOffset);
        const FCollisionShape Shape = ChainDefinition.bEffectSphere ? FCollisionShape::MakeSphere(ChainDefinition.EffectHalfExtent.X) : FCollisionShape::MakeBox(ChainDefinition.EffectHalfExtent);
        bool bOriginBlocked = false;
        const bool bContactUnblocked = IsUnblockedContact(Runtime.SegmentSourcePosition, Capsule, OutContact, &bOriginBlocked);
        if (bOriginBlocked) return EContactResult::Invalid;
        // Preserve the first effect's active window when an enemy enters the volume or emerges from cover later.
        // 적이 나중에 판정 영역에 들어오거나 엄폐에서 벗어나면 최초 효과의 활성 구간 동안 다시 검사합니다.
        if (!bContactUnblocked || !Capsule->OverlapComponent(Center, Runtime.InitialRotation, Shape)) return Runtime.ElapsedSeconds < ChainDefinition.EffectHitDelaySeconds + ChainDefinition.EffectDuration ? EContactResult::Pending : EContactResult::Invalid;
    }
    else
    {
        if (FVector::DistSquared(Runtime.SegmentSourcePosition, Capsule->GetComponentLocation()) > FMath::Square(static_cast<double>(ChainDefinition.Chain.JumpDistance))) return EContactResult::Invalid;
        if (!IsUnblockedContact(Runtime.SegmentSourcePosition, Capsule, OutContact)) return EContactResult::Invalid;
    }
    return EContactResult::Ready;
}

int32 ACombatChainEffectActor::FindNextTarget(FVector Origin) const
{
    int32 ClosestUnitId = INDEX_NONE;
    double ClosestDistance = UE_DOUBLE_BIG_NUMBER;
    for (const TPair<int32, TWeakObjectPtr<AUnitBase>>& Entry : RegisteredUnits)
    {
        if (Runtime.HitUnitIds.Contains(Entry.Key)) continue;
        UCapsuleComponent* Capsule = nullptr;
        if (!IsEligibleTarget(Entry.Value.Get(), Capsule)) continue;
        const double Distance = FVector::DistSquared(Origin, Capsule->GetComponentLocation());
        if (!FMath::IsFinite(Distance) || Distance > FMath::Square(static_cast<double>(ChainDefinition.Chain.JumpDistance))) continue;
        if (ClosestUnitId != INDEX_NONE && (Distance > ClosestDistance || (Distance == ClosestDistance && Entry.Key >= ClosestUnitId))) continue;
        FVector Contact;
        if (!IsUnblockedContact(Origin, Capsule, Contact)) continue;
        ClosestUnitId = Entry.Key;
        ClosestDistance = Distance;
    }
    return ClosestUnitId;
}

void ACombatChainEffectActor::BeginSegment(int32 PreviousUnitId, int32 TargetUnitId, FVector SourcePosition, double PresentationTime)
{
    if (HasResolved() || IsActorBeingDestroyed() || Segments.Num() >= ChainDefinition.Chain.MaxTargets) return;
    AUnitBase* Target = ResolveRegisteredUnit(TargetUnitId);
    UCapsuleComponent* Capsule = nullptr;
    if (SourcePosition.ContainsNaN() || !IsEligibleTarget(Target, Capsule))
    {
        ResolveChain();
        return;
    }
    Runtime.PreviousUnitId = PreviousUnitId;
    Runtime.TargetUnitId = TargetUnitId;
    Runtime.SegmentSourcePosition = SourcePosition;
    Runtime.SegmentStartedElapsed = Runtime.ElapsedSeconds;
    Runtime.bContactWindowStarted = false;
    Runtime.NextHopTime = Runtime.ElapsedSeconds + (Runtime.HopIndex == 0 ? ChainDefinition.EffectHitDelaySeconds : ChainDefinition.Chain.JumpIntervalSeconds);
    SegmentPresentationStartedAt = FMath::IsFinite(PresentationTime) && PresentationTime >= 0.0 ? PresentationTime : -1.0;
    FCombatChainSegment& Segment = Segments.AddDefaulted_GetRef();
    Segment.Sequence = Runtime.HopIndex;
    Segment.SourceUnitId = PreviousUnitId;
    Segment.TargetUnitId = TargetUnitId;
    Segment.SourcePosition = SourcePosition;
    Segment.TargetPosition = Capsule->GetComponentLocation();
    Segment.ServerStartedAt = GetServerTime();
    OnRep_ChainPresentation();
    ForceNetUpdate();
}

void ACombatChainEffectActor::AdvanceEffect(float DeltaSeconds, double PresentationTime)
{
    if (!HasAuthority() || !bChainInitialized || HasResolved() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) return;
    AUnitBase* Source = ResolveRegisteredUnit(Runtime.SourceUnitId);
    if (!IsValid(Source) || !Source->IsUnitAlive() || !GetWorld())
    {
        ResolveChain();
        return;
    }
    double StepSeconds = DeltaSeconds;
    const bool bHasPresentationClock = SegmentPresentationStartedAt >= 0.0 && FMath::IsFinite(PresentationTime) && PresentationTime >= 0.0;
    // A newly launched segment starts its own presentation budget, so old simulation debt cannot skip its lead-in.
    // 새 구간은 자체 표현 시간 예산을 시작하여 이전 시뮬레이션 누적 시간이 준비 구간을 건너뛰지 못하게 합니다.
    if (bHasPresentationClock) StepSeconds = FMath::Min(StepSeconds, FMath::Max(0.0, PresentationTime - SegmentPresentationStartedAt - (Runtime.ElapsedSeconds - Runtime.SegmentStartedElapsed)));
    if (StepSeconds <= 0.0 && bHasPresentationClock && Runtime.bContactWindowStarted) return;
    Runtime.ElapsedSeconds += StepSeconds;
    if (Runtime.ElapsedSeconds + UE_DOUBLE_SMALL_NUMBER < Runtime.NextHopTime) return;
    Runtime.bContactWindowStarted = true;
    AUnitBase* Target = nullptr;
    FVector Contact;
    const EContactResult ContactResult = CheckCurrentContact(Target, Contact);
    if (ContactResult == EContactResult::Pending) return;
    if (ContactResult == EContactResult::Invalid)
    {
        ResolveChain();
        return;
    }
    const int32 HitUnitId = Runtime.TargetUnitId;
    Runtime.LastHitPosition = Target->GetCapsuleComponent()->GetComponentLocation();
    Runtime.HitUnitIds.Add(HitUnitId);
    if (Segments.IsValidIndex(Runtime.HopIndex)) Segments[Runtime.HopIndex].TargetPosition = Runtime.LastHitPosition;
    FVector Direction = Runtime.LastHitPosition - Runtime.SegmentSourcePosition;
    Direction.Z = 0.f;
    const FQuat Rotation = Direction.IsNearlyZero() ? Runtime.InitialRotation : Direction.Rotation().Quaternion();
    const float Power = ChainDefinition.Power * FMath::Pow(ChainDefinition.Chain.DamageMultiplierPerJump, static_cast<float>(Runtime.HopIndex));
    // Record contact before callbacks because applying GAS effects may kill units or reenter battle cleanup.
    // GAS 효과 적용으로 유닛이 사망하거나 전투 정리에 재진입할 수 있으므로 콜백 전에 접촉을 기록합니다.
    if (!ChainDefinition.ImpactVfx.Niagara.IsNull() || !ChainDefinition.ImpactVfx.Cascade.IsNull() || !ChainDefinition.ImpactVfx.Sound.IsNull()) MulticastChainImpact(ChainDefinition.ImpactVfx, FTransform(Rotation, Contact));
    OnImpact.Broadcast(Source, Target, Power);
    if (HasResolved() || IsActorBeingDestroyed()) return;
    Source = ResolveRegisteredUnit(Runtime.SourceUnitId);
    if (!IsValid(Source) || !Source->IsUnitAlive())
    {
        ResolveChain();
        return;
    }
    ++Runtime.HopIndex;
    if (Runtime.HopIndex >= ChainDefinition.Chain.MaxTargets)
    {
        ForceNetUpdate();
        ResolveChain();
        return;
    }
    FVector NextOrigin = Runtime.LastHitPosition;
    Target = ResolveRegisteredUnit(HitUnitId);
    if (IsValid(Target) && Target->IsUnitAlive() && IsValid(Target->GetCapsuleComponent())) NextOrigin = Target->GetCapsuleComponent()->GetComponentLocation();
    const int32 NextTargetId = FindNextTarget(NextOrigin);
    if (NextTargetId == INDEX_NONE)
    {
        ForceNetUpdate();
        ResolveChain();
        return;
    }
    BeginSegment(HitUnitId, NextTargetId, NextOrigin, PresentationTime);
}

void ACombatChainEffectActor::ResolveChain()
{
    if (!HasAuthority() || HasResolved()) return;
    ResolveEffect(false);
    if (IsActorBeingDestroyed()) return;
    SetLifeSpan(NaturalPresentationLimit(ChainPresentation.Vfx));
    SetActorTickEnabled(true);
}

double ACombatChainEffectActor::GetServerTime() const
{
    const UWorld* World = GetWorld();
    if (!World) return 0.0;
    const AGameStateBase* State = World->GetGameState();
    return IsValid(State) ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void ACombatChainEffectActor::OnRep_ChainPresentation()
{
    if (IsActorBeingDestroyed() || !ChainPresentation.bReady || ChainPresentation.MaxTargets < 1 || ChainPresentation.MaxTargets > 32 || Segments.Num() > ChainPresentation.MaxTargets || !FMath::IsFinite(ChainPresentation.VisualLifetime) || ChainPresentation.VisualLifetime <= 0.f || ChainPresentation.EffectOffset.ContainsNaN()) return;
    UWorld* World = GetWorld();
    if (!World || GetNetMode() == NM_DedicatedServer) return;
    SetActorTickEnabled(true);
    const FCombatChainSegment* LatestSegment = nullptr;
    for (const FCombatChainSegment& Segment : Segments)
    {
        if (Segment.Sequence < 0 || Segment.Sequence >= ChainPresentation.MaxTargets || Segment.SourcePosition.ContainsNaN() || Segment.TargetPosition.ContainsNaN() || !FMath::IsFinite(Segment.ServerStartedAt)) continue;
        if (!LatestSegment || Segment.Sequence > LatestSegment->Sequence) LatestSegment = &Segment;
    }
    if (!LatestSegment || LatestSegment->Sequence <= LatestPresentedSequence) return;
    // Remove old particles before launching the next connection, including when replication skips several hops.
    // 복제로 여러 점프를 건너뛴 경우에도 다음 연결을 표시하기 전에 이전 파티클을 제거합니다.
    for (FCombatChainSegmentPresentation& Presentation : Presentations) CombatSkillPresentation::Destroy(Presentation.Components);
    const FCombatChainSegment& Segment = *LatestSegment;
    if (GetServerTime() >= Segment.ServerStartedAt + ChainPresentation.VisualLifetime)
    {
        LatestPresentedSequence = Segment.Sequence;
        return;
    }
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    FVector Direction = Segment.TargetPosition - Segment.SourcePosition;
    Direction.Z = 0.f;
    const FQuat Rotation = Direction.IsNearlyZero() ? GetActorQuat() : Direction.Rotation().Quaternion();
    const FVector Origin = Segment.SourcePosition + Rotation.RotateVector(ChainPresentation.EffectOffset);
    AActor* Holder = World->SpawnActor<AActor>(Origin, Rotation.Rotator(), Params);
    if (!Holder) return;
    Holder->SetReplicates(false);
    Holder->SetActorEnableCollision(false);
    Holder->SetActorTickEnabled(false);
    USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("ChainSegmentOrigin"));
    Holder->SetRootComponent(Root);
    Root->RegisterComponent();
    Holder->SetActorLocationAndRotation(Origin, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    FCombatChainSegmentPresentation& Presentation = Presentations.AddDefaulted_GetRef();
    Presentation.Holder = Holder;
    Presentation.Sequence = Segment.Sequence;
    FCombatSkillVfx SegmentVisual = ChainPresentation.Vfx;
    // Play main audio once on the first observed connection, even when replication skips earlier segments.
    // 이전 구간을 건너뛰어 수신하더라도 처음 표시하는 연결에서 주 사운드를 한 번 재생합니다.
    if (LatestPresentedSequence != INDEX_NONE)
    {
        if (bool* AudioOn = SegmentVisual.BoolParameters.Find(TEXT("User.AudioOn"))) *AudioOn = false;
        SegmentVisual.Sound.Reset();
    }
    const CombatSkillPresentation::FEndpointParameters Endpoints{Segment.SourcePosition, Segment.TargetPosition};
    CombatSkillPresentation::Attach(Holder, SegmentVisual, Presentation.Components, Presentation.Audio, Endpoints, true);
    LatestPresentedSequence = Segment.Sequence;
}

void ACombatChainEffectActor::UpdatePresentations()
{
    const double ServerTime = GetServerTime();
    for (int32 Index = Presentations.Num() - 1; Index >= 0; --Index)
    {
        FCombatChainSegmentPresentation& Presentation = Presentations[Index];
        const FCombatChainSegment* Segment = Segments.FindByPredicate([&Presentation](const FCombatChainSegment& Candidate) { return Candidate.Sequence == Presentation.Sequence; });
        const bool bNaturallyComplete = !CombatSkillPresentation::HasActiveAudio(Presentation.Audio) && !Presentation.Components.ContainsByPredicate([](const UFXSystemComponent* Component) { return IsValid(Component); });
        if (!Segment || !IsValid(Presentation.Holder) || bNaturallyComplete || ServerTime >= Segment->ServerStartedAt + ChainPresentation.VisualLifetime)
        {
            DestroyPresentation(Presentation);
            Presentations.RemoveAt(Index);
            continue;
        }
        FVector TargetPosition = Segment->TargetPosition;
        AUnitBase* Target = ResolveVisualUnit(Segment->TargetUnitId);
        if (IsValid(Target) && Target->IsUnitAlive() && IsValid(Target->GetCapsuleComponent())) TargetPosition = Target->GetCapsuleComponent()->GetComponentLocation();
        // Keep the holder and source fixed so active Niagara instances retain their original LWC tile.
        // 활성 Niagara 인스턴스가 원래 LWC 타일을 유지하도록 보관 액터와 시작점을 고정합니다.
        const CombatSkillPresentation::FEndpointParameters Endpoints{Segment->SourcePosition, TargetPosition};
        CombatSkillPresentation::UpdateEndpoints(Presentation.Components, ChainPresentation.Vfx, Endpoints);
    }
}

void ACombatChainEffectActor::DestroyPresentation(FCombatChainSegmentPresentation& Presentation)
{
    CombatSkillPresentation::Destroy(Presentation.Components);
    CombatSkillPresentation::StopAudio(Presentation.Audio);
    if (IsValid(Presentation.Holder)) Presentation.Holder->Destroy();
    Presentation.Holder = nullptr;
}

void ACombatChainEffectActor::HandleChainOwnerDestroyed(AActor* DestroyedActor)
{
    Destroy();
}

void ACombatChainEffectActor::MulticastChainImpact_Implementation(const FCombatSkillVfx& ImpactVisual, const FTransform& Transform)
{
    CombatSkillPresentation::Impact(GetWorld(), ImpactVisual, Transform);
}

void ACombatChainEffectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(GetOwner())) GetOwner()->OnDestroyed.RemoveDynamic(this, &ACombatChainEffectActor::HandleChainOwnerDestroyed);
    for (FCombatChainSegmentPresentation& Presentation : Presentations) DestroyPresentation(Presentation);
    Presentations.Reset();
    RegisteredUnits.Reset();
    Super::EndPlay(EndPlayReason);
}
