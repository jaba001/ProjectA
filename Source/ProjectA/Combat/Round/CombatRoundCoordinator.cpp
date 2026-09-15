#include "Combat/Round/CombatRoundCoordinator.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/PartyPlayerController.h"
#include "EngineUtils.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Combat/Library/CombatEffectLibrary.h"
#include "Combat/Round/CombatRoundProjectile.h"
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
    constexpr double MovementTimeout = 6.0;

    FText RoundText(const TCHAR* Value)
    {
        return FText::FromString(Value);
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
    DOREPLIFETIME(ACombatRoundCoordinator, Arena);
}

void ACombatRoundCoordinator::BuildPrototypeSkills()
{
    Skills.Reset();
    FCombatRoundSkill Skill;
    Skill.SkillId = TEXT("Strike");
    Skill.Name = RoundText(TEXT("접근 타격"));
    Skills.Add(Skill);

    Skill = FCombatRoundSkill();
    Skill.SkillId = TEXT("Arrow");
    Skill.Name = RoundText(TEXT("직선 투사체"));
    Skill.Kind = ECombatRoundSkillKind::Projectile;
    Skill.Approach = ECombatRoundApproach::None;
    Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
    Skill.bTargetOnly = false;
    Skill.Power = 20.f;
    Skill.WindupSeconds = 0.2f;
    Skills.Add(Skill);

    Skill = FCombatRoundSkill();
    Skill.SkillId = TEXT("GroundStrike");
    Skill.Name = RoundText(TEXT("지점 접근 타격"));
    Skill.Kind = ECombatRoundSkillKind::GroundAttack;
    Skill.Approach = ECombatRoundApproach::Tile;
    Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
    Skill.Power = 20.f;
    Skill.HitRange = 170.f;
    Skill.WindupSeconds = 0.45f;
    Skills.Add(Skill);

    Skill = FCombatRoundSkill();
    Skill.SkillId = TEXT("MoveShot");
    Skill.Name = RoundText(TEXT("아군 칸 이동 사격"));
    Skill.Kind = ECombatRoundSkillKind::Projectile;
    Skill.Approach = ECombatRoundApproach::Tile;
    Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
    Skill.bRemainAtDestination = true;
    Skill.Power = 15.f;
    Skill.bTargetOnly = false;
    Skill.SubActionPointCost = 1;
    Skills.Add(Skill);

    Skill = FCombatRoundSkill();
    Skill.SkillId = TEXT("Guard");
    Skill.Name = RoundText(TEXT("아군 엄호"));
    Skill.Kind = ECombatRoundSkillKind::Guard;
    Skill.Approach = ECombatRoundApproach::None;
    Skill.Power = 25.f;
    Skill.WindupSeconds = 0.15f;
    Skill.HitRange = 1500.f;
    Skills.Add(Skill);

    Skill = FCombatRoundSkill();
    Skill.SkillId = TEXT("Wait");
    Skill.Name = RoundText(TEXT("행동 대기"));
    Skill.Kind = ECombatRoundSkillKind::Wait;
    Skill.Approach = ECombatRoundApproach::None;
    Skill.ActionPointCost = 0;
    Skill.Power = 0.f;
    Skills.Add(Skill);
}

const FCombatRoundSkill* ACombatRoundCoordinator::FindSkill(FName SkillId) const
{
    return Skills.FindByPredicate([SkillId](const FCombatRoundSkill& Skill) { return Skill.SkillId == SkillId; });
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
    BuildPrototypeSkills();
    View.CombatId = InManager->GetCombatInstanceId();
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
        if (!IsValid(Unit) || !Unit->IsUnitAlive() || !Unit->GetCurrentTile() || !Unit->GetAttributeSet())
        {
            OutError = RoundText(TEXT("유닛의 시작 위치 또는 생존 상태가 올바르지 않습니다."));
            return false;
        }
        if (Arena->Grid->GetTileAtCoord(Unit->GetCurrentTile()->GridCoord) != Unit->GetCurrentTile() || Unit->GetCurrentTile()->GetOccupyingUnit() != Unit || !CombatRoundRules::IsOwnTerritory(Unit->GetTeam() == ETeam::Enemy, Unit->GetCurrentTile()->GridCoord))
        {
            OutError = RoundText(TEXT("유닛의 시작 칸과 전투 Grid·진영·점유가 일치하지 않습니다."));
            return false;
        }
        Unit->OnTurnEnd();
        Unit->GetCharacterMovement()->StopMovementImmediately();
        Unit->GetCharacterMovement()->DisableMovement();
        Unit->GetCharacterMovement()->SetComponentTickEnabled(false);
        Unit->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        FCombatRoundUnitView Entry;
        Entry.UnitId = View.Units.Num() + 1;
        Entry.Unit = Unit;
        Entry.bEnemy = Unit->GetTeam() == ETeam::Enemy;
        Entry.HomeCoord = Unit->GetCurrentTile()->GridCoord;
        Entry.Speed = Unit->CombatSpeed;
        Entry.HP = Unit->GetAttributeSet()->GetHP();
        Entry.SkillIds = {TEXT("Guard"), TEXT("Wait"), TEXT("MoveShot"), TEXT("GroundStrike")};
        for (USkillDefinitionDataAsset* Definition : Unit->GetEquippedSkillDataAssets())
        {
            if (!IsValid(Definition)) continue;
            const FName SkillId(*Definition->GetPrimaryAssetId().ToString());
            if (!FindSkill(SkillId))
            {
                FCombatRoundSkill Skill;
                if (!Definition->ResolveRoundSkill(Skill, OutError)) return false;
                Skills.Add(Skill);
            }
            Entry.SkillIds.AddUnique(SkillId);
        }
        if (Unit->GetEquippedSkillDataAssets().IsEmpty())
        {
            Entry.SkillIds.Add(TEXT("Strike"));
            Entry.SkillIds.Add(TEXT("Arrow"));
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
        if (!Entry.bEnemy && !InManager->IsPartyAIControlled(Unit) && Entry.OwnerSlot == 0)
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
    View.Phase = ECombatRoundPhase::Suspended;
    View.Message = RoundText(TEXT("전투가 중단되었습니다. 시간차 전투의 중간 복구는 아직 지원하지 않습니다."));
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
    int32 HighestSpeed = 0;
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        Entry.HP = 0.f;
        if (IsValid(Entry.Unit) && Entry.Unit->GetAttributeSet()) Entry.HP = Entry.Unit->GetAttributeSet()->GetHP();
        if (IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive())
        {
            Entry.Speed = FMath::Max(0, Entry.Unit->CombatSpeed);
            HighestSpeed = FMath::Max(HighestSpeed, Entry.Speed);
            Entry.Unit->ResetActionPoint();
            Entry.Unit->ResetSubActionPoint();
        }
    }
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        FCombatRoundUnitView& Entry = View.Units[Index];
        Entry.Guard = 0.f;
        Entry.StartDelay = CombatRoundRules::StartDelay(HighestSpeed, Entry.Speed);
        Entry.Command = FCombatRoundCommand();
        Entry.Command.UnitId = Entry.UnitId;
        Entry.Command.DestinationCoord = Entry.HomeCoord;
        Entry.Command.TargetCoord = Entry.HomeCoord;
        Entry.bReady = false;
        Entry.Status = RoundText(TEXT("행동 선택 필요"));
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
        Entry.Command.SkillId = TEXT("Wait");
        if (View.Units.IsValidIndex(TargetIndex))
        {
            Entry.Command.TargetUnitId = View.Units[TargetIndex].UnitId;
            Entry.Command.TargetCoord = View.Units[TargetIndex].HomeCoord;
            TSet<FName> EquippedSkillIds;
            for (const USkillDefinitionDataAsset* Definition : Entry.Unit->GetEquippedSkillDataAssets())
            {
                if (IsValid(Definition)) EquippedSkillIds.Add(FName(*Definition->GetPrimaryAssetId().ToString()));
            }
            for (FName SkillId : Entry.SkillIds)
            {
                const FCombatRoundSkill* Candidate = FindSkill(SkillId);
                if (!Candidate || Candidate->Kind == ECombatRoundSkillKind::Wait || Candidate->Kind == ECombatRoundSkillKind::Guard || Candidate->bRemainAtDestination) continue;
                // Only equipped return attacks gain tile approach; prototype tactics keep their previous priority.
                // 장착한 복귀형 공격만 타일 접근을 허용하여 시험 전술의 기존 선택 우선순위를 유지합니다.
                if (Candidate->Approach == ECombatRoundApproach::Tile && !EquippedSkillIds.Contains(SkillId)) continue;
                Entry.Command.SkillId = SkillId;
                Entry.Command.DestinationCoord = Candidate->Approach == ECombatRoundApproach::Tile ? View.Units[TargetIndex].HomeCoord : Entry.HomeCoord;
                FText Error;
                if (ValidateCommand(Entry.Command, Error)) break;
                Entry.Command.SkillId = TEXT("Wait");
                Entry.Command.DestinationCoord = Entry.HomeCoord;
            }
        }
        Entry.bReady = true;
        Entry.Status = RoundText(TEXT("AI 계획 고정"));
    }
    View.Message = RoundText(TEXT("각 유닛의 행동을 적용한 뒤 준비 완료를 선택하세요. 적 계획은 이미 고정되었습니다."));
    PublishState();
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
    if (View.Phase != ECombatRoundPhase::Planning)
    {
        OutError = RoundText(TEXT("계획 단계에서만 행동을 선택할 수 있습니다."));
        return false;
    }
    return ValidateCommand(Command, OutError);
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
    return Skill->Kind == ECombatRoundSkillKind::Guard ? bAlly : !bAlly;
}

bool ACombatRoundCoordinator::ValidateCommand(const FCombatRoundCommand& Command, FText& OutError) const
{
    const int32 Index = FindUnitIndex(Command.UnitId);
    const FCombatRoundSkill* Skill = FindSkill(Command.SkillId);
    if (!View.Units.IsValidIndex(Index) || !Skill || !IsValid(View.Units[Index].Unit) || !View.Units[Index].Unit->IsUnitAlive())
    {
        OutError = RoundText(TEXT("행동할 유닛 또는 스킬이 올바르지 않습니다."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    if (!Entry.SkillIds.Contains(Command.SkillId))
    {
        OutError = RoundText(TEXT("이 유닛에게 부여된 스킬이 아닙니다."));
        return false;
    }
    if ((Skill->ActionPointCost > 0 && !Entry.Unit->HasEnoughActionPoint(Skill->ActionPointCost)) || !Entry.Unit->HasEnoughSubActionPoint(Skill->SubActionPointCost))
    {
        OutError = RoundText(TEXT("행동에 필요한 AP 또는 보조 AP가 부족합니다."));
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
        if (!Arena->Grid->GetTileAtCoord(Command.DestinationCoord))
        {
            OutError = RoundText(TEXT("접근 목적지 칸을 선택하세요."));
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
    if (!ValidateCommand(Command, OutError)) return false;
    View.Units[Index].Command = Command;
    View.Units[Index].Status = RoundText(TEXT("계획 적용됨"));
    for (FCombatRoundUnitView& Entry : View.Units)
    {
        if (Entry.OwnerSlot > 0 && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive()) Entry.bReady = false;
    }
    ++View.PlanRevision;
    View.Message = RoundText(TEXT("계획이 변경되었습니다. 팀 준비를 다시 확인하세요."));
    FText Conflict;
    if (!ValidateDestinations(Conflict)) View.Message = Conflict;
    PublishState();
    return true;
}

bool ACombatRoundCoordinator::ValidateDestinations(FText& OutError) const
{
    TMap<FIntPoint, int32> Reserved;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        Reserved.Add(Entry.HomeCoord, Entry.UnitId);
    }
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        const FCombatRoundSkill* Skill = FindSkill(Entry.Command.SkillId);
        if (!Skill || !Skill->bRemainAtDestination || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        const int32* Existing = Reserved.Find(Entry.Command.DestinationCoord);
        if (Existing && *Existing != Entry.UnitId)
        {
            OutError = RoundText(TEXT("최종 이동 칸이 다른 유닛의 복귀 칸 또는 이동 목적지와 겹칩니다."));
            return false;
        }
        Reserved.Add(Entry.Command.DestinationCoord, Entry.UnitId);
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
            if (Entry.OwnerSlot == Slot && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive() && !ValidateCommand(Entry.Command, OutError)) return false;
        }
        if (!ValidateDestinations(OutError)) return false;
    }
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
    if (bAllReady) LockPlans();
    PublishState();
    return true;
}

void ACombatRoundCoordinator::LockPlans()
{
    FText Error;
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        if (!ValidateCommand(Entry.Command, Error))
        {
            View.Message = Error;
            return;
        }
    }
    if (!ValidateDestinations(Error))
    {
        View.Message = Error;
        return;
    }
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        FCombatRoundUnitView& Entry = View.Units[Index];
        if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
        const FCombatRoundSkill& Skill = *FindSkill(Entry.Command.SkillId);
        if (Skill.ActionPointCost > 0) Entry.Unit->ConsumeActionPoint(Skill.ActionPointCost);
        if (Skill.SubActionPointCost > 0) Entry.Unit->ConsumeSubActionPoint(Skill.SubActionPointCost);
        FActionRuntime& Action = Actions[Index];
        Action.OriginalLocation = Entry.Unit->GetActorLocation();
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
    View.Phase = ECombatRoundPhase::Resolving;
    View.Message = RoundText(TEXT("계획 잠금 완료. 속도차에 따라 행동을 실행합니다."));
    Accumulator = 0.0;
    SimulationTime = 0.0;
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
        if (bAllReady) LockPlans();
        return;
    }

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
        Unit->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        return true;
    }
    const FVector Delta = Difference.GetClampedToMaxSize(Speed * StepSeconds);
    FHitResult Hit;
    Unit->SetActorLocation(Current + Delta, true, &Hit);
    if (!Difference.IsNearlyZero()) Unit->SetActorRotation(Difference.Rotation());
    if (StepSeconds > 0.f) Unit->GetCharacterMovement()->Velocity = (Unit->GetActorLocation() - Current) / StepSeconds;
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
    const FCombatRoundSkill* Skill = FindSkill(Entry.Command.SkillId);
    if (!Skill)
    {
        StartReturn(Index, true, RoundText(TEXT("스킬 정의 누락")));
        return;
    }
    if (Entry.ActionPhase == ECombatRoundActionPhase::Waiting)
    {
        if (SimulationTime + 0.00001 < Entry.StartDelay) return;
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
            if (Skill->TargetLoss == ECombatRoundTargetLoss::NearestEnemy && Skill->Kind != ECombatRoundSkillKind::Guard)
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
            if (Skill->Approach == ECombatRoundApproach::Unit)
            {
                const FVector TowardSource = (Entry.Unit->GetActorLocation() - Action.AimLocation).GetSafeNormal2D();
                Destination = Action.AimLocation + TowardSource * FMath::Max(20.f, Skill->HitRange * 0.7f);
            }
            if (SimulationTime - Action.PhaseStarted > MovementTimeout)
            {
                StartReturn(Index, true, RoundText(TEXT("접근 시간 초과로 불발")));
                return;
            }
            if (!MoveUnitToward(Index, Destination, Skill->MoveSpeed, StepSeconds)) return;
            Action.PhaseStarted = SimulationTime;
            Entry.ActionPhase = ECombatRoundActionPhase::Casting;
            Entry.Status = RoundText(TEXT("시전 중"));
        }
        if (Entry.ActionPhase == ECombatRoundActionPhase::Casting)
        {
            FVector Facing = Action.AimLocation - Entry.Unit->GetActorLocation();
            Facing.Z = 0.f;
            if (!Facing.IsNearlyZero()) Entry.Unit->SetActorRotation(Facing.Rotation());
            if (SimulationTime + 0.00001 < Action.PhaseStarted + Skill->WindupSeconds) return;
            ReleaseSkill(Index, *Skill);
            return;
        }
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
        else if (!MoveUnitToward(Index, Action.OriginalLocation, Skill->MoveSpeed, StepSeconds))
        {
            return;
        }
        Entry.Unit->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        Entry.ActionPhase = ECombatRoundActionPhase::Complete;
        if (Action.bFailed) Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
    }
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
    const FCombatRoundSkill* Skill = FindSkill(Entry.Command.SkillId);
    if (Skill && Skill->bRemainAtDestination && !bFailed)
    {
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Entry.Command.DestinationCoord);
        if (Tile && CombatRoundRules::IsOwnTerritory(Entry.bEnemy, Tile->GridCoord))
        {
            Entry.Unit->SetCurrentTile(Tile);
            Entry.HomeCoord = Tile->GridCoord;
            Entry.ActionPhase = ECombatRoundActionPhase::Complete;
            return;
        }
    }
    if (FVector::DistSquared2D(Entry.Unit->GetActorLocation(), Action.OriginalLocation) <= 4.f)
    {
        Entry.ActionPhase = ECombatRoundActionPhase::Complete;
        if (bFailed) Entry.ActionPhase = ECombatRoundActionPhase::Cancelled;
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
    if (Skill.Kind == ECombatRoundSkillKind::Guard)
    {
        if (!IsValid(Target) || !Target->IsUnitAlive() || FVector::Dist2D(Entry.Unit->GetActorLocation(), Target->GetActorLocation()) > Skill.HitRange)
        {
            StartReturn(Index, true, RoundText(TEXT("엄호 대상 또는 거리 조건 불충족")));
            return;
        }
        View.Units[TargetIndex].Guard = FMath::Max(View.Units[TargetIndex].Guard, Skill.Power);
        StartReturn(Index, false, RoundText(TEXT("엄호 적용 완료")));
        return;
    }
    if (Skill.Kind == ECombatRoundSkillKind::Projectile)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACombatRoundProjectile* Projectile = GetWorld()->SpawnActor<ACombatRoundProjectile>(Entry.Unit->GetActorLocation(), FRotator::ZeroRotator, Params);
        if (!Projectile)
        {
            StartReturn(Index, true, RoundText(TEXT("투사체 생성 실패")));
            return;
        }
        Projectiles.Add(Projectile);
        Projectile->OnImpact.AddUObject(this, &ACombatRoundCoordinator::ApplyHit);
        Projectile->OnResolved.AddUObject(this, &ACombatRoundCoordinator::HandleProjectileResolved);
        Projectile->InitializeProjectile(Entry.Unit, Target, Action.AimLocation, Skill.ProjectileSpeed, Skill.Power, Skill.ProjectileRadius, Skill.ProjectileLifetime, Skill.bHoming, Skill.bTargetOnly);
        StartReturn(Index, false, RoundText(TEXT("발사 완료")));
        return;
    }
    bool bHit = false;
    if (Skill.Kind == ECombatRoundSkillKind::GroundAttack)
    {
        if (Skill.Approach == ECombatRoundApproach::None || FVector::Dist2D(Entry.Unit->GetActorLocation(), Action.AimLocation) <= Skill.HitRange)
        {
            for (const FCombatRoundUnitView& Candidate : View.Units)
            {
                if (Candidate.bEnemy == Entry.bEnemy || !IsValid(Candidate.Unit) || !Candidate.Unit->IsUnitAlive()) continue;
                if (FVector::Dist2D(Candidate.Unit->GetActorLocation(), Action.AimLocation) <= Skill.HitRange)
                {
                    ApplyHit(Entry.Unit, Candidate.Unit, Skill.Power);
                    bHit = true;
                }
            }
        }
    }
    else if (IsValid(Target) && Target->IsUnitAlive())
    {
        const FVector Offset = Target->GetActorLocation() - Entry.Unit->GetActorLocation();
        const double FacingDot = FVector::DotProduct(Entry.Unit->GetActorForwardVector(), Offset.GetSafeNormal2D());
        if (Offset.Size2D() <= Skill.HitRange && FacingDot >= 0.5)
        {
            ApplyHit(Entry.Unit, Target, Skill.Power);
            bHit = true;
        }
    }
    if (bHit) StartReturn(Index, false, RoundText(TEXT("타격 완료")));
    else StartReturn(Index, true, RoundText(TEXT("타격 시점의 실제 거리·범위에서 벗어남")));
}

void ACombatRoundCoordinator::ApplyHit(AUnitBase* Source, AUnitBase* Target, float Damage)
{
    if (!HasAuthority() || View.Phase != ECombatRoundPhase::Resolving || !IsValid(Source) || !IsValid(Target) || !Target->IsUnitAlive() || Source->GetTeam() == Target->GetTeam() || !FMath::IsFinite(Damage) || Damage < 0.f) return;
    const int32 Index = View.Units.IndexOfByPredicate([Target](const FCombatRoundUnitView& Entry) { return Entry.Unit == Target; });
    if (!View.Units.IsValidIndex(Index)) return;
    FCombatRoundUnitView& Entry = View.Units[Index];
    const float Absorbed = FMath::Min(Entry.Guard, Damage);
    Entry.Guard -= Absorbed;
    const float AppliedDamage = Damage - Absorbed;
    if (AppliedDamage > 0.f) UCombatEffectLibrary::ApplyDamageToUnit(Source, Target, UGE_Damage::StaticClass(), AppliedDamage);
    UE_LOG(LogTemp, Log, TEXT("[Round] Round=%d Time=%.2f Source=%d Target=%d Damage=%.1f Guard=%.1f / 서버 피격"), View.RoundNumber, SimulationTime, Source->UnitIndex, Target->UnitIndex, AppliedDamage, Absorbed);
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
    if (!bPlayersAlive && !bEnemiesAlive)
    {
        View.Phase = ECombatRoundPhase::Suspended;
        View.Message = RoundText(TEXT("양측 전멸. 정식 결과 정책 확정 전까지 보상·승패를 적용하지 않습니다."));
        PublishState();
        return;
    }
    PublishState();
    OnCombatFinished.Broadcast(Result);
}
