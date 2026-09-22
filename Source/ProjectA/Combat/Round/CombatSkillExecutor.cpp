#include "Combat/Round/CombatSkillExecutor.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Combat/Library/CombatCollisionPolicy.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/Library/CombatWeaponTraceLibrary.h"
#include "Combat/Round/CombatRoundProjectile.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"

namespace
{
    AUnitBase* FindMeleeCollision(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill)
    {
        const float Radius = FMath::Min(Skill.MeleeRadius, Skill.HitRange * 0.5f);
        const FVector Origin = Source->GetCapsuleComponent()->GetComponentLocation();
        const FVector Forward = Source->GetActorForwardVector();
        const FVector Start = Origin + Forward * Radius;
        const FVector End = Origin + Forward * (Skill.HitRange - Radius);
        const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
        const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, Source);
        const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
        if (World->OverlapBlockingTestByChannel(Start, FQuat::Identity, ECC_WorldDynamic, Shape, Params, Responses)) return nullptr;
        FHitResult WallHit;
        const bool bHitWall = World->SweepSingleByChannel(WallHit, Start, End, FQuat::Identity, ECC_WorldDynamic, Shape, Params, Responses);
        const float WallTime = WallHit.bStartPenetrating ? 0.f : WallHit.Time;
        AUnitBase* FirstUnit = nullptr;
        int32 FirstUnitId = MAX_int32;
        float FirstTime = 1.f;

        // Query registered capsules directly so pawn movement responses and cosmetic meshes cannot change a hit.
        // 등록된 캡슐을 직접 조회하여 폰 이동 응답이나 표현용 메시가 피격 판정을 바꾸지 않도록 합니다.
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = CombatCollisionPolicy::TargetCapsule(World, Source, Source->GetTeam(), Candidate.Unit);
            if (!Capsule) continue;
            FHitResult Hit;
            const bool bInitialOverlap = Capsule->OverlapComponent(Start, FQuat::Identity, Shape);
            if (!bInitialOverlap && !Capsule->SweepComponent(Hit, Start, End, FQuat::Identity, Shape)) continue;
            const float HitTime = bInitialOverlap || Hit.bStartPenetrating ? 0.f : Hit.Time;
            if (CombatCollisionPolicy::IsBlockedByWorld(bHitWall, WallTime, HitTime)) continue;
            if (!FirstUnit || CombatCollisionPolicy::IsEarlierContact(HitTime, Candidate.UnitId, FirstTime, FirstUnitId))
            {
                FirstUnit = Candidate.Unit;
                FirstUnitId = Candidate.UnitId;
                FirstTime = HitTime;
            }
        }
        return FirstUnit;
    }

    TArray<AUnitBase*> FindMeleeAreaCollisions(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill)
    {
        TArray<AUnitBase*> Hits;
        const FVector Origin = Source->GetCapsuleComponent()->GetComponentLocation();
        const FQuat Rotation = Source->GetActorQuat();
        const FVector Center = Origin + Source->GetActorForwardVector() * Skill.MeleeAreaHalfExtent.X;
        const FCollisionShape Shape = FCollisionShape::MakeBox(Skill.MeleeAreaHalfExtent);
        const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, Source);
        const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
        if (World->OverlapBlockingTestByChannel(Origin, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses)) return Hits;
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = CombatCollisionPolicy::TargetCapsule(World, Source, Source->GetTeam(), Candidate.Unit);
            if (!Capsule || !Capsule->OverlapComponent(Center, Rotation, Shape)) continue;
            FVector Contact;
            if (Capsule->GetClosestPointOnCollision(Origin, Contact) < 0.f) continue;
            if (World->LineTraceTestByChannel(Origin, Contact, ECC_WorldDynamic, Params, Responses)) continue;
            Hits.AddUnique(Candidate.Unit);
        }
        return Hits;
    }

    TArray<AUnitBase*> FindMeleeSideCollisions(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill, ACombatGridManager* Grid, FIntPoint TargetCoord, FVector Center)
    {
        TArray<AUnitBase*> Hits;
        ACombatGridTile* TargetTile = IsValid(Grid) ? Grid->GetTileAtCoord(TargetCoord) : nullptr;
        if (!IsValid(TargetTile) || FVector::Dist2D(Source->GetActorLocation(), Center) > Skill.HitRange) return Hits;
        FVector Start = Center;
        FVector End = Center;
        if (ACombatGridTile* Left = Grid->GetTileAtCoord(TargetCoord + FIntPoint(-1, 0))) Start += Left->GetActorLocation() - TargetTile->GetActorLocation();
        if (ACombatGridTile* Right = Grid->GetTileAtCoord(TargetCoord + FIntPoint(1, 0))) End += Right->GetActorLocation() - TargetTile->GetActorLocation();
        const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Min(Skill.MeleeRadius, Skill.HitRange * 0.5f));
        const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, Source);
        const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
        if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses)) return Hits;
        if (World->LineTraceTestByChannel(Source->GetCapsuleComponent()->GetComponentLocation(), Center, ECC_WorldDynamic, Params, Responses)) return Hits;
        // Tiles size the lateral sweep; only current capsule contacts receive damage, once per enemy.
        // 타일은 횡방향 스윕 길이만 정하며 현재 캡슐이 충돌한 적에게만 한 번씩 피해를 줍니다.
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = CombatCollisionPolicy::TargetCapsule(World, Source, Source->GetTeam(), Candidate.Unit);
            if (!Capsule) continue;
            FHitResult Hit;
            if (!Capsule->OverlapComponent(Start, FQuat::Identity, Shape) && !Capsule->SweepComponent(Hit, Start, End, FQuat::Identity, Shape)) continue;
            FVector Contact;
            if (Capsule->GetClosestPointOnCollision(Center, Contact) < 0.f) continue;
            if (World->LineTraceTestByChannel(Center, Contact, ECC_WorldDynamic, Params, Responses)) continue;
            Hits.AddUnique(Candidate.Unit);
        }
        return Hits;
    }

    TArray<AUnitBase*> FindGroundCollisions(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, FVector Center, float Radius)
    {
        TArray<AUnitBase*> Hits;
        const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
        const FCollisionQueryParams Params = CombatCollisionPolicy::WorldQuery(World, Source);
        const FCollisionResponseParams Responses = CombatCollisionPolicy::WorldResponses();
        if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses)) return Hits;
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = CombatCollisionPolicy::TargetCapsule(World, Source, Source->GetTeam(), Candidate.Unit);
            if (!Capsule || !Capsule->OverlapComponent(Center, FQuat::Identity, Shape)) continue;
            FVector Contact;
            if (Capsule->GetClosestPointOnCollision(Center, Contact) < 0.f) continue;
            // Occlude the real capsule contact, not its actor center, so exposed capsule edges remain hittable.
            // 실제 캡슐 접촉점까지 차폐를 검사하여 노출된 캡슐 가장자리는 피격될 수 있게 합니다.
            if (World->LineTraceTestByChannel(Center, Contact, ECC_WorldDynamic, Params, Responses)) continue;
            Hits.AddUnique(Candidate.Unit);
        }
        return Hits;
    }
}

bool CombatSkillExecution::CanUseSkill(const AUnitBase* Source, const FCombatRoundSkill& Skill)
{
    if (!IsValid(Source) || !Source->IsUnitAlive()) return false;
    const UAbilitySystemComponent* ASC = Source->GetAbilitySystemComponent();
    return ASC && MatchesOwnedTags(Source, Skill.SourceTagQuery, Skill.SourceRequiredTags, Skill.SourceBlockedTags) && !ASC->AreAbilityTagsBlocked(Skill.EffectTags);
}

bool CombatSkillExecution::CanAffectTarget(const AUnitBase* Target, const FCombatRoundSkill& Skill)
{
    if (!IsValid(Target) || !Target->IsUnitAlive()) return false;
    return MatchesOwnedTags(Target, Skill.TargetTagQuery, Skill.TargetRequiredTags, Skill.TargetBlockedTags);
}

bool CombatSkillExecution::MatchesOwnedTags(const AUnitBase* Unit, const FGameplayTagQuery& Query, const FGameplayTagContainer& Required, const FGameplayTagContainer& Blocked)
{
    const UAbilitySystemComponent* ASC = IsValid(Unit) ? Unit->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return false;
    FGameplayTagContainer Tags;
    ASC->GetOwnedGameplayTags(Tags);
    return Tags.HasAll(Required) && !Tags.HasAny(Blocked) && (Query.IsEmpty() || Query.Matches(Tags));
}

bool CombatSkillExecution::ApplyEffect(AUnitBase* Source, AUnitBase* Target, const FCombatRoundSkill& Skill)
{
    if (!IsValid(Source) || !Source->HasAuthority() || !CombatCollisionPolicy::IsLivingEnemy(Source->GetWorld(), Source, Source->GetTeam(), Target) || !CanAffectTarget(Target, Skill)) return false;
    // Source requirements are checked at release, never after launch when the caster may have died.
    // 시전자가 사망할 수 있는 발사 이후가 아니라 발동 시점에 시전자 조건을 검사합니다.
    if (!Skill.EffectClass && Skill.Power <= 0.f) return true;
    const TSubclassOf<UGameplayEffect> EffectClass = Skill.EffectClass ? Skill.EffectClass : TSubclassOf<UGameplayEffect>(UGE_Damage::StaticClass());
    return UCombatEffectLibrary::ApplyTaggedEffectToUnit(Source, Target, EffectClass, Skill.Power, Skill.EffectTags);
}

CombatSkillExecution::FReleaseResult CombatSkillExecution::Release(const FReleaseContext& Context, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill, TFunctionRef<void(AUnitBase*)> OnHit, TFunctionRef<void(ACombatRoundProjectile*)> RegisterProjectile)
{
    FReleaseResult Result;
    Result.Status = FText::FromString(TEXT("공격 충돌 없음 또는 장애물에 차단됨"));
    if (!IsValid(Context.Owner) || !Context.Owner->HasAuthority() || !CanUseSkill(Context.Source, Skill))
    {
        Result.Status = FText::FromString(TEXT("스킬 발동 태그 조건 불충족"));
        return Result;
    }
    UWorld* World = Context.Owner->GetWorld();
    if (!World || Context.Source->GetWorld() != World) return Result;
    if (Skill.Kind == ECombatRoundSkillKind::Projectile)
    {
        FActorSpawnParameters Params;
        Params.Owner = Context.Owner;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACombatRoundProjectile* Projectile = World->SpawnActor<ACombatRoundProjectile>(Context.Source->GetActorLocation(), FRotator::ZeroRotator, Params);
        if (!Projectile)
        {
            Result.Status = FText::FromString(TEXT("투사체 생성 실패"));
            return Result;
        }
        RegisterProjectile(Projectile);
        TArray<AUnitBase*> AllowedTargets;
        for (const FCombatRoundUnitView& Candidate : Units) AllowedTargets.Add(Candidate.Unit);
        Projectile->SetAllowedTargets(AllowedTargets);
        Projectile->SetTargetTagConditions(Skill.TargetTagQuery, Skill.TargetRequiredTags, Skill.TargetBlockedTags);
        Projectile->InitializeProjectile(Context.Source, Context.Target, Context.AimLocation, Skill.ProjectileSpeed, Skill.Power, Skill.ProjectileRadius, Skill.ProjectileLifetime, Skill.bHoming, Skill.bTargetOnly);
        Result.bSucceeded = true;
        Result.Status = FText::FromString(TEXT("발사 완료"));
        return Result;
    }
    TArray<FCombatRoundUnitView> EligibleUnits;
    for (const FCombatRoundUnitView& Unit : Units)
    {
        if (CanAffectTarget(Unit.Unit, Skill)) EligibleUnits.Add(Unit);
    }
    TArray<AUnitBase*> Hits;
    if (Skill.Kind == ECombatRoundSkillKind::GroundAttack)
    {
        if (Skill.Approach == ECombatRoundApproach::None || FVector::Dist(Context.Source->GetActorLocation(), Context.AimLocation) <= Skill.HitRange) Hits = FindGroundCollisions(World, Context.Source, EligibleUnits, Context.AimLocation, Skill.HitRange);
    }
    else if (Skill.Kind == ECombatRoundSkillKind::Melee)
    {
        if (Skill.bUseMeleeAreaCollision) Hits = FindMeleeAreaCollisions(World, Context.Source, EligibleUnits, Skill);
        else if (Skill.MeleeArea == ESkillAreaType::TargetAndSides) Hits = FindMeleeSideCollisions(World, Context.Source, EligibleUnits, Skill, Context.Grid, Context.TargetCoord, Context.AimLocation);
        else if (AUnitBase* Hit = FindMeleeCollision(World, Context.Source, EligibleUnits, Skill)) Hits.Add(Hit);
    }
    for (AUnitBase* Hit : Hits)
    {
        if (!CanAffectTarget(Hit, Skill)) continue;
        OnHit(Hit);
        Result.bSucceeded = true;
    }
    if (Result.bSucceeded) Result.Status = FText::FromString(TEXT("타격 완료"));
    return Result;
}

CombatSkillExecution::ETraceResult CombatSkillExecution::AdvanceWeaponTrace(AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill, double Elapsed, FWeaponTraceState& State, AUnitBase*& OutHit, FText& OutStatus)
{
    OutHit = nullptr;
    if (!IsValid(Source) || !Source->HasAuthority() || !CanUseSkill(Source, Skill))
    {
        OutStatus = FText::FromString(TEXT("스킬 발동 태그 조건 불충족"));
        return ETraceResult::Invalid;
    }
    UAnimMontage* Montage = Source->ResolveRoundCastMontage(Skill.CastMontage);
    if (!IsValid(Montage) || !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.f)
    {
        OutStatus = FText::FromString(TEXT("검 공격 애니메이션 설정 누락"));
        return ETraceResult::Invalid;
    }
    const double WindowEnd = Skill.WindupSeconds + Skill.WeaponTraceDuration;
    if (Elapsed + UE_DOUBLE_SMALL_NUMBER < Skill.WindupSeconds) return ETraceResult::Pending;
    const double SampleUntil = FMath::Min(Elapsed, WindowEnd);
    const bool bFirstSample = State.SampleTime < 0.0;
    if (!bFirstSample && SampleUntil <= State.SampleTime + UE_DOUBLE_SMALL_NUMBER) return ETraceResult::Pending;
    double SampleTime = bFirstSample ? Skill.WindupSeconds : FMath::Min(State.SampleTime + 0.005, SampleUntil);
    TArray<FCombatRoundUnitView> EligibleUnits;
    for (const FCombatRoundUnitView& Unit : Units)
    {
        if (CanAffectTarget(Unit.Unit, Skill)) EligibleUnits.Add(Unit);
    }
    for (;;)
    {
        CombatWeaponTrace::FBladePose Current;
        if (!CombatWeaponTrace::SampleBlade(Source, Skill, Montage, SampleTime * Montage->RateScale, Current))
        {
            OutStatus = FText::FromString(TEXT("검 장착 또는 칼날 소켓 설정 누락"));
            return ETraceResult::Invalid;
        }
        const CombatWeaponTrace::FBladePose Previous = State.SampleTime < 0.0 ? Current : CombatWeaponTrace::FBladePose{State.PreviousBase, State.PreviousTip};
        State.SampleTime = SampleTime;
        State.PreviousBase = Current.Base;
        State.PreviousTip = Current.Tip;
        OutHit = CombatWeaponTrace::FindFirstHit(Source->GetWorld(), Source, EligibleUnits, Previous, Current, Skill.WeaponTraceRadius);
        if (OutHit)
        {
            OutStatus = FText::FromString(TEXT("검 타격 완료"));
            return ETraceResult::Hit;
        }
        if (SampleTime + UE_DOUBLE_SMALL_NUMBER >= SampleUntil) break;
        SampleTime = FMath::Min(SampleTime + 0.005, SampleUntil);
    }
    if (Elapsed + UE_DOUBLE_SMALL_NUMBER < WindowEnd) return ETraceResult::Pending;
    OutStatus = FText::FromString(TEXT("칼날 충돌 없음 또는 장애물에 차단됨"));
    return ETraceResult::Miss;
}
