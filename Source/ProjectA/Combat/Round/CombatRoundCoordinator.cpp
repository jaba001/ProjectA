#include "Combat/Round/CombatRoundCoordinator.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Unit/PlayerUnit.h"
#include "Controller/PartyPlayerController.h"
#include "EngineUtils.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/Library/CombatWeaponTraceLibrary.h"
#include "Combat/Round/CombatRoundProjectile.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Game/Encounter/CombatArena.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/Effect/GE_Damage.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Net/UnrealNetwork.h"
#include "Unit/UnitBase.h"

namespace
{
    constexpr float RoundStep = 0.01f;
    // SAP repositioning uses one fixed walking speed for every unit, independent of combat speed.
    // SAP 위치 이동은 전투 속도와 무관하게 모든 유닛에 동일한 고정 보행 속도를 사용합니다.
    constexpr float SAPMoveSpeed = 350.f;
    constexpr double MovementTimeout = 6.0;
    constexpr double MaximumMontageRecoverySeconds = 60.0;

    double MontageRecoveryBudget(const UAnimMontage* Montage)
    {
        if (!Montage) return 0.0;
        const double Length = Montage->GetPlayLength();
        const double Rate = Montage->RateScale;
        const double BlendOut = Montage->BlendOut.GetBlendTime();
        const bool bValidLength = FMath::IsFinite(Length) && Length > 0.0;
        const bool bValidRate = FMath::IsFinite(Rate) && Rate > 0.0;
        const bool bValidBlend = FMath::IsFinite(BlendOut) && BlendOut >= 0.0;
        bool bLooping = false;
        TSet<int32> VisitedSections;
        int32 SectionIndex = Montage->GetSectionIndexFromPosition(0.f);
        while (Montage->IsValidSectionIndex(SectionIndex))
        {
            if (VisitedSections.Contains(SectionIndex))
            {
                bLooping = true;
                break;
            }
            VisitedSections.Add(SectionIndex);
            SectionIndex = Montage->GetSectionIndex(Montage->CompositeSections[SectionIndex].NextSectionName);
        }
        // Use a conservative full-asset budget when animation cannot tick; actual instances finish naturally first.
        // 애니메이션을 갱신할 수 없을 때는 전체 에셋 기준의 보수적 시간을 사용하고 실제 인스턴스는 자연 종료를 우선합니다.
        const double Budget = (bValidLength ? Length : 1.0) / (bValidRate ? Rate : 1.0) + (bValidBlend ? BlendOut : 0.0) + 0.25;
        if (!bValidLength || !bValidRate || !bValidBlend || bLooping || !Montage->bEnableAutoBlendOut || Budget > MaximumMontageRecoverySeconds)
        {
            UE_LOG(LogTemp, Warning, TEXT("[RoundAnimation] Bounded montage recovery Montage=%s Length=%.3f Rate=%.3f Budget=%.3f Loop=%d AutoBlendOut=%d / 몽타주 설정의 무한 대기를 방지하기 위해 최대 60초 안에 정리합니다"), *GetPathNameSafe(Montage), Length, Rate, Budget, bLooping, Montage->bEnableAutoBlendOut);
        }
        return FMath::Clamp(Budget, 0.01, MaximumMontageRecoverySeconds);
    }

    FText RoundText(const TCHAR* Value)
    {
        return FText::FromString(Value);
    }

    FCollisionResponseParams AttackWorldResponses()
    {
        FCollisionResponseParams Responses(ECR_Ignore);
        Responses.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
        Responses.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
        return Responses;
    }

    FCollisionQueryParams AttackWorldQuery(UWorld* World, AUnitBase* Source)
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatRoundAttack), false, Source);
        Params.bFindInitialOverlaps = true;
        Params.bIgnoreTouches = true;
        for (TActorIterator<APawn> It(World); It; ++It) Params.AddIgnoredActor(*It);
        return Params;
    }

    UCapsuleComponent* AttackTargetCapsule(AUnitBase* Source, const FCombatRoundUnitView& Candidate)
    {
        AUnitBase* Unit = Candidate.Unit;
        if (!IsValid(Unit) || Unit == Source || !Unit->IsUnitAlive() || Unit->GetTeam() == Source->GetTeam() || !Unit->GetActorEnableCollision()) return nullptr;
        UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
        return IsValid(Capsule) && Capsule->IsQueryCollisionEnabled() ? Capsule : nullptr;
    }

    AUnitBase* FindMeleeCollision(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FCombatRoundSkill& Skill)
    {
        const float Radius = FMath::Min(Skill.MeleeRadius, Skill.HitRange * 0.5f);
        const FVector Origin = Source->GetCapsuleComponent()->GetComponentLocation();
        const FVector Forward = Source->GetActorForwardVector();
        const FVector Start = Origin + Forward * Radius;
        const FVector End = Origin + Forward * (Skill.HitRange - Radius);
        const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
        const FCollisionQueryParams Params = AttackWorldQuery(World, Source);
        const FCollisionResponseParams Responses = AttackWorldResponses();
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
            UCapsuleComponent* Capsule = AttackTargetCapsule(Source, Candidate);
            if (!Capsule) continue;
            FHitResult Hit;
            const bool bInitialOverlap = Capsule->OverlapComponent(Start, FQuat::Identity, Shape);
            if (!bInitialOverlap && !Capsule->SweepComponent(Hit, Start, End, FQuat::Identity, Shape)) continue;
            const float HitTime = bInitialOverlap || Hit.bStartPenetrating ? 0.f : Hit.Time;
            if (bHitWall && WallTime <= HitTime) continue;
            if (!FirstUnit || HitTime < FirstTime || (HitTime == FirstTime && Candidate.UnitId < FirstUnitId))
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
        const FCollisionQueryParams Params = AttackWorldQuery(World, Source);
        const FCollisionResponseParams Responses = AttackWorldResponses();
        if (World->OverlapBlockingTestByChannel(Origin, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses)) return Hits;
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = AttackTargetCapsule(Source, Candidate);
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
        const FCollisionQueryParams Params = AttackWorldQuery(World, Source);
        const FCollisionResponseParams Responses = AttackWorldResponses();
        if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses)) return Hits;
        if (World->LineTraceTestByChannel(Source->GetCapsuleComponent()->GetComponentLocation(), Center, ECC_WorldDynamic, Params, Responses)) return Hits;
        // Tiles size the lateral sweep; only current capsule contacts receive damage, once per enemy.
        // 타일은 횡방향 스윕 길이만 정하며 현재 캡슐이 충돌한 적에게만 한 번씩 피해를 줍니다.
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = AttackTargetCapsule(Source, Candidate);
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
        const FCollisionQueryParams Params = AttackWorldQuery(World, Source);
        const FCollisionResponseParams Responses = AttackWorldResponses();
        if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(0.1f), Params, Responses)) return Hits;
        for (const FCombatRoundUnitView& Candidate : Units)
        {
            UCapsuleComponent* Capsule = AttackTargetCapsule(Source, Candidate);
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

ACombatRoundCoordinator::ACombatRoundCoordinator()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(20.f);
}

void ACombatRoundCoordinator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatRoundCoordinator, View);
    DOREPLIFETIME(ACombatRoundCoordinator, Skills);
    DOREPLIFETIME(ACombatRoundCoordinator, bSAPMovementInProgress);
    DOREPLIFETIME(ACombatRoundCoordinator, Arena);
}

const FCombatRoundSkill* ACombatRoundCoordinator::FindSkill(FName SkillId) const
{
    return Skills.FindByPredicate([SkillId](const FCombatRoundSkill& Skill) { return Skill.SkillId == SkillId; });
}

const FCombatRoundSkill* ACombatRoundCoordinator::FindCommandSkill(const FCombatRoundCommand& Command) const
{
    const int32 Index = FindUnitIndex(Command.UnitId);
    if (Command.SkillId.IsNone() && View.Units.IsValidIndex(Index))
    {
        // An empty command skips the action without granting a synthetic skill or charging action costs.
        // 빈 명령은 가상 스킬을 부여하거나 행동 비용을 차감하지 않고 행동을 건너뜁니다.
        static const FCombatRoundSkill Idle = []()
        {
            FCombatRoundSkill Skill;
            Skill.Kind = ECombatRoundSkillKind::Wait;
            Skill.Approach = ECombatRoundApproach::None;
            Skill.ActionPointCost = 0;
            Skill.SubActionPointCost = 0;
            Skill.Power = 0.f;
            return Skill;
        }();
        return &Idle;
    }
    return FindSkill(Command.SkillId);
}

int32 ACombatRoundCoordinator::FindUnitIndex(int32 UnitId) const
{
    return View.Units.IndexOfByPredicate([UnitId](const FCombatRoundUnitView& Entry) { return Entry.UnitId == UnitId; });
}

bool ACombatRoundCoordinator::InitializeFromCombat(ACombatManager* InManager, FText& OutError)
{
    if (!HasAuthority() || !IsValid(InManager) || InManager->GetWorld() != GetWorld() || View.CombatId.IsValid())
    {
        OutError = RoundText(TEXT("전투 실행 문맥이 올바르지 않습니다."));
        return false;
    }
    CombatManager = InManager;
    for (TActorIterator<ACombatArena> It(GetWorld()); It; ++It)
    {
        if (It->Grid == InManager->GetCombatGrid() && It->Grid && !It->Grid->TileMap.IsEmpty())
        {
            Arena = *It;
            break;
        }
    }
    if (!Arena || !CombatManager->GetActionAuthority())
    {
        OutError = RoundText(TEXT("전투장 또는 참가자 권위가 없습니다."));
        return false;
    }
    Skills.Reset();
    View.CombatId = InManager->GetCombatInstanceId();
    URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const bool bRestoring = Run && Run->GetPhase() == ERunPhase::Combat && Run->HasCombatCheckpoint();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APartyPlayerController* Controller = Cast<APartyPlayerController>(It->Get());
        if (!Controller) continue;
        Controller->SetCombatContext(InManager, false);
        bool bOwnsCharacter = false;
        for (AUnitBase* Unit : InManager->GetRegisteredUnits())
        {
            if (CombatManager->GetActionAuthority()->CanControllerControl(Controller, Unit)) bOwnsCharacter = true;
        }
        if (bOwnsCharacter) Participants.Add(Controller);
    }
    for (AUnitBase* Unit : InManager->GetRegisteredUnits())
    {
        if (!IsValid(Unit) || !Unit->GetAttributeSet() || (!Unit->IsUnitAlive() && !bRestoring) || (Unit->IsUnitAlive() && !Unit->GetCurrentTile()))
        {
            OutError = RoundText(TEXT("유닛의 시작 위치 또는 생존 상태가 올바르지 않습니다."));
            return false;
        }
        if (Unit->IsUnitAlive() && (Arena->Grid->GetTileAtCoord(Unit->GetCurrentTile()->GridCoord) != Unit->GetCurrentTile() || Unit->GetCurrentTile()->GetOccupyingUnit() != Unit || !CombatRoundRules::IsOwnTerritory(Unit->GetTeam() == ETeam::Enemy, Unit->GetCurrentTile()->GridCoord)))
        {
            OutError = RoundText(TEXT("유닛의 시작 칸과 전투 Grid·진영·점유가 일치하지 않습니다."));
            return false;
        }
        Unit->OnTurnEnd();
        Unit->GetCharacterMovement()->StopMovementImmediately();
        Unit->GetCharacterMovement()->DisableMovement();
        Unit->GetCharacterMovement()->SetComponentTickEnabled(false);
        Unit->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Unit->GetCapsuleComponent()->SetCollisionEnabled(Unit->IsUnitAlive() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        FCombatRoundUnitView Entry;
        Entry.UnitId = View.Units.Num() + 1;
        Entry.Unit = Unit;
        Entry.bEnemy = Unit->GetTeam() == ETeam::Enemy;
        Entry.HomeCoord = Unit->GetCurrentTile() ? Unit->GetCurrentTile()->GridCoord : FIntPoint::ZeroValue;
        Entry.Speed = Unit->GetCombatSpeed();
        Entry.HP = Unit->GetAttributeSet()->GetHP();
        for (USkillDefinitionDataAsset* Definition : Unit->GetEquippedSkillDataAssets())
        {
            if (!IsValid(Definition)) continue;
            FCombatRoundSkill Skill;
            if (!Definition->ResolveRoundSkill(Skill, OutError)) return false;
            if (!FindSkill(Skill.SkillId)) Skills.Add(Skill);
            Entry.SkillIds.AddUnique(Skill.SkillId);
        }
        Unit->UnitIndex = Entry.UnitId;
        for (int32 Index = 0; Index < Participants.Num(); ++Index)
        {
            if (CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Participants[Index]), Unit))
            {
                Entry.OwnerSlot = Index + 1;
                break;
            }
        }
        if (Unit->IsUnitAlive() && !Entry.bEnemy && !InManager->IsPartyAIControlled(Unit) && Entry.OwnerSlot == 0)
        {
            OutError = RoundText(TEXT("인간 캐릭터의 원래 소유 연결을 확인하지 못했습니다."));
            return false;
        }
        View.Units.Add(Entry);
    }
    if (View.Units.Num() < 2 || View.Units.Num() > 8)
    {
        OutError = RoundText(TEXT("전투 유닛은 양 진영 합계 2~8명이어야 합니다."));
        return false;
    }
    const bool bHasPlayer = View.Units.ContainsByPredicate([](const FCombatRoundUnitView& Entry) { return !Entry.bEnemy; });
    const bool bHasEnemy = View.Units.ContainsByPredicate([](const FCombatRoundUnitView& Entry) { return Entry.bEnemy; });
    if (!bHasPlayer || !bHasEnemy)
    {
        OutError = RoundText(TEXT("양 진영에 생존 유닛이 있어야 전투를 시작할 수 있습니다."));
        return false;
    }
    for (const FCombatRoundSkill& Skill : Skills)
    {
        if (!CombatRoundRules::IsValidSkill(Skill))
        {
            OutError = FText::FromString(FString::Printf(TEXT("스킬 실행 수치가 올바르지 않습니다: %s"), *Skill.SkillId.ToString()));
            return false;
        }
    }
    Actions.SetNum(View.Units.Num());
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        if (APartyPlayerController* Controller = Cast<APartyPlayerController>(Participants[Index])) Controller->SetRoundSession(this, Index + 1);
    }
    if (bRestoring) return RestorePlanningCheckpoint(Run->GetCombatCheckpoint(), OutError);
    BeginPlanning();
    return true;
}

bool ACombatRoundCoordinator::IsRoundSessionActive() const
{
    return View.Phase == ECombatRoundPhase::Planning || View.Phase == ECombatRoundPhase::Resolving;
}

void ACombatRoundCoordinator::SuspendRound()
{
    if (!HasAuthority()) return;
    FinishPlanningMove(false);
    bSAPMovementInProgress = false;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (IsValid(Entry.Unit))
        {
            Entry.Unit->SetRoundCastMontage(nullptr);
            Entry.Unit->SetRoundMovementVelocity(FVector::ZeroVector);
            Entry.Unit->ForceNetUpdate();
        }
    }
    View.Phase = ECombatRoundPhase::Suspended;
    View.Message = RoundText(TEXT("전투가 중단되었습니다. 재개 시 마지막으로 저장한 준비 상태에서 복구합니다."));
    PublishState();
}

void ACombatRoundCoordinator::StopRound()
{
    if (!HasAuthority()) return;
    View.Phase = ECombatRoundPhase::Finished;
    CleanupUnits();
    PublishState();
}

int32 ACombatRoundCoordinator::GetParticipantSlot(const APlayerController* Controller) const
{
    if (!Controller) return 0;
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        if (Participants[Index] == Controller) return Index + 1;
    }
    return 0;
}

void ACombatRoundCoordinator::CleanupUnits()
{
    TGuardValue<bool> CleanupGuard(bCleaningUp, true);
    FinishPlanningMove(false);
    bSAPMovementInProgress = false;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (IsValid(Entry.Unit))
        {
            Entry.Unit->SetRoundCastMontage(nullptr);
            Entry.Unit->SetRoundMovementVelocity(FVector::ZeroVector);
            Entry.Unit->ForceNetUpdate();
        }
    }
    for (ACombatRoundProjectile* Projectile : Projectiles)
    {
        if (IsValid(Projectile))
        {
            Projectile->OnImpact.RemoveAll(this);
            Projectile->OnResolved.RemoveAll(this);
            Projectile->Destroy();
        }
    }
    Projectiles.Reset();
    View.Units.Reset();
    Actions.Reset();
}

void ACombatRoundCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority()) CleanupUnits();
    Super::EndPlay(EndPlayReason);
}

void ACombatRoundCoordinator::PublishState()
{
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        Entry.HP = 0.f;
        if (IsValid(Entry.Unit) && Entry.Unit->GetAttributeSet()) Entry.HP = Entry.Unit->GetAttributeSet()->GetHP();
    }
    View.ElapsedSeconds = static_cast<float>(SimulationTime);
    View.PendingProjectiles = Projectiles.Num();
    ForceNetUpdate();
    OnRoundStateChanged.Broadcast();
}

int32 ACombatRoundCoordinator::FindNearestEnemy(int32 SourceIndex) const
{
    if (!View.Units.IsValidIndex(SourceIndex) || !IsValid(View.Units[SourceIndex].Unit)) return INDEX_NONE;
    const FCombatRoundUnitView& Source = View.Units[SourceIndex];
    int32 Best = INDEX_NONE;
    double Distance = TNumericLimits<double>::Max();
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const FCombatRoundUnitView& Candidate = View.Units[Index];
        if (Candidate.bEnemy == Source.bEnemy || !IsValid(Candidate.Unit) || !Candidate.Unit->IsUnitAlive()) continue;
        const double CandidateDistance = FVector::DistSquared2D(Source.Unit->GetActorLocation(), Candidate.Unit->GetActorLocation());
        if (CandidateDistance < Distance)
        {
            Distance = CandidateDistance;
            Best = Index;
        }
    }
    return Best;
}

void ACombatRoundCoordinator::BeginPlanning()
{
    ++View.RoundNumber;
    ++View.PlanRevision;
    View.Phase = ECombatRoundPhase::Planning;
    SimulationTime = 0.0;
    Accumulator = 0.0;
    bLockRetryBlocked = false;
    float HighestSpeed = 0.0f;
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        Entry.HP = 0.f;
        if (IsValid(Entry.Unit) && Entry.Unit->GetAttributeSet()) Entry.HP = Entry.Unit->GetAttributeSet()->GetHP();
        if (IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive())
        {
            Entry.Speed = Entry.Unit->GetCombatSpeed();
            HighestSpeed = FMath::Max(HighestSpeed, Entry.Speed);
            Entry.Unit->ResetActionPoint();
            Entry.Unit->ResetSubActionPoint();
        }
    }
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        FCombatRoundUnitView& Entry = View.Units[Index];
        Entry.StartDelay = CombatRoundRules::StartDelay(HighestSpeed, Entry.Speed);
        Entry.Command = FCombatRoundCommand();
        Entry.Command.UnitId = Entry.UnitId;
        Entry.Command.DestinationCoord = Entry.HomeCoord;
        Entry.Command.TargetCoord = Entry.HomeCoord;
        Entry.bHasMovePlan = false;
        Entry.MoveDestinationCoord = Entry.HomeCoord;
        Entry.bReady = false;
        Entry.Status = RoundText(TEXT("스킬 미선택 · 턴 넘기기"));
        Entry.ActionPhase = ECombatRoundActionPhase::Planned;
        Actions[Index] = FActionRuntime();
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive())
        {
            Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
            Entry.Status = RoundText(TEXT("사망"));
            Entry.bReady = true;
            continue;
        }
        Actions[Index].OriginalLocation = Entry.Unit->GetActorLocation();
    }
    // Reset every command before choosing AI actions from the new round state.
    // 새 라운드 상태로 AI 행동을 선택하기 전에 모든 명령을 초기화합니다.
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        FCombatRoundUnitView& Entry = View.Units[Index];
        if (Entry.OwnerSlot != 0 || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;

        // AI commands are fixed before any human draft can be submitted.
        // 인간 초안이 제출되기 전에 AI 명령을 고정합니다.
        const int32 TargetIndex = FindNearestEnemy(Index);
        Entry.Command.SkillId = NAME_None;
        if (View.Units.IsValidIndex(TargetIndex))
        {
            Entry.Command.TargetUnitId = View.Units[TargetIndex].UnitId;
            Entry.Command.TargetCoord = View.Units[TargetIndex].HomeCoord;
            for (FName SkillId : Entry.SkillIds)
            {
                const FCombatRoundSkill* Candidate = FindSkill(SkillId);
                if (!Candidate || Candidate->Kind == ECombatRoundSkillKind::Wait || Candidate->bRemainAtDestination) continue;
                Entry.Command.SkillId = SkillId;
                Entry.Command.DestinationCoord = Entry.HomeCoord;
                if (Candidate->Approach == ECombatRoundApproach::Tile)
                {
                    int32 BestDistance = MAX_int32;
                    for (int32 X = 0; X < 4; ++X)
                    {
                        for (int32 Y = 0; Y < 4; ++Y)
                        {
                            const FIntPoint Coord(X, Y);
                            ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Coord);
                            if (!Tile || (Tile->GetOccupyingUnit() && Tile->GetOccupyingUnit() != Entry.Unit) || IsDestinationReservedByOther(Index, Coord)) continue;
                            const FIntPoint Offset = Coord - View.Units[TargetIndex].HomeCoord;
                            const int32 Distance = FMath::Abs(Offset.X) + FMath::Abs(Offset.Y);
                            if (Distance < BestDistance)
                            {
                                BestDistance = Distance;
                                Entry.Command.DestinationCoord = Coord;
                            }
                        }
                    }
                }
                FText Error;
                if (ValidateCommand(Entry.Command, Error)) break;
                Entry.Command.SkillId = NAME_None;
                Entry.Command.DestinationCoord = Entry.HomeCoord;
            }
        }
        Entry.bReady = true;
        Entry.Status = RoundText(TEXT("AI 계획 고정"));
    }
    View.Message = RoundText(TEXT("준비 완료를 누르면 스킬을 선택하지 않은 유닛은 행동을 건너뜁니다. 적 계획은 이미 고정되었습니다."));
    FText SaveError;
    if (!PersistPlanningCheckpoint(SaveError)) View.Message = SaveError;
    PublishState();
}

bool ACombatRoundCoordinator::PersistPlanningCheckpoint(FText& OutError) const
{
    URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    // Asset previews and isolated development rooms have no persistent Run to confirm.
    // 에셋 미리보기와 독립 개발 방에는 확정할 영속 Run이 없습니다.
    if (!Run || Run->GetPhase() == ERunPhase::None)
    {
        OutError = FText::GetEmpty();
        return true;
    }
    FCombatCheckpointData Checkpoint;
    return CapturePlanningCheckpoint(Checkpoint, OutError) && Run->CommitCombatCheckpoint(Checkpoint, OutError);
}

bool ACombatRoundCoordinator::RetryPlanningCheckpoint(FText& OutError)
{
    if (!HasExecutionAuthority() || !CanRetryPlanningCheckpoint()) return false;
    if (!LockPlans(OutError))
    {
        View.Message = OutError;
        PublishState();
        return false;
    }
    bLockRetryBlocked = false;
    PublishState();
    return true;
}

bool ACombatRoundCoordinator::ValidateRequest(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, FText& OutError) const
{
    if (!HasExecutionAuthority() || !IsValid(Controller) || Controller->GetWorld() != GetWorld() || GetParticipantSlot(Controller) == 0)
    {
        OutError = RoundText(TEXT("현재 전투를 조작할 권한이 없습니다."));
        return false;
    }
    bool bOwnsCharacter = false;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), Entry.Unit)) bOwnsCharacter = true;
    }
    if (!bOwnsCharacter)
    {
        OutError = RoundText(TEXT("이 전투에 조작 가능한 원래 캐릭터가 없습니다."));
        return false;
    }
    if (View.Phase != ECombatRoundPhase::Planning || CombatId != View.CombatId || RoundNumber != View.RoundNumber || Revision != View.PlanRevision)
    {
        OutError = RoundText(TEXT("계획이 변경되었거나 이미 잠겼습니다. 최신 상태에서 다시 선택하세요."));
        return false;
    }
    if (bSAPMovementInProgress)
    {
        OutError = RoundText(TEXT("이동이 끝난 뒤 계획과 준비를 변경하세요."));
        return false;
    }
    return true;
}

bool ACombatRoundCoordinator::HasExecutionAuthority() const
{
    const UCombatActionAuthority* Authority = nullptr;
    if (IsValid(CombatManager)) Authority = CombatManager->GetActionAuthority();
    return HasAuthority() && View.CombatId.IsValid() && Authority && Authority->GetCombatInstanceId() == View.CombatId && Authority->HasManagedExecutionAuthority(false);
}

bool ACombatRoundCoordinator::CanPlanCommand(const FCombatRoundCommand& Command, FText& OutError) const
{
    OutError = FText::GetEmpty();
    if (View.Phase != ECombatRoundPhase::Planning || bSAPMovementInProgress)
    {
        OutError = RoundText(TEXT("계획 단계에서만 행동을 선택할 수 있습니다."));
        return false;
    }
    if (!ValidateCommand(Command, OutError)) return false;
    const int32 Index = FindUnitIndex(Command.UnitId);
    TArray<FIntPoint> Path;
    if (View.Units[Index].bHasMovePlan && !BuildPlanningMovePath(Command.UnitId, View.Units[Index].MoveDestinationCoord, Path, OutError)) return false;
    return ValidateDestinations(OutError, Index, &Command);
}

bool ACombatRoundCoordinator::CanMoveUnit(int32 UnitId, FIntPoint Destination, FText& OutError) const
{
    OutError = FText::GetEmpty();
    const int32 Index = FindUnitIndex(UnitId);
    if (View.Phase != ECombatRoundPhase::Planning || !View.Units.IsValidIndex(Index))
    {
        OutError = RoundText(TEXT("계획 단계에서만 SAP 이동을 예약할 수 있습니다."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    const FCombatRoundSkill* Skill = FindCommandSkill(Entry.Command);
    if (Entry.bEnemy || Entry.OwnerSlot == 0 || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive())
    {
        OutError = RoundText(TEXT("살아 있는 인간 조작 캐릭터만 이동을 예약할 수 있습니다."));
        return false;
    }
    if (!Entry.Unit->HasEnoughSubActionPoint(1 + (Skill ? Skill->SubActionPointCost : 0)))
    {
        OutError = RoundText(TEXT("이동 SAP 1과 선택한 스킬의 SAP 합계가 부족합니다."));
        return false;
    }
    TArray<FIntPoint> Path;
    return ValidateDestinations(OutError, Index, nullptr, &Destination) && BuildPlanningMovePath(UnitId, Destination, Path, OutError);
}

bool ACombatRoundCoordinator::IsDestinationReservedByOther(int32 UnitIndex, FIntPoint Coord) const
{
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const FCombatRoundUnitView& Entry = View.Units[Index];
        if (Index == UnitIndex || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        if (Entry.HomeCoord == Coord || (Entry.bHasMovePlan && Entry.MoveDestinationCoord == Coord)) return true;
        const FCombatRoundSkill* Skill = FindSkill(Entry.Command.SkillId);
        if (Skill && Skill->Approach == ECombatRoundApproach::Tile && Entry.Command.DestinationCoord == Coord) return true;
    }
    return false;
}

bool ACombatRoundCoordinator::BuildPlanningMovePath(int32 UnitId, FIntPoint Destination, TArray<FIntPoint>& OutPath, FText& OutError) const
{
    OutPath.Reset();
    const int32 Index = FindUnitIndex(UnitId);
    if (!View.Units.IsValidIndex(Index) || !IsValid(Arena) || !IsValid(Arena->Grid))
    {
        OutError = RoundText(TEXT("SAP 이동의 유닛 또는 전투장을 확인할 수 없습니다."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    AUnitBase* Unit = Entry.Unit;
    ACombatGridTile* Origin = Arena->Grid->GetTileAtCoord(Entry.HomeCoord);
    if (!IsValid(Unit) || !Unit->IsUnitAlive() || !IsValid(Origin) || Unit->GetCurrentTile() != Origin || Origin->GetOccupyingUnit() != Unit)
    {
        OutError = RoundText(TEXT("살아 있는 캐릭터의 현재 칸을 확인할 수 없습니다."));
        return false;
    }
    const ETileTerritory Territory = Entry.bEnemy ? ETileTerritory::Enemy : ETileTerritory::Player;
    ACombatGridTile* Target = Arena->Grid->GetTileAtCoord(Destination);
    if (!IsValid(Target) || Target == Origin || Target->GetOccupyingUnit() || !CombatRoundRules::IsOwnTerritory(Entry.bEnemy, Destination) || Target->GetTerritory() != Territory || IsDestinationReservedByOther(Index, Destination))
    {
        OutError = RoundText(TEXT("이동할 아군 진영의 예약되지 않은 빈칸을 선택하세요."));
        return false;
    }

    // Breadth-first search keeps occupied homes and other reservations out of every movement path.
    // 너비 우선 탐색으로 점유 중인 원점과 다른 예약 칸을 모든 이동 경로에서 제외합니다.
    TArray<FIntPoint> Queue = {Entry.HomeCoord};
    TMap<FIntPoint, FIntPoint> Previous;
    TMap<FIntPoint, int32> Distances;
    Distances.Add(Entry.HomeCoord, 0);
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const FIntPoint Current = Queue[Head];
        const int32 Distance = Distances.FindChecked(Current);
        if (Distance >= Unit->GetMoveRange()) continue;
        for (int32 DX = -1; DX <= 1; ++DX)
        {
            for (int32 DY = -1; DY <= 1; ++DY)
            {
                if (DX == 0 && DY == 0) continue;
                const FIntPoint Next = Current + FIntPoint(DX, DY);
                if (Distances.Contains(Next) || !CombatRoundRules::IsOwnTerritory(Entry.bEnemy, Next) || IsDestinationReservedByOther(Index, Next)) continue;
                ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Next);
                if (!IsValid(Tile) || Tile->GetTerritory() != Territory || Tile->GetOccupyingUnit()) continue;
                Distances.Add(Next, Distance + 1);
                Previous.Add(Next, Current);
                if (Next == Destination)
                {
                    for (FIntPoint Step = Destination; Step != Entry.HomeCoord; Step = Previous.FindChecked(Step)) OutPath.Insert(Step, 0);
                    return true;
                }
                Queue.Add(Next);
            }
        }
    }
    OutError = RoundText(TEXT("이동 범위 안에서 빈 아군 칸을 따라 도달할 수 없습니다."));
    return false;
}

bool ACombatRoundCoordinator::SubmitMove(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, int32 UnitId, FIntPoint Destination, FText& OutError)
{
    if (!ValidateRequest(Controller, CombatId, RoundNumber, Revision, OutError)) return false;
    const int32 Index = FindUnitIndex(UnitId);
    if (!View.Units.IsValidIndex(Index) || !CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), View.Units[Index].Unit))
    {
        OutError = RoundText(TEXT("원래 자신의 캐릭터만 이동을 예약할 수 있습니다."));
        return false;
    }
    if (!CanMoveUnit(UnitId, Destination, OutError)) return false;
    const FCombatRoundView PreviousView = View;
    View.Units[Index].bHasMovePlan = true;
    View.Units[Index].MoveDestinationCoord = Destination;
    View.Units[Index].Status = RoundText(TEXT("SAP 이동 예약됨"));
    ClearOwnerReady(GetParticipantSlot(Controller));
    View.Message = RoundText(TEXT("이동을 예약했습니다. 준비 완료 후 SAP 이동부터 실행합니다."));
    ++View.PlanRevision;
    if (!PersistPlanningCheckpoint(OutError))
    {
        View = PreviousView;
        return false;
    }
    bLockRetryBlocked = false;
    PublishState();
    return true;
}

bool ACombatRoundCoordinator::CancelMove(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, int32 UnitId, FText& OutError)
{
    if (!ValidateRequest(Controller, CombatId, RoundNumber, Revision, OutError)) return false;
    const int32 Index = FindUnitIndex(UnitId);
    if (!View.Units.IsValidIndex(Index) || !IsValid(View.Units[Index].Unit) || !View.Units[Index].Unit->IsUnitAlive() || !CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), View.Units[Index].Unit) || !View.Units[Index].bHasMovePlan)
    {
        OutError = RoundText(TEXT("자신의 살아 있는 캐릭터에게 예약된 이동이 없습니다."));
        return false;
    }
    const FCombatRoundView PreviousView = View;
    View.Units[Index].bHasMovePlan = false;
    View.Units[Index].MoveDestinationCoord = View.Units[Index].HomeCoord;
    View.Units[Index].Status = RoundText(TEXT("SAP 이동 예약 취소"));
    ClearOwnerReady(GetParticipantSlot(Controller));
    View.Message = RoundText(TEXT("이동 예약을 취소했습니다. 행동 계획과 준비를 다시 확인하세요."));
    ++View.PlanRevision;
    OutError = FText::GetEmpty();
    if (!PersistPlanningCheckpoint(OutError))
    {
        View = PreviousView;
        return false;
    }
    bLockRetryBlocked = false;
    PublishState();
    return true;
}

void ACombatRoundCoordinator::BeginNextMove()
{
    while (NextMoveIndex < View.Units.Num())
    {
        const int32 Index = NextMoveIndex++;
        FCombatRoundUnitView& Entry = View.Units[Index];
        if (!Entry.bHasMovePlan) continue;
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive() || Actions[Index].MovePath.IsEmpty())
        {
            bSAPMovementFailed = true;
            Entry.Status = RoundText(TEXT("SAP 이동 취소"));
            continue;
        }
        PlanningMoveIndex = Index;
        PlanningMovePath = Actions[Index].MovePath;
        PlanningMoveStep = 0;
        PlanningMoveElapsed = 0.0;
        PlanningMoveOrigin = Entry.Unit->GetActorLocation();
        PlanningMoveRotation = Entry.Unit->GetActorRotation();
        Actions[Index].OriginalLocation = PlanningMoveOrigin;
        Entry.Unit->SetRoundCastMontage(nullptr);
        Entry.Status = RoundText(TEXT("SAP 이동 중"));
        return;
    }
    bSAPMovementInProgress = false;
    BeginActionResolution();
}

void ACombatRoundCoordinator::AdvanceSAPMovement(float DeltaSeconds)
{
    if (!bSAPMovementInProgress) return;
    if (PlanningMoveIndex == INDEX_NONE) BeginNextMove();
    if (!bSAPMovementInProgress) return;
    PlanningMoveElapsed += DeltaSeconds;
    bool bCanAdvance = View.Units.IsValidIndex(PlanningMoveIndex) && PlanningMovePath.IsValidIndex(PlanningMoveStep) && IsValid(Arena) && IsValid(Arena->Grid) && PlanningMoveElapsed <= MovementTimeout;
    AUnitBase* Unit = bCanAdvance ? View.Units[PlanningMoveIndex].Unit.Get() : nullptr;
    ACombatGridTile* Origin = bCanAdvance ? Arena->Grid->GetTileAtCoord(View.Units[PlanningMoveIndex].HomeCoord) : nullptr;
    ACombatGridTile* Next = bCanAdvance ? Arena->Grid->GetTileAtCoord(PlanningMovePath[PlanningMoveStep]) : nullptr;
    ACombatGridTile* Destination = bCanAdvance ? Arena->Grid->GetTileAtCoord(PlanningMovePath.Last()) : nullptr;
    bCanAdvance = bCanAdvance && IsValid(Unit) && Unit->IsUnitAlive() && IsValid(Origin) && Unit->GetCurrentTile() == Origin && Origin->GetOccupyingUnit() == Unit && IsValid(Next) && !Next->GetOccupyingUnit() && IsValid(Destination) && !Destination->GetOccupyingUnit();
    if (!bCanAdvance)
    {
        FinishPlanningMove(false);
    }
    else if (MoveUnitToward(PlanningMoveIndex, Next->GetActorLocation(), SAPMoveSpeed, DeltaSeconds))
    {
        ++PlanningMoveStep;
        if (PlanningMoveStep == PlanningMovePath.Num()) FinishPlanningMove(true);
    }
    if (PlanningMoveIndex == INDEX_NONE) BeginNextMove();
    PublishState();
}

void ACombatRoundCoordinator::FinishPlanningMove(bool bSucceeded)
{
    if (PlanningMoveIndex == INDEX_NONE) return;
    if (View.Units.IsValidIndex(PlanningMoveIndex))
    {
        FCombatRoundUnitView& Entry = View.Units[PlanningMoveIndex];
        AUnitBase* Unit = Entry.Unit;
        ACombatGridTile* Destination = IsValid(Arena) && IsValid(Arena->Grid) && !PlanningMovePath.IsEmpty() ? Arena->Grid->GetTileAtCoord(PlanningMovePath.Last()) : nullptr;
        bSucceeded = bSucceeded && IsValid(Unit) && Unit->IsUnitAlive() && IsValid(Destination) && !Destination->GetOccupyingUnit();
        if (IsValid(Unit) && Unit->IsUnitAlive())
        {
            if (bSucceeded)
            {
                Unit->SetCurrentTile(Destination);
                Entry.HomeCoord = Destination->GridCoord;
            }
            else
            {
                // Failed SAP movement keeps its paid cost and restores the origin before surviving AP actions run.
                // 실패한 SAP 이동은 지불한 비용을 유지하고 생존 AP 행동 전에 출발점을 복원합니다.
                Unit->SetActorLocation(PlanningMoveOrigin, false);
            }
            Unit->SetRoundMovementVelocity(FVector::ZeroVector);
            Unit->SetActorRotation(PlanningMoveRotation);
            Unit->ForceNetUpdate();
        }
        bSAPMovementFailed |= !bSucceeded;
        Entry.Status = RoundText(bSucceeded ? TEXT("SAP 이동 완료") : TEXT("SAP 이동 취소"));
    }
    PlanningMoveIndex = INDEX_NONE;
    PlanningMovePath.Reset();
    PlanningMoveStep = 0;
    PlanningMoveElapsed = 0.0;
}

bool ACombatRoundCoordinator::IsValidUnitTarget(int32 SourceUnitId, FName SkillId, int32 TargetUnitId) const
{
    const int32 SourceIndex = FindUnitIndex(SourceUnitId);
    const int32 TargetIndex = FindUnitIndex(TargetUnitId);
    const FCombatRoundSkill* Skill = FindSkill(SkillId);
    if (!View.Units.IsValidIndex(SourceIndex) || !View.Units.IsValidIndex(TargetIndex) || !Skill) return false;
    if (Skill->Kind == ECombatRoundSkillKind::Wait || Skill->Kind == ECombatRoundSkillKind::GroundAttack) return false;
    const FCombatRoundUnitView& Source = View.Units[SourceIndex];
    const FCombatRoundUnitView& Target = View.Units[TargetIndex];
    if (!IsValid(Source.Unit) || !IsValid(Target.Unit) || !Source.Unit->IsUnitAlive() || !Target.Unit->IsUnitAlive()) return false;
    if (!(Source.HP > 0.f) || !(Target.HP > 0.f) || !Source.SkillIds.Contains(SkillId)) return false;
    const bool bAlly = Source.bEnemy == Target.bEnemy;
    return !bAlly;
}

bool ACombatRoundCoordinator::ValidateCommand(const FCombatRoundCommand& Command, FText& OutError) const
{
    const int32 Index = FindUnitIndex(Command.UnitId);
    const FCombatRoundSkill* Skill = FindCommandSkill(Command);
    if (!View.Units.IsValidIndex(Index) || !Skill || !IsValid(View.Units[Index].Unit) || !View.Units[Index].Unit->IsUnitAlive())
    {
        OutError = RoundText(TEXT("행동할 유닛 또는 스킬이 올바르지 않습니다."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    if (!Command.SkillId.IsNone() && !Entry.SkillIds.Contains(Command.SkillId))
    {
        OutError = RoundText(TEXT("이 유닛에게 부여된 스킬이 아닙니다."));
        return false;
    }
    if ((Skill->ActionPointCost > 0 && !Entry.Unit->HasEnoughActionPoint(Skill->ActionPointCost)) || !Entry.Unit->HasEnoughSubActionPoint(Skill->SubActionPointCost + (Entry.bHasMovePlan ? 1 : 0)))
    {
        OutError = RoundText(TEXT("행동 AP 또는 예약 이동과 스킬의 합산 SAP가 부족합니다."));
        return false;
    }
    if (Skill->Kind == ECombatRoundSkillKind::Wait) return true;
    if (!Arena || !Arena->Grid)
    {
        OutError = RoundText(TEXT("전투 Grid가 없습니다."));
        return false;
    }
    if (Skill->Approach == ECombatRoundApproach::Tile)
    {
        ACombatGridTile* Destination = Arena->Grid->GetTileAtCoord(Command.DestinationCoord);
        if (!Destination || (Destination->GetOccupyingUnit() && Destination->GetOccupyingUnit() != Entry.Unit) || IsDestinationReservedByOther(Index, Command.DestinationCoord))
        {
            OutError = RoundText(TEXT("다른 유닛의 복귀 칸이나 예약 목적지가 아닌 빈 접근 칸을 선택하세요."));
            return false;
        }
        if (Skill->bRemainAtDestination && !CombatRoundRules::IsOwnTerritory(Entry.bEnemy, Command.DestinationCoord))
        {
            OutError = RoundText(TEXT("이동 공격의 최종 위치는 자기 진영이어야 합니다."));
            return false;
        }
    }
    if (Skill->Kind == ECombatRoundSkillKind::GroundAttack)
    {
        if (!Arena->Grid->GetTileAtCoord(Command.TargetCoord))
        {
            OutError = RoundText(TEXT("공격할 지점 칸을 선택하세요."));
            return false;
        }
        return true;
    }
    const int32 TargetIndex = FindUnitIndex(Command.TargetUnitId);
    if (!View.Units.IsValidIndex(TargetIndex) || !IsValid(View.Units[TargetIndex].Unit) || !View.Units[TargetIndex].Unit->IsUnitAlive())
    {
        OutError = RoundText(TEXT("살아 있는 대상 유닛을 선택하세요."));
        return false;
    }
    if (!IsValidUnitTarget(Command.UnitId, Command.SkillId, Command.TargetUnitId))
    {
        OutError = RoundText(TEXT("스킬의 대상 진영이 올바르지 않습니다."));
        return false;
    }
    return true;
}

bool ACombatRoundCoordinator::SubmitPlan(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, const FCombatRoundCommand& Command, FText& OutError)
{
    if (!ValidateRequest(Controller, CombatId, RoundNumber, Revision, OutError)) return false;
    const int32 Index = FindUnitIndex(Command.UnitId);
    if (!View.Units.IsValidIndex(Index) || !CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), View.Units[Index].Unit))
    {
        OutError = RoundText(TEXT("원래 자신의 캐릭터만 계획할 수 있습니다."));
        return false;
    }
    if (!CanPlanCommand(Command, OutError)) return false;
    const FCombatRoundView PreviousView = View;
    View.Units[Index].Command = Command;
    View.Units[Index].Status = RoundText(TEXT("계획 적용됨"));
    ClearOwnerReady(GetParticipantSlot(Controller));
    ++View.PlanRevision;
    View.Message = RoundText(TEXT("계획이 변경되어 자신의 준비만 해제되었습니다."));
    FText Conflict;
    if (!ValidateDestinations(Conflict)) View.Message = Conflict;
    if (!PersistPlanningCheckpoint(OutError))
    {
        View = PreviousView;
        return false;
    }
    bLockRetryBlocked = false;
    PublishState();
    return true;
}

bool ACombatRoundCoordinator::ValidateDestinations(FText& OutError, int32 CandidateIndex, const FCombatRoundCommand* CandidateCommand, const FIntPoint* CandidateMove) const
{
    TMap<FIntPoint, int32> Reserved;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        Reserved.Add(Entry.HomeCoord, Entry.UnitId);
    }
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const FCombatRoundUnitView& Entry = View.Units[Index];
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        const FCombatRoundCommand& Command = Index == CandidateIndex && CandidateCommand ? *CandidateCommand : Entry.Command;
        const FCombatRoundSkill* Skill = FindSkill(Command.SkillId);
        TArray<FIntPoint> Destinations;
        if (Index == CandidateIndex && CandidateMove) Destinations.Add(*CandidateMove);
        else if (Entry.bHasMovePlan) Destinations.Add(Entry.MoveDestinationCoord);
        if (Skill && Skill->Approach == ECombatRoundApproach::Tile) Destinations.Add(Command.DestinationCoord);
        for (FIntPoint Destination : Destinations)
        {
            const int32* Existing = Reserved.Find(Destination);
            if (Existing && *Existing != Entry.UnitId)
            {
                OutError = RoundText(TEXT("목적지가 다른 유닛의 원래 칸, SAP 이동 또는 AP 접근 목적지와 겹칩니다."));
                return false;
            }
            Reserved.Add(Destination, Entry.UnitId);
        }
    }
    return true;
}

bool ACombatRoundCoordinator::SetParticipantReady(APlayerController* Controller, FGuid CombatId, int32 RoundNumber, int32 Revision, bool bReady, FText& OutError)
{
    if (!ValidateRequest(Controller, CombatId, RoundNumber, Revision, OutError)) return false;
    const int32 Slot = GetParticipantSlot(Controller);
    if (bReady)
    {
        for (const FCombatRoundUnitView& Entry : View.Units)
        {
            if (Entry.OwnerSlot == Slot && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive() && !CanPlanCommand(Entry.Command, OutError)) return false;
        }
        if (!ValidateDestinations(OutError)) return false;
    }
    const FCombatRoundView PreviousView = View;
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        if (Entry.OwnerSlot == Slot && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive()) Entry.bReady = bReady;
    }
    ++View.PlanRevision;
    bool bAllReady = true;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive() && !Entry.bReady) bAllReady = false;
    }
    if (!(bAllReady ? LockPlans(OutError) : PersistPlanningCheckpoint(OutError)))
    {
        View = PreviousView;
        return false;
    }
    bLockRetryBlocked = false;
    PublishState();
    return true;
}

void ACombatRoundCoordinator::ClearOwnerReady(int32 OwnerSlot)
{
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        if (Entry.OwnerSlot == OwnerSlot && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive()) Entry.bReady = false;
    }
}

bool ACombatRoundCoordinator::LockPlans(FText& OutError)
{
    if (!ValidateDestinations(OutError)) return false;
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const FCombatRoundUnitView& Entry = View.Units[Index];
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        if (!ValidateCommand(Entry.Command, OutError) || (Entry.bHasMovePlan && !BuildPlanningMovePath(Entry.UnitId, Entry.MoveDestinationCoord, Actions[Index].MovePath, OutError)))
        {
            return false;
        }
    }

    // Persist the complete ready boundary before charging costs or executing any movement or attack.
    // 비용 차감이나 이동·공격 실행 전에 준비 완료 경계 전체를 저장합니다.
    if (!PersistPlanningCheckpoint(OutError)) return false;

    // Commit the complete AP and SAP budget only after every reservation passes validation.
    // 모든 예약 검증이 끝난 뒤에만 AP와 SAP 전체 비용을 확정합니다.
    bool bHasMoves = false;
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        const FCombatRoundSkill& Skill = *FindCommandSkill(Entry.Command);
        if (Skill.ActionPointCost > 0) Entry.Unit->ConsumeActionPoint(Skill.ActionPointCost);
        const int32 SAPCost = Skill.SubActionPointCost + (Entry.bHasMovePlan ? 1 : 0);
        if (SAPCost > 0) Entry.Unit->ConsumeSubActionPoint(SAPCost);
        bHasMoves |= Entry.bHasMovePlan;
        Entry.ActionPhase = ECombatRoundActionPhase::Waiting;
        Entry.Status = RoundText(TEXT("AP 실행 대기"));
    }
    View.Phase = ECombatRoundPhase::Resolving;
    Accumulator = 0.0;
    SimulationTime = 0.0;
    MontageClock = 0.0;
    bSAPMovementInProgress = bHasMoves;
    bSAPMovementFailed = false;
    NextMoveIndex = 0;
    if (bSAPMovementInProgress)
    {
        View.Message = RoundText(TEXT("계획 잠금 완료. 모든 예약 SAP 이동을 먼저 실행합니다."));
        BeginNextMove();
    }
    else BeginActionResolution();
    return true;
}

void ACombatRoundCoordinator::BeginActionResolution()
{
    if (!IsValid(Arena) || !IsValid(Arena->Grid))
    {
        SuspendRound();
        return;
    }
    // Every surviving attack captures its post-movement home and starts the original speed clock at zero.
    // 모든 생존 공격은 이동 후 원점을 캡처하고 기존 속도 시계를 0에서 시작합니다.
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        FCombatRoundUnitView& Entry = View.Units[Index];
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive())
        {
            Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
            continue;
        }
        const FCombatRoundSkill& Skill = *FindCommandSkill(Entry.Command);
        FActionRuntime& Action = Actions[Index];
        Action = FActionRuntime();
        Action.OriginalLocation = Entry.Unit->GetActorLocation();
        Action.OriginalRotation = Entry.Unit->GetActorRotation();
        Action.EffectiveTargetUnitId = Entry.Command.TargetUnitId;
        const int32 TargetIndex = FindUnitIndex(Entry.Command.TargetUnitId);
        Action.AimLocation = Action.OriginalLocation;
        if (View.Units.IsValidIndex(TargetIndex) && IsValid(View.Units[TargetIndex].Unit)) Action.AimLocation = View.Units[TargetIndex].Unit->GetActorLocation();
        if (Skill.Kind == ECombatRoundSkillKind::GroundAttack) Action.AimLocation = Arena->Grid->GetTileAtCoord(Entry.Command.TargetCoord)->GetActorLocation() + FVector(0.f, 0.f, 100.f);
        Action.Destination = Action.OriginalLocation;
        if (Skill.Approach == ECombatRoundApproach::Tile) Action.Destination = Arena->Grid->GetTileAtCoord(Entry.Command.DestinationCoord)->GetActorLocation() + FVector(0.f, 0.f, 100.f);
        Entry.ActionPhase = ECombatRoundActionPhase::Waiting;
        Entry.Status = RoundText(TEXT("시작 대기"));
    }
    View.Message = RoundText(bSAPMovementFailed ? TEXT("일부 SAP 이동이 실패했습니다. SAP는 환불되지 않으며 생존 캐릭터의 AP 행동을 계속 실행합니다.") : TEXT("SAP 이동 처리 완료. 속도차에 따라 AP 행동을 실행합니다."));
    Accumulator = 0.0;
    SimulationTime = 0.0;
    MontageClock = 0.0;
    AdvanceSimulation(0.f);
}

void ACombatRoundCoordinator::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !IsRoundSessionActive() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) return;
    if (!HasExecutionAuthority())
    {
        SuspendRound();
        return;
    }
    if (View.Phase == ECombatRoundPhase::Planning)
    {
        bool bAllReady = true;
        for (const FCombatRoundUnitView& Entry : View.Units)
        {
            if (IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive() && !Entry.bReady) bAllReady = false;
        }
        FText Error;
        if (bAllReady && !bLockRetryBlocked && !LockPlans(Error))
        {
            bLockRetryBlocked = true;
            View.Message = Error;
            PublishState();
        }
        return;
    }

    if (bSAPMovementInProgress)
    {
        AdvanceSAPMovement(DeltaSeconds);
        return;
    }
    // Montage playback advances once per frame; catching up simulation debt must not consume a newly started animation.
    // 몽타주 재생은 프레임마다 진행하므로 누적 시뮬레이션 시간을 따라잡으며 새로 시작한 애니메이션 시간을 소진하지 않습니다.
    MontageClock += DeltaSeconds;
    // Retain simulation debt instead of dropping elapsed time when a frame is slow.
    // 프레임이 느릴 때 경과 시간을 버리지 않고 남은 시뮬레이션 시간을 보존합니다.
    Accumulator += DeltaSeconds;
    int32 Steps = 0;
    while (Accumulator + UE_DOUBLE_SMALL_NUMBER >= RoundStep && View.Phase == ECombatRoundPhase::Resolving && Steps < 50)
    {
        Accumulator -= RoundStep;
        SimulationTime += RoundStep;
        AdvanceSimulation(RoundStep);
        ++Steps;
    }
    if (View.Phase == ECombatRoundPhase::Resolving) PublishState();
}

void ACombatRoundCoordinator::AdvanceSimulation(float StepSeconds)
{
    if (View.Phase != ECombatRoundPhase::Resolving || !HasExecutionAuthority())
    {
        SuspendRound();
        return;
    }
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        AdvanceAction(Index, StepSeconds);
    }
    const TArray<TObjectPtr<ACombatRoundProjectile>> Pending = Projectiles;
    for (ACombatRoundProjectile* Projectile : Pending)
    {
        if (IsValid(Projectile) && !Projectile->HasResolved()) Projectile->AdvanceProjectile(StepSeconds);
    }
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive())
        {
            Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
            Entry.Status = RoundText(TEXT("사망으로 남은 행동 취소"));
        }
    }
    FinishRoundIfSettled();
}

bool ACombatRoundCoordinator::MoveUnitToward(int32 Index, FVector Destination, float Speed, float StepSeconds)
{
    AUnitBase* Unit = View.Units[Index].Unit;
    const FVector Current = Unit->GetActorLocation();
    Destination.Z = Actions[Index].OriginalLocation.Z;
    const FVector Difference = Destination - Current;
    if (Difference.SizeSquared2D() <= 4.f)
    {
        Unit->SetActorLocation(Destination, false);
        Unit->SetRoundMovementVelocity(FVector::ZeroVector);
        return true;
    }
    const FVector Delta = Difference.GetClampedToMaxSize(Speed * StepSeconds);
    FHitResult Hit;
    Unit->SetActorLocation(Current + Delta, true, &Hit);
    if (!Difference.IsNearlyZero()) Unit->SetActorRotation(Difference.Rotation());
    if (StepSeconds > 0.f) Unit->SetRoundMovementVelocity((Unit->GetActorLocation() - Current) / StepSeconds);
    return FVector::DistSquared2D(Unit->GetActorLocation(), Destination) <= 4.f;
}

void ACombatRoundCoordinator::AdvanceAction(int32 Index, float StepSeconds)
{
    FCombatRoundUnitView& Entry = View.Units[Index];
    FActionRuntime& Action = Actions[Index];
    if (CombatRoundRules::IsTerminal(Entry.ActionPhase)) return;
    if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive())
    {
        Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
        Entry.Status = RoundText(TEXT("발동 전 사망으로 취소"));
        return;
    }
    const FCombatRoundSkill* Skill = FindCommandSkill(Entry.Command);
    if (!Skill)
    {
        StartReturn(Index, true, RoundText(TEXT("스킬 정의 누락")));
        return;
    }
    if (Entry.ActionPhase == ECombatRoundActionPhase::Waiting)
    {
        if (SimulationTime + 0.00001 < Entry.StartDelay) return;
        Entry.Unit->SetRoundCastMontage(nullptr);
        Action.PhaseStarted = SimulationTime;
        Entry.ActionPhase = ECombatRoundActionPhase::Casting;
        Entry.Status = RoundText(TEXT("시전 중"));
        if (Skill->Kind == ECombatRoundSkillKind::Wait)
        {
            Entry.ActionPhase = ECombatRoundActionPhase::Complete;
            Entry.Status = RoundText(TEXT("대기 완료"));
            return;
        }
        if (Skill->Approach != ECombatRoundApproach::None)
        {
            Entry.ActionPhase = ECombatRoundActionPhase::Approaching;
            Entry.Status = RoundText(TEXT("접근 중"));
        }
    }
    if (Entry.ActionPhase == ECombatRoundActionPhase::Approaching || Entry.ActionPhase == ECombatRoundActionPhase::Casting)
    {
        int32 TargetIndex = FindUnitIndex(Action.EffectiveTargetUnitId);
        const bool bNeedsUnit = Skill->Kind != ECombatRoundSkillKind::GroundAttack;
        if (bNeedsUnit && (!View.Units.IsValidIndex(TargetIndex) || !IsValid(View.Units[TargetIndex].Unit) || !View.Units[TargetIndex].Unit->IsUnitAlive()))
        {
            if (Skill->TargetLoss == ECombatRoundTargetLoss::NearestEnemy)
            {
                TargetIndex = FindNearestEnemy(Index);
                if (View.Units.IsValidIndex(TargetIndex)) Action.EffectiveTargetUnitId = View.Units[TargetIndex].UnitId;
            }
            if (Skill->TargetLoss == ECombatRoundTargetLoss::Cancel || (Skill->TargetLoss == ECombatRoundTargetLoss::NearestEnemy && !View.Units.IsValidIndex(TargetIndex)))
            {
                StartReturn(Index, true, RoundText(TEXT("목표 사망으로 불발")));
                return;
            }
        }
        if (View.Units.IsValidIndex(TargetIndex) && IsValid(View.Units[TargetIndex].Unit) && View.Units[TargetIndex].Unit->IsUnitAlive() && Skill->Kind != ECombatRoundSkillKind::GroundAttack)
        {
            Action.AimLocation = View.Units[TargetIndex].Unit->GetActorLocation();
        }
        if (Entry.ActionPhase == ECombatRoundActionPhase::Approaching)
        {
            FVector Destination = Action.Destination;
            bool bWithinApproachRange = false;
            if (Skill->Approach == ECombatRoundApproach::Tile)
            {
                ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Entry.Command.DestinationCoord);
                if (!Tile || (Tile->GetOccupyingUnit() && Tile->GetOccupyingUnit() != Entry.Unit) || IsDestinationReservedByOther(Index, Entry.Command.DestinationCoord))
                {
                    StartReturn(Index, true, RoundText(TEXT("접근 칸 점유 또는 예약 충돌로 불발")));
                    return;
                }
            }
            if (Skill->Approach == ECombatRoundApproach::Unit)
            {
                const FVector FromTarget = Entry.Unit->GetActorLocation() - Action.AimLocation;
                const float ApproachRange = FMath::Max(20.f, Skill->HitRange * 0.7f);
                // A moving target entering reach ends approach; never retreat to align with a point behind the attacker.
                // 이동 중인 목표가 접근 범위에 들어오면 접근을 끝내며 공격자 뒤쪽 지점에 맞추려고 후퇴하지 않습니다.
                bWithinApproachRange = FromTarget.SizeSquared2D() <= FMath::Square(ApproachRange);
                Destination = Action.AimLocation + FromTarget.GetSafeNormal2D() * ApproachRange;
            }
            if (SimulationTime - Action.PhaseStarted > MovementTimeout)
            {
                StartReturn(Index, true, RoundText(TEXT("접근 시간 초과로 불발")));
                return;
            }
            if (!bWithinApproachRange && !MoveUnitToward(Index, Destination, CombatRoundRules::AttackMoveSpeed(*Skill, Entry.Speed), StepSeconds)) return;
            Action.PhaseStarted = SimulationTime;
            Entry.ActionPhase = ECombatRoundActionPhase::Casting;
            Entry.Status = RoundText(TEXT("시전 중"));
        }
        if (Entry.ActionPhase == ECombatRoundActionPhase::Casting)
        {
            FVector Facing = Action.AimLocation - Entry.Unit->GetActorLocation();
            Facing.Z = 0.f;
            if (!Facing.IsNearlyZero()) Entry.Unit->SetActorRotation(Facing.Rotation());
            if (!Action.bMontageStarted)
            {
                UAnimMontage* Montage = Entry.Unit->ResolveRoundCastMontage(Skill->CastMontage);
                Action.bMontageStarted = true;
                Action.MontageStartedAt = MontageClock;
                Action.MontageRecoverySeconds = MontageRecoveryBudget(Montage);
                Entry.Unit->SetRoundMovementVelocity(FVector::ZeroVector);
                Entry.Unit->SetRoundCastMontage(Montage);
                Action.bTrackMontageCompletion = Entry.Unit->HasRoundCastMontageInstance();
            }
            if (Skill->bUseWeaponTrace)
            {
                AdvanceWeaponTrace(Index, *Skill);
                return;
            }
            if (SimulationTime + 0.00001 < Action.PhaseStarted + Skill->WindupSeconds) return;
            ReleaseSkill(Index, *Skill);
            return;
        }
    }
    if (Entry.ActionPhase == ECombatRoundActionPhase::Recovery)
    {
        const bool bHasMontageInstance = Entry.Unit->HasRoundCastMontageInstance();
        const bool bFinished = Action.bTrackMontageCompletion && !bHasMontageInstance;
        if (!bFinished && MontageClock + 0.00001 < Action.MontageStartedAt + Action.MontageRecoverySeconds) return;
        if (!bFinished)
        {
            if (bHasMontageInstance) UE_LOG(LogTemp, Warning, TEXT("[RoundAnimation] Recovery timeout Unit=%d Round=%d / 몽타주 종료 대기 시간 초과로 표현을 정리하고 복귀합니다"), Entry.UnitId, View.RoundNumber);
            // A zero blend stops held or looping poses before return movement begins, including on remote clients.
            // 원격 클라이언트를 포함하여 복귀 이동 전에 유지되거나 반복되는 자세를 0초 블렌드로 정리합니다.
            Entry.Unit->SetRoundCastMontage(nullptr, true);
        }
        StartReturn(Index, Action.bFailed, Entry.Status);
        return;
    }

    if (Entry.ActionPhase == ECombatRoundActionPhase::Returning)
    {
        if (SimulationTime - Action.PhaseStarted > MovementTimeout)
        {
            // Recover a failed return to its reserved home, never strand a survivor in enemy territory.
            // 복귀 실패 시 예약한 원위치로 정리하여 생존 유닛을 상대 진영에 남기지 않습니다.
            Entry.Unit->SetActorLocation(Action.OriginalLocation, false);
            Action.bFailed = true;
            Entry.Status = RoundText(TEXT("복귀 시간 초과: 예약 칸으로 복원"));
            UE_LOG(LogTemp, Warning, TEXT("[Round] Return timeout Unit=%d Round=%d / 복귀 시간 초과"), Entry.UnitId, View.RoundNumber);
        }
        else if (!MoveUnitToward(Index, Action.OriginalLocation, CombatRoundRules::AttackMoveSpeed(*Skill, Entry.Speed), StepSeconds))
        {
            return;
        }
        Entry.Unit->SetRoundMovementVelocity(FVector::ZeroVector);
        // Return travel faces home; restore the pre-action facing once the return has settled.
        // 복귀 이동 중에는 원위치를 바라보고 복귀가 끝나면 행동 전 방향을 복원합니다.
        Entry.Unit->SetActorRotation(Action.OriginalRotation);
        Entry.Unit->ForceNetUpdate();
        Entry.ActionPhase = ECombatRoundActionPhase::Complete;
        if (Action.bFailed) Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
    }
}

void ACombatRoundCoordinator::StartRecovery(int32 Index, bool bFailed, const FText& Status)
{
    FCombatRoundUnitView& Entry = View.Units[Index];
    FActionRuntime& Action = Actions[Index];
    if (Action.MontageRecoverySeconds <= 0.0)
    {
        StartReturn(Index, bFailed, Status);
        return;
    }
    // Release remains authoritative and runs once; only the movement home waits for presentation to finish.
    // 발동은 서버에서 한 번만 실행하며 표현 종료를 기다리는 대상은 원위치 복귀뿐입니다.
    Action.bFailed = bFailed;
    Entry.Status = Status;
    Entry.ActionPhase = ECombatRoundActionPhase::Recovery;
}

void ACombatRoundCoordinator::StartReturn(int32 Index, bool bFailed, const FText& Status)
{
    FCombatRoundUnitView& Entry = View.Units[Index];
    FActionRuntime& Action = Actions[Index];
    Action.bFailed = bFailed;
    Action.PhaseStarted = SimulationTime;
    Entry.Status = Status;
    Entry.ActionPhase = ECombatRoundActionPhase::Returning;
    if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive())
    {
        Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
        return;
    }
    const FCombatRoundSkill* Skill = FindCommandSkill(Entry.Command);
    if (bFailed && !Action.bReleased) Entry.Unit->SetRoundCastMontage(nullptr);
    if (Skill && Skill->bRemainAtDestination && !bFailed)
    {
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Entry.Command.DestinationCoord);
        if (Tile && CombatRoundRules::IsOwnTerritory(Entry.bEnemy, Tile->GridCoord) && (!Tile->GetOccupyingUnit() || Tile->GetOccupyingUnit() == Entry.Unit) && !IsDestinationReservedByOther(Index, Tile->GridCoord))
        {
            Entry.Unit->SetCurrentTile(Tile);
            Entry.HomeCoord = Tile->GridCoord;
            Entry.Unit->SetRoundMovementVelocity(FVector::ZeroVector);
            Entry.Unit->ForceNetUpdate();
            Entry.ActionPhase = ECombatRoundActionPhase::Complete;
            return;
        }
        Action.bFailed = true;
        bFailed = true;
        Entry.Status = RoundText(TEXT("최종 배치 실패: 출발 칸으로 복귀"));
    }
    if (FVector::DistSquared2D(Entry.Unit->GetActorLocation(), Action.OriginalLocation) <= 4.f)
    {
        Entry.Unit->SetRoundMovementVelocity(FVector::ZeroVector);
        Entry.Unit->SetActorRotation(Action.OriginalRotation);
        Entry.Unit->ForceNetUpdate();
        Entry.ActionPhase = ECombatRoundActionPhase::Complete;
        if (bFailed) Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
    }
}

void ACombatRoundCoordinator::AdvanceWeaponTrace(int32 Index, const FCombatRoundSkill& Skill)
{
    FCombatRoundUnitView& Entry = View.Units[Index];
    FActionRuntime& Action = Actions[Index];
    if (Action.bReleased || !HasExecutionAuthority() || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) return;
    UAnimMontage* Montage = Entry.Unit->ResolveRoundCastMontage(Skill.CastMontage);
    if (!IsValid(Montage) || !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.f)
    {
        Action.bReleased = true;
        StartRecovery(Index, true, RoundText(TEXT("검 공격 애니메이션 설정 누락")));
        return;
    }
    // Clip simulation catch-up to animation time so a newly started swing cannot hit within the same stalled frame.
    // 새 휘두르기가 지연된 동일 프레임에서 타격하지 않도록 누적 시뮬레이션을 애니메이션 시간으로 제한합니다.
    const double Elapsed = FMath::Min(SimulationTime - Action.PhaseStarted, MontageClock - Action.MontageStartedAt);
    const double WindowEnd = Skill.WindupSeconds + Skill.WeaponTraceDuration;
    if (Elapsed + UE_DOUBLE_SMALL_NUMBER < Skill.WindupSeconds) return;
    const double SampleUntil = FMath::Min(Elapsed, WindowEnd);
    const bool bFirstSample = Action.WeaponTraceTime < 0.0;
    if (!bFirstSample && SampleUntil <= Action.WeaponTraceTime + UE_DOUBLE_SMALL_NUMBER) return;
    double SampleTime = bFirstSample ? Skill.WindupSeconds : FMath::Min(Action.WeaponTraceTime + 0.005, SampleUntil);
    for (;;)
    {
        CombatWeaponTrace::FBladePose Current;
        if (!CombatWeaponTrace::SampleBlade(Entry.Unit, Skill, Montage, SampleTime * Montage->RateScale, Current))
        {
            Action.bReleased = true;
            StartRecovery(Index, true, RoundText(TEXT("검 장착 또는 칼날 소켓 설정 누락")));
            return;
        }
        const CombatWeaponTrace::FBladePose Previous = Action.WeaponTraceTime < 0.0 ? Current : CombatWeaponTrace::FBladePose{Action.PreviousBladeBase, Action.PreviousBladeTip};
        Action.WeaponTraceTime = SampleTime;
        Action.PreviousBladeBase = Current.Base;
        Action.PreviousBladeTip = Current.Tip;
        if (AUnitBase* HitUnit = CombatWeaponTrace::FindFirstHit(GetWorld(), Entry.Unit, View.Units, Previous, Current, Skill.WeaponTraceRadius))
        {
            Action.bReleased = true;
            ApplyHit(Entry.Unit, HitUnit, Skill.Power);
            StartRecovery(Index, false, RoundText(TEXT("검 타격 완료")));
            return;
        }
        if (SampleTime + UE_DOUBLE_SMALL_NUMBER >= SampleUntil) break;
        SampleTime = FMath::Min(SampleTime + 0.005, SampleUntil);
    }
    if (Elapsed + UE_DOUBLE_SMALL_NUMBER >= WindowEnd)
    {
        Action.bReleased = true;
        StartRecovery(Index, true, RoundText(TEXT("칼날 충돌 없음 또는 장애물에 차단됨")));
    }
}

void ACombatRoundCoordinator::ReleaseSkill(int32 Index, const FCombatRoundSkill& Skill)
{
    FCombatRoundUnitView& Entry = View.Units[Index];
    FActionRuntime& Action = Actions[Index];
    if (Action.bReleased || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) return;
    Action.bReleased = true;
    const int32 TargetIndex = FindUnitIndex(Action.EffectiveTargetUnitId);
    AUnitBase* Target = nullptr;
    if (View.Units.IsValidIndex(TargetIndex)) Target = View.Units[TargetIndex].Unit;
    if (Skill.Kind == ECombatRoundSkillKind::Projectile)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACombatRoundProjectile* Projectile = GetWorld()->SpawnActor<ACombatRoundProjectile>(Entry.Unit->GetActorLocation(), FRotator::ZeroRotator, Params);
        if (!Projectile)
        {
            StartRecovery(Index, true, RoundText(TEXT("투사체 생성 실패")));
            return;
        }
        Projectiles.Add(Projectile);
        Projectile->OnImpact.AddUObject(this, &ACombatRoundCoordinator::ApplyHit);
        Projectile->OnResolved.AddUObject(this, &ACombatRoundCoordinator::HandleProjectileResolved);
        TArray<AUnitBase*> AllowedTargets;
        for (const FCombatRoundUnitView& Candidate : View.Units) AllowedTargets.Add(Candidate.Unit);
        Projectile->SetAllowedTargets(AllowedTargets);
        Projectile->InitializeProjectile(Entry.Unit, Target, Action.AimLocation, Skill.ProjectileSpeed, Skill.Power, Skill.ProjectileRadius, Skill.ProjectileLifetime, Skill.bHoming, Skill.bTargetOnly);
        StartRecovery(Index, false, RoundText(TEXT("발사 완료")));
        return;
    }
    bool bHit = false;
    if (Skill.Kind == ECombatRoundSkillKind::GroundAttack)
    {
        if (Skill.Approach == ECombatRoundApproach::None || FVector::Dist(Entry.Unit->GetActorLocation(), Action.AimLocation) <= Skill.HitRange)
        {
            for (AUnitBase* HitUnit : FindGroundCollisions(GetWorld(), Entry.Unit, View.Units, Action.AimLocation, Skill.HitRange))
            {
                ApplyHit(Entry.Unit, HitUnit, Skill.Power);
                bHit = true;
            }
        }
    }
    else if (Skill.Kind == ECombatRoundSkillKind::Melee)
    {
        if (Skill.bUseMeleeAreaCollision)
        {
            for (AUnitBase* HitUnit : FindMeleeAreaCollisions(GetWorld(), Entry.Unit, View.Units, Skill))
            {
                ApplyHit(Entry.Unit, HitUnit, Skill.Power);
                bHit = true;
            }
        }
        else if (Skill.MeleeArea == ESkillAreaType::TargetAndSides)
        {
            const FIntPoint TargetCoord = View.Units.IsValidIndex(TargetIndex) ? View.Units[TargetIndex].HomeCoord : Entry.Command.TargetCoord;
            for (AUnitBase* HitUnit : FindMeleeSideCollisions(GetWorld(), Entry.Unit, View.Units, Skill, IsValid(Arena) ? Arena->Grid.Get() : nullptr, TargetCoord, Action.AimLocation))
            {
                ApplyHit(Entry.Unit, HitUnit, Skill.Power);
                bHit = true;
            }
        }
        else if (AUnitBase* HitUnit = FindMeleeCollision(GetWorld(), Entry.Unit, View.Units, Skill))
        {
            ApplyHit(Entry.Unit, HitUnit, Skill.Power);
            bHit = true;
        }
    }
    if (bHit) StartRecovery(Index, false, RoundText(TEXT("타격 완료")));
    else StartRecovery(Index, true, RoundText(TEXT("공격 충돌 없음 또는 장애물에 차단됨")));
}

void ACombatRoundCoordinator::ApplyHit(AUnitBase* Source, AUnitBase* Target, float Damage)
{
    if (!HasAuthority() || View.Phase != ECombatRoundPhase::Resolving || !IsValid(Source) || !IsValid(Target) || !Target->IsUnitAlive() || Source->GetTeam() == Target->GetTeam() || !FMath::IsFinite(Damage) || Damage < 0.f) return;
    const int32 Index = View.Units.IndexOfByPredicate([Target](const FCombatRoundUnitView& Entry) { return Entry.Unit == Target; });
    if (!View.Units.IsValidIndex(Index)) return;
    FCombatRoundUnitView& Entry = View.Units[Index];
    if (Damage > 0.f) UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), Damage);
    Entry.HP = Target->GetAttributeSet() ? Target->GetAttributeSet()->GetHP() : 0.f;
    if (!Target->IsUnitAlive())
    {
        Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
        Entry.Status = RoundText(TEXT("사망으로 남은 행동 취소"));
    }
    UE_LOG(LogTemp, Log, TEXT("[Round] Round=%d Time=%.2f Source=%d Target=%d Damage=%.1f / 서버 피격"), View.RoundNumber, SimulationTime, Source->UnitIndex, Target->UnitIndex, Damage);
}

void ACombatRoundCoordinator::HandleProjectileResolved(ACombatRoundProjectile* Projectile)
{
    if (bCleaningUp) return;
    Projectiles.Remove(Projectile);
}

void ACombatRoundCoordinator::FinishRoundIfSettled()
{
    if (View.Phase != ECombatRoundPhase::Resolving || !Projectiles.IsEmpty()) return;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (!CombatRoundRules::IsTerminal(Entry.ActionPhase)) return;
    }
    bool bPlayersAlive = false;
    bool bEnemiesAlive = false;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        if (Entry.bEnemy) bEnemiesAlive = true;
        else bPlayersAlive = true;
    }
    if (bPlayersAlive && bEnemiesAlive)
    {
        BeginPlanning();
        return;
    }
    View.Phase = ECombatRoundPhase::Finished;
    View.Message = RoundText(TEXT("패배"));
    ECombatResult Result = ECombatResult::Defeat;
    if (bPlayersAlive)
    {
        View.Message = RoundText(TEXT("승리"));
        Result = ECombatResult::Victory;
    }
    PublishState();
    OnCombatFinished.Broadcast(Result);
}
