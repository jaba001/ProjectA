#include "Combat/Round/CombatRoundCoordinator.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/Round/CombatAIPlanning.h"
#include "Combat/Round/CombatPlanValidator.h"
#include "Combat/Round/CombatSkillPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Encounter/CombatArena.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "Unit/UnitDataRules.h"
#include "Unit/PlayerUnit.h"

bool ACombatRoundCoordinator::CanModifyDebugRoster(APlayerController* Controller, FText& OutError) const
{
    OutError = FText::GetEmpty();
    const APartyPlayerController* PartyController = Cast<APartyPlayerController>(Controller);
    if (!ACombatDebugGameMode::IsDebugWorld(GetWorld()) || !HasExecutionAuthority() || !IsValid(PartyController) || !PartyController->IsLocalController() || PartyController->GetWorld() != GetWorld() || PartyController->GetCombatManager() != CombatManager || GetParticipantSlot(Controller) <= 0 || !CombatManager->GetActionAuthority()->CanRegisterDebugUnit(PartyController))
    {
        OutError = FText::FromString(TEXT("참가 중인 독립 로컬 디버그 전투에서만 캐릭터를 추가할 수 있습니다."));
        return false;
    }
    if ((View.Phase != ECombatRoundPhase::Planning && View.Phase != ECombatRoundPhase::Finished) || bSAPMovementInProgress || PlanningMoveIndex != INDEX_NONE || !Projectiles.IsEmpty() || !ActiveEffects.IsEmpty())
    {
        OutError = FText::FromString(TEXT("모든 행동이 끝난 계획 단계 또는 전투 종료 후 캐릭터를 추가할 수 있습니다."));
        return false;
    }
    if (View.Units.Num() >= 8)
    {
        OutError = FText::FromString(TEXT("사망한 캐릭터를 포함하여 아군·적군 합계 8명까지 추가할 수 있습니다. 전투 초기화로 새 테스트를 시작하세요."));
        return false;
    }
    if (!IsValid(Arena) || !IsValid(Arena->Grid) || Actions.Num() != View.Units.Num() || CombatManager->GetRegisteredUnits().Num() != View.Units.Num())
    {
        OutError = FText::FromString(TEXT("전투장 또는 현재 캐릭터 등록 상태가 올바르지 않습니다."));
        return false;
    }
    return true;
}

ACombatGridTile* ACombatRoundCoordinator::FindDebugSpawnTile(bool bEnemy) const
{
    if (!ACombatDebugGameMode::IsDebugWorld(GetWorld()) || !IsValid(Arena) || !IsValid(Arena->Grid)) return nullptr;
    const CombatPlanValidation::FState State = BuildPlanningState();
    const ETileTerritory Territory = bEnemy ? ETileTerritory::Enemy : ETileTerritory::Player;
    for (int32 Y = bEnemy ? 2 : 0; Y < (bEnemy ? 4 : 2); ++Y)
    {
        for (int32 X = 0; X < 4; ++X)
        {
            ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(FIntPoint(X, Y));
            if (IsValid(Tile) && Tile->GetWorld() == GetWorld() && Tile->GetTerritory() == Territory && CombatRoundRules::IsOwnTerritory(bEnemy, Tile->GridCoord) && !Tile->GetOccupyingUnit() && !CombatPlanValidation::IsReservedByOther(State, INDEX_NONE, Tile->GridCoord)) return Tile;
        }
    }
    return nullptr;
}

bool ACombatRoundCoordinator::CanAddDebugUnit(APlayerController* Controller, bool bEnemy, FText& OutError) const
{
    if (!CanModifyDebugRoster(Controller, OutError)) return false;
    if (!FindDebugSpawnTile(bEnemy))
    {
        OutError = FText::FromString(bEnemy ? TEXT("적군 진영에 점유·이동 예약이 없는 빈 칸이 없습니다.") : TEXT("아군 진영에 점유·이동 예약이 없는 빈 칸이 없습니다."));
        return false;
    }
    return true;
}

bool ACombatRoundCoordinator::AddDebugUnit(APlayerController* Controller, AUnitBase* Unit, FText& OutError)
{
    if (!CanModifyDebugRoster(Controller, OutError)) return false;
    if (!IsValid(Unit) || Unit->IsActorBeingDestroyed() || Unit->GetWorld() != GetWorld() || !Unit->IsUnitAlive() || !Unit->GetAttributeSet() || !Unit->GetAbilitySystemComponent() || !Unit->GetCharacterMovement() || !Unit->GetCapsuleComponent() || (Unit->GetTeam() != ETeam::Player && Unit->GetTeam() != ETeam::Enemy) || View.Units.ContainsByPredicate([Unit](const FCombatRoundUnitView& Entry) { return Entry.Unit == Unit; }) || CombatManager->GetRegisteredUnits().Contains(Unit))
    {
        OutError = FText::FromString(TEXT("추가할 캐릭터의 생존·속성·등록 상태가 올바르지 않습니다."));
        return false;
    }
    const bool bEnemy = Unit->GetTeam() == ETeam::Enemy;
    const APlayerUnit* Player = Cast<APlayerUnit>(Unit);
    if (!bEnemy && (!Player || Player->IsServerAIControlled()))
    {
        OutError = FText::FromString(TEXT("추가할 아군은 로컬 플레이어가 조작하는 캐릭터여야 합니다."));
        return false;
    }
    ACombatGridTile* Tile = Unit->GetCurrentTile();
    const ETileTerritory Territory = bEnemy ? ETileTerritory::Enemy : ETileTerritory::Player;
    if (!IsValid(Tile) || Tile->GetWorld() != GetWorld() || Arena->Grid->GetTileAtCoord(Tile->GridCoord) != Tile || Tile->GetOccupyingUnit() != Unit || Tile->GetTerritory() != Territory || !CombatRoundRules::IsOwnTerritory(bEnemy, Tile->GridCoord) || CombatPlanValidation::IsReservedByOther(BuildPlanningState(), INDEX_NONE, Tile->GridCoord))
    {
        OutError = FText::FromString(TEXT("추가할 캐릭터의 배치 칸이 잘못되었거나 기존 이동 예약과 겹칩니다."));
        return false;
    }
    if (!UnitDataRules::IsValidHealth(Unit->GetAttributeSet()->GetMaxHP(), Unit->GetAttributeSet()->GetHP()) || !UnitDataRules::IsValidActionPoints(Unit->GetMaxActionPoint(), Unit->GetMaxSubActionPoint()) || Unit->GetActorLocation().ContainsNaN() || Unit->GetActorRotation().ContainsNaN())
    {
        OutError = FText::FromString(TEXT("추가할 캐릭터의 체력·행동력 또는 위치 수치가 올바르지 않습니다."));
        return false;
    }
    TArray<FCombatRoundSkill> NewSkills;
    TMap<FName, USkillDefinitionDataAsset*> DefinitionsById;
    TArray<FName> AddedSkillIds;
    const auto ValidateLoadout = [&NewSkills, &DefinitionsById, &OutError](AUnitBase* Candidate, TArray<FName>* OutSkillIds)
    {
        if (!IsValid(Candidate))
        {
            OutError = FText::FromString(TEXT("기존 전투 캐릭터가 사라져 스킬 목록을 검증할 수 없습니다."));
            return false;
        }
        if (!UnitDataRules::ValidateSkills(Candidate->GetEquippedSkillDataAssets(), false, OutError)) return false;
        for (USkillDefinitionDataAsset* Definition : Candidate->GetEquippedSkillDataAssets())
        {
            FCombatRoundSkill Skill;
            if (!Definition->ResolveRoundSkill(Skill, OutError)) return false;
            if (USkillDefinitionDataAsset* const* Existing = DefinitionsById.Find(Skill.SkillId))
            {
                if (*Existing != Definition)
                {
                    OutError = FText::FromString(FString::Printf(TEXT("서로 다른 스킬의 식별자가 중복됩니다: %s"), *Skill.SkillId.ToString()));
                    return false;
                }
            }
            else
            {
                DefinitionsById.Add(Skill.SkillId, Definition);
                NewSkills.Add(Skill);
            }
            if (OutSkillIds) OutSkillIds->Add(Skill.SkillId);
        }
        return true;
    };
    int32 NewUnitId = 1;
    for (const FCombatRoundUnitView& Existing : View.Units)
    {
        if (!ValidateLoadout(Existing.Unit, nullptr)) return false;
        NewUnitId = FMath::Max(NewUnitId, Existing.UnitId + 1);
        if (View.Phase == ECombatRoundPhase::Planning && Existing.bHasMovePlan && IsValid(Existing.Unit) && Existing.Unit->IsUnitAlive())
        {
            TArray<FIntPoint> Path;
            if (!BuildPlanningMovePath(Existing.UnitId, Existing.MoveDestinationCoord, Path, OutError))
            {
                OutError = FText::FromString(TEXT("새 캐릭터가 기존 SAP 이동 경로를 막습니다. 해당 이동 계획을 변경하거나 취소한 뒤 추가하세요."));
                return false;
            }
        }
    }
    if (!ValidateLoadout(Unit, &AddedSkillIds)) return false;
    TArray<TObjectPtr<UObject>> PreparedVisualAssets = PreparedSkillVisualAssets;
    if (!CombatSkillPresentation::Prepare(GetWorld(), NewSkills, PreparedVisualAssets, OutError)) return false;
    if (!CombatManager->RegisterDebugUnit(Unit, Cast<APartyPlayerController>(Controller)))
    {
        OutError = FText::FromString(TEXT("새 캐릭터의 전투 식별자 또는 조작 권한을 등록하지 못했습니다."));
        return false;
    }
    // All fallible checks precede registration so a failed request leaves the current battle untouched.
    // 실패한 요청이 현재 전투를 바꾸지 않도록 등록 전에 실패 가능한 검사를 모두 마칩니다.
    Unit->OnTurnEnd();
    Unit->GetCharacterMovement()->StopMovementImmediately();
    Unit->GetCharacterMovement()->DisableMovement();
    Unit->GetCharacterMovement()->SetComponentTickEnabled(false);
    Unit->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Unit->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Unit->UnitIndex = NewUnitId;
    Unit->ResetActionPoint();
    Unit->ResetSubActionPoint();
    FCombatRoundUnitView Added;
    Added.UnitId = NewUnitId;
    Added.Unit = Unit;
    Added.bEnemy = bEnemy;
    Added.OwnerSlot = bEnemy ? 0 : GetParticipantSlot(Controller);
    Added.HomeCoord = Tile->GridCoord;
    Added.Speed = Unit->GetCombatSpeed();
    Added.HP = Unit->GetAttributeSet()->GetHP();
    Added.SkillIds = MoveTemp(AddedSkillIds);
    Added.Command.UnitId = NewUnitId;
    Added.Command.TargetCoord = Added.HomeCoord;
    Added.Command.DestinationCoord = Added.HomeCoord;
    Added.MoveDestinationCoord = Added.HomeCoord;
    Added.Status = FText::FromString(TEXT("스킬 미선택 · 턴 넘기기"));
    const int32 AddedIndex = View.Units.Add(MoveTemp(Added));
    Actions.AddDefaulted();
    Actions[AddedIndex].OriginalLocation = Unit->GetActorLocation();
    Actions[AddedIndex].OriginalRotation = Unit->GetActorRotation();
    Skills = MoveTemp(NewSkills);
    PreparedSkillVisualAssets = MoveTemp(PreparedVisualAssets);
    const bool bFinished = View.Phase == ECombatRoundPhase::Finished;
    const bool bPlayersAlive = View.Units.ContainsByPredicate([](const FCombatRoundUnitView& Entry) { return !Entry.bEnemy && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive(); });
    const bool bEnemiesAlive = View.Units.ContainsByPredicate([](const FCombatRoundUnitView& Entry) { return Entry.bEnemy && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive(); });
    if (bFinished && bPlayersAlive && bEnemiesAlive)
    {
        CombatManager->ClearDebugCombatResult();
        BeginPlanning();
        View.Message = FText::FromString(TEXT("캐릭터 추가 완료 · 기존 스킬·장비 유지 · 다음 라운드를 계획하세요."));
    }
    else
    {
        float HighestSpeed = 0.f;
        for (FCombatRoundUnitView& Entry : View.Units)
        {
            if (IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive())
            {
                Entry.Speed = Entry.Unit->GetCombatSpeed();
                HighestSpeed = FMath::Max(HighestSpeed, Entry.Speed);
            }
        }
        for (FCombatRoundUnitView& Entry : View.Units)
        {
            Entry.StartDelay = CombatRoundRules::StartDelay(HighestSpeed, Entry.Speed);
            if (Entry.OwnerSlot > 0 && IsValid(Entry.Unit) && Entry.Unit->IsUnitAlive()) Entry.bReady = false;
        }
        if (bEnemy && !bFinished)
        {
            FCombatRoundUnitView& Entry = View.Units[AddedIndex];
            Entry.Command = CombatAIPlanning::ChooseCommand(View, BuildPlanningState(), AddedIndex, [this, AddedIndex](const FCombatRoundCommand& Command)
            {
                FText Error;
                if (!ValidateCommand(Command, Error)) return false;
                // Preview the new AI reservation without leaving it behind when an existing SAP route becomes blocked.
                // 기존 SAP 경로가 막히면 새 AI 예약을 남기지 않도록 임시 명령으로 확인합니다.
                TGuardValue<FCombatRoundCommand> CommandGuard(View.Units[AddedIndex].Command, Command);
                for (const FCombatRoundUnitView& Existing : View.Units)
                {
                    if (!Existing.bHasMovePlan || !IsValid(Existing.Unit) || !Existing.Unit->IsUnitAlive()) continue;
                    TArray<FIntPoint> Path;
                    if (!BuildPlanningMovePath(Existing.UnitId, Existing.MoveDestinationCoord, Path, Error)) return false;
                }
                return true;
            });
            Entry.bReady = true;
            Entry.Status = FText::FromString(TEXT("AI 계획 고정"));
        }
        ++View.PlanRevision;
        bLockRetryBlocked = false;
        View.Message = FText::FromString(bFinished ? TEXT("캐릭터 추가 완료 · 양 진영에 생존 캐릭터가 있어야 전투가 재개됩니다.") : TEXT("캐릭터 추가 완료 · 기존 계획 유지 · 아군 준비 완료를 다시 눌러주세요."));
    }
    Unit->ForceNetUpdate();
    CombatManager->RefreshTileProtectedByFront();
    PublishState();
    return true;
}

ACombatGridTile* ACombatRoundCoordinator::FindDebugReviveTile(int32 UnitIndex) const
{
    if (!View.Units.IsValidIndex(UnitIndex) || !IsValid(Arena) || !IsValid(Arena->Grid)) return nullptr;
    TSet<FIntPoint> ReservedMovePath;
    if (View.Phase == ECombatRoundPhase::Planning)
    {
        for (const FCombatRoundUnitView& Entry : View.Units)
        {
            if (!Entry.bHasMovePlan || !IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive()) continue;
            TArray<FIntPoint> Path;
            FText Error;
            if (BuildPlanningMovePath(Entry.UnitId, Entry.MoveDestinationCoord, Path, Error))
            {
                for (FIntPoint Coord : Path) ReservedMovePath.Add(Coord);
            }
        }
    }
    const auto Available = [this, UnitIndex, &ReservedMovePath](ACombatGridTile* Tile)
    {
        return IsValid(Tile) && Tile->GetWorld() == GetWorld() && Tile->GetTerritory() == ETileTerritory::Player && CombatRoundRules::IsOwnTerritory(false, Tile->GridCoord) && !Tile->GetOccupyingUnit() && !IsDestinationReservedByOther(UnitIndex, Tile->GridCoord) && !ReservedMovePath.Contains(Tile->GridCoord);
    };
    ACombatGridTile* Home = Arena->Grid->GetTileAtCoord(View.Units[UnitIndex].HomeCoord);
    if (Available(Home)) return Home;
    // Prefer home, then scan free allied tiles without displacing units or blocking their valid SAP paths.
    // 원래 복귀 칸을 우선하고 다른 유닛이나 유효한 SAP 경로를 막지 않는 빈 아군 칸을 탐색합니다.
    for (int32 Y = 0; Y < 2; ++Y)
    {
        for (int32 X = 0; X < 4; ++X)
        {
            ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(FIntPoint(X, Y));
            if (Available(Tile)) return Tile;
        }
    }
    return nullptr;
}

bool ACombatRoundCoordinator::CanReviveDebugUnit(APlayerController* Controller, int32 UnitId, FText& OutError) const
{
    OutError = FText::GetEmpty();
    if (!ACombatDebugGameMode::IsDebugWorld(GetWorld()) || !HasExecutionAuthority() || !IsValid(Controller) || !Controller->IsLocalController() || Controller->GetWorld() != GetWorld())
    {
        OutError = FText::FromString(TEXT("독립 로컬 디버그 전투에서만 부활할 수 있습니다."));
        return false;
    }
    if ((View.Phase != ECombatRoundPhase::Planning && View.Phase != ECombatRoundPhase::Finished) || bSAPMovementInProgress || PlanningMoveIndex != INDEX_NONE || !Projectiles.IsEmpty() || !ActiveEffects.IsEmpty())
    {
        OutError = FText::FromString(TEXT("모든 행동이 끝난 계획 단계 또는 전투 종료 후 부활할 수 있습니다."));
        return false;
    }
    const int32 Index = FindUnitIndex(UnitId);
    const int32 OwnerSlot = GetParticipantSlot(Controller);
    if (!View.Units.IsValidIndex(Index) || !Actions.IsValidIndex(Index) || OwnerSlot <= 0)
    {
        OutError = FText::FromString(TEXT("부활할 아군을 선택하세요."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    if (!IsValid(Entry.Unit) || Entry.bEnemy || Entry.OwnerSlot != OwnerSlot || !CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), Entry.Unit))
    {
        OutError = FText::FromString(TEXT("자신이 조작하는 아군만 부활할 수 있습니다."));
        return false;
    }
    if (Entry.Unit->IsUnitAlive())
    {
        OutError = FText::FromString(TEXT("생존 중인 아군입니다. 사망 후 부활할 수 있습니다."));
        return false;
    }
    if (!FindDebugReviveTile(Index))
    {
        OutError = FText::FromString(TEXT("아군 진영에 점유·이동 예약·기존 SAP 경로를 피할 수 있는 빈 칸이 없습니다."));
        return false;
    }
    return true;
}

bool ACombatRoundCoordinator::ReviveDebugUnit(APlayerController* Controller, int32 UnitId, FText& OutError)
{
    if (!CanReviveDebugUnit(Controller, UnitId, OutError)) return false;
    const int32 Index = FindUnitIndex(UnitId);
    FCombatRoundUnitView& Entry = View.Units[Index];
    ACombatGridTile* Tile = FindDebugReviveTile(Index);
    if (!Entry.Unit->ReviveForDebug(Tile))
    {
        OutError = FText::FromString(TEXT("유닛의 부활 상태를 복구하지 못했습니다."));
        return false;
    }
    Entry.HomeCoord = Tile->GridCoord;
    const bool bFinished = View.Phase == ECombatRoundPhase::Finished;
    const bool bHasLivingEnemy = View.Units.ContainsByPredicate([](const FCombatRoundUnitView& Unit) { return Unit.bEnemy && IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive(); });
    // Preserve the combat identity and loadout caches; only reopen planning after a settled result.
    // 전투 식별자와 장착 캐시를 보존하며 결과가 확정된 뒤에만 계획 단계를 다시 엽니다.
    if (bFinished && bHasLivingEnemy)
    {
        CombatManager->ClearDebugCombatResult();
        BeginPlanning();
        View.Message = FText::FromString(TEXT("아군 부활 완료 · 스킬·장비 유지 · 다음 라운드를 계획하세요."));
    }
    else
    {
        Entry.Speed = Entry.Unit->GetCombatSpeed();
        float HighestSpeed = 0.f;
        for (const FCombatRoundUnitView& Unit : View.Units)
        {
            if (IsValid(Unit.Unit) && Unit.Unit->IsUnitAlive()) HighestSpeed = FMath::Max(HighestSpeed, Unit.Speed);
        }
        for (FCombatRoundUnitView& Unit : View.Units) Unit.StartDelay = CombatRoundRules::StartDelay(HighestSpeed, Unit.Speed);
        ResetDebugUnitPlan(Index, FText::FromString(bFinished ? TEXT("아군 부활 완료 · 생존한 적이 없습니다. 전투 초기화로 새 테스트를 시작하세요.") : TEXT("아군 부활 완료 · HP·AP·SAP 복구 · 스킬·장비를 유지합니다.")));
    }
    CombatManager->RefreshTileProtectedByFront();
    PublishState();
    return true;
}

bool ACombatRoundCoordinator::CanEditDebugUnit(APlayerController* Controller, int32 UnitId, FText& OutError) const
{
    OutError = FText::GetEmpty();
    if (!ACombatDebugGameMode::IsDebugWorld(GetWorld()) || !HasExecutionAuthority() || !IsValid(Controller) || !Controller->IsLocalController() || Controller->GetWorld() != GetWorld())
    {
        OutError = FText::FromString(TEXT("독립 디버그 전투에서만 장착을 변경할 수 있습니다."));
        return false;
    }
    if (View.Phase != ECombatRoundPhase::Planning || bSAPMovementInProgress || PlanningMoveIndex != INDEX_NONE || !Projectiles.IsEmpty() || !ActiveEffects.IsEmpty())
    {
        OutError = FText::FromString(TEXT("계획 단계에서 이동과 효과가 모두 끝난 뒤 변경하세요."));
        return false;
    }
    const int32 Index = FindUnitIndex(UnitId);
    const int32 OwnerSlot = GetParticipantSlot(Controller);
    if (!View.Units.IsValidIndex(Index) || OwnerSlot <= 0)
    {
        OutError = FText::FromString(TEXT("변경할 소유 유닛을 선택하세요."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive() || Entry.bEnemy || Entry.OwnerSlot != OwnerSlot || !CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), Entry.Unit))
    {
        OutError = FText::FromString(TEXT("생존한 자신의 아군만 변경할 수 있습니다."));
        return false;
    }
    if (!Entry.Unit->GetVelocity().IsNearlyZero() || (Entry.Unit->GetCharacterMovement() && !Entry.Unit->GetCharacterMovement()->Velocity.IsNearlyZero()))
    {
        OutError = FText::FromString(TEXT("유닛이 정지한 뒤 변경하세요."));
        return false;
    }
    return true;
}

bool ACombatRoundCoordinator::SetDebugUnitSkills(APlayerController* Controller, int32 UnitId, const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Definitions, FText& OutError)
{
    if (!CanEditDebugUnit(Controller, UnitId, OutError) || !UnitDataRules::ValidateSkills(Definitions, false, OutError)) return false;
    const int32 ChangedIndex = FindUnitIndex(UnitId);
    if (View.Units[ChangedIndex].Unit->GetEquippedSkillDataAssets() == Definitions) return true;
    TArray<FCombatRoundSkill> NewSkills;
    TArray<TArray<FName>> NewUnitSkillIds;
    TMap<FName, USkillDefinitionDataAsset*> DefinitionsById;
    NewUnitSkillIds.SetNum(View.Units.Num());
    // Validate every replacement profile before mutating either the unit or replicated planning state.
    // 유닛 또는 복제되는 계획 상태를 변경하기 전에 모든 교체 프로필을 검증합니다.
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const AUnitBase* Unit = View.Units[Index].Unit;
        if (!IsValid(Unit))
        {
            OutError = FText::FromString(TEXT("전투 유닛이 사라져 스킬 목록을 갱신할 수 없습니다."));
            return false;
        }
        const TArray<TObjectPtr<USkillDefinitionDataAsset>>& UnitSkills = Index == ChangedIndex ? Definitions : Unit->GetEquippedSkillDataAssets();
        if (!UnitDataRules::ValidateSkills(UnitSkills, false, OutError)) return false;
        for (USkillDefinitionDataAsset* Definition : UnitSkills)
        {
            FCombatRoundSkill Skill;
            if (!Definition->ResolveRoundSkill(Skill, OutError)) return false;
            if (USkillDefinitionDataAsset* const* Existing = DefinitionsById.Find(Skill.SkillId))
            {
                if (*Existing != Definition)
                {
                    OutError = FText::FromString(FString::Printf(TEXT("서로 다른 스킬의 식별자가 중복됩니다: %s"), *Skill.SkillId.ToString()));
                    return false;
                }
            }
            else
            {
                DefinitionsById.Add(Skill.SkillId, Definition);
                NewSkills.Add(Skill);
            }
            NewUnitSkillIds[Index].Add(Skill.SkillId);
        }
    }
    TArray<TObjectPtr<UObject>> PreparedVisualAssets = PreparedSkillVisualAssets;
    if (!CombatSkillPresentation::Prepare(GetWorld(), NewSkills, PreparedVisualAssets, OutError)) return false;
    View.Units[ChangedIndex].Unit->SetDebugEquippedSkills(Definitions);
    Skills = MoveTemp(NewSkills);
    PreparedSkillVisualAssets = MoveTemp(PreparedVisualAssets);
    for (int32 Index = 0; Index < View.Units.Num(); ++Index) View.Units[Index].SkillIds = MoveTemp(NewUnitSkillIds[Index]);
    ResetDebugUnitPlan(ChangedIndex, FText::FromString(TEXT("스킬 장착이 변경되어 해당 유닛의 계획과 자신의 준비가 해제되었습니다.")));
    return true;
}

void ACombatRoundCoordinator::NotifyDebugEquipmentChanged(APlayerController* Controller, int32 UnitId)
{
    FText Error;
    if (!CanEditDebugUnit(Controller, UnitId, Error)) return;
    ResetDebugUnitPlan(FindUnitIndex(UnitId), FText::FromString(TEXT("장비가 변경되어 해당 유닛의 계획과 자신의 준비가 해제되었습니다.")));
}

void ACombatRoundCoordinator::ResetDebugUnitPlan(int32 UnitIndex, const FText& Message)
{
    FCombatRoundUnitView& Entry = View.Units[UnitIndex];
    Entry.Command = FCombatRoundCommand();
    Entry.Command.UnitId = Entry.UnitId;
    Entry.Command.TargetCoord = Entry.HomeCoord;
    Entry.Command.DestinationCoord = Entry.HomeCoord;
    Entry.bHasMovePlan = false;
    Entry.MoveDestinationCoord = Entry.HomeCoord;
    Entry.ActionPhase = ECombatRoundActionPhase::Planned;
    Entry.Status = FText::FromString(TEXT("스킬 미선택 · 턴 넘기기"));
    Actions[UnitIndex] = FActionRuntime();
    Actions[UnitIndex].OriginalLocation = Entry.Unit->GetActorLocation();
    ClearOwnerReady(Entry.OwnerSlot);
    ++View.PlanRevision;
    View.Message = Message;
    bLockRetryBlocked = false;
    PublishState();
}
