#include "Combat/Round/CombatRoundCoordinator.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/PlayerUnit.h"
#include "Unit/CharacterAppearanceComponent.h"

bool ACombatRoundCoordinator::CapturePlanningCheckpoint(FCombatCheckpointData& OutCheckpoint, FText& OutError) const
{
    OutError = FText::FromString(TEXT("계획 단계의 전투 상태와 Run 소유권을 저장할 수 없습니다."));
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const UCombatActionAuthority* Authority = IsValid(CombatManager) ? CombatManager->GetActionAuthority() : nullptr;
    if (!HasAuthority() || View.Phase != ECombatRoundPhase::Planning || bSAPMovementInProgress || !Projectiles.IsEmpty() || !Run || Run->GetPhase() != ERunPhase::Combat || !Authority || !View.CombatId.IsValid() || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Run->GetRunIdentity(), &Authority->GetRunIdentity(), 0)) return false;
    if (Run->GetRunIdentity().Origin == ERunIdentityOrigin::LegacyOffline && GetNetMode() != NM_Standalone) return false;
    const FCombatCheckpointData& Previous = Run->GetCombatCheckpoint();
    if (Previous.Revision >= MAX_int64 - 1) return false;
    FCombatCheckpointData Candidate;
    Candidate.SchemaVersion = UCombatCheckpointLibrary::CurrentSchemaVersion;
    Candidate.ContentVersion = UCombatCheckpointLibrary::CurrentContentVersion;
    Candidate.AttemptId = Run->HasCombatCheckpoint() ? Previous.AttemptId : View.CombatId;
    Candidate.Revision = Previous.Revision + 1;
    Candidate.Identity = Run->GetRunIdentity();
    Candidate.NodeId = Run->GetCurrentNodeId();
    Candidate.EncounterId = Run->GetCurrentEncounterId();
    Candidate.RoundNumber = View.RoundNumber;
    Candidate.PlanRevision = View.PlanRevision;
    if (Run->HasCombatCheckpoint())
    {
        Candidate.bHasOpponentSnapshot = Previous.bHasOpponentSnapshot;
        Candidate.OpponentSnapshot = Previous.OpponentSnapshot;
        Candidate.OpponentCatalog = Previous.OpponentCatalog;
    }
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        AUnitBase* Unit = Entry.Unit;
        if (!IsValid(Unit) || !Unit->GetAttributeSet()) return false;
        const UAS_Unit* Attributes = Unit->GetAttributeSet();
        FCombatCheckpointUnit& Saved = Candidate.Units.AddDefaulted_GetRef();
        Saved.UnitId = Authority->GetUnitId(Unit);
        Saved.RoundUnitId = Entry.UnitId;
        Saved.Team = Unit->GetTeam();
        Saved.UnitClass = FSoftObjectPath(Unit->GetClass());
        Saved.CharacterName = Unit->RuntimeCharacterName;
        if (Unit->CharacterAppearance) Saved.Appearance = Unit->CharacterAppearance->Selection;
        Saved.bDead = !Unit->IsUnitAlive();
        Saved.HP = Saved.bDead ? 0.0f : Attributes->GetHP();
        Saved.MaxHP = Attributes->GetMaxHP();
        Saved.Strength = Attributes->GetStrength();
        Saved.Dexterity = Attributes->GetDexterity();
        Saved.Intelligence = Attributes->GetIntelligence();
        Saved.AP = Unit->GetCurrentActionPoint();
        Saved.MaxAP = Unit->GetMaxActionPoint();
        Saved.SubAP = Unit->GetCurrentSubActionPoint();
        Saved.MaxSubAP = Unit->GetMaxSubActionPoint();
        Saved.MoveRange = Unit->GetMoveRange();
        Saved.HealingItemCount = Unit->HealingItemCount;
        Saved.HealingItemAmount = Unit->HealingItemAmount;
        Saved.bHasTile = Unit->GetCurrentTile() != nullptr;
        Saved.GridCoord = Entry.HomeCoord;
        Saved.Transform = Unit->GetActorTransform();
        Saved.DefaultAttackAbility = FSoftObjectPath(Unit->GetDefaultAttackAbilityClass().Get());
        if (Saved.bHasTile && (!IsValid(Arena) || !IsValid(Arena->Grid) || Unit->GetCurrentTile()->GridCoord != Entry.HomeCoord || Arena->Grid->GetTileAtCoord(Entry.HomeCoord) != Unit->GetCurrentTile() || Unit->GetCurrentTile()->GetOccupyingUnit() != Unit)) return false;
        for (USkillDefinitionDataAsset* Skill : Unit->GetEquippedSkillDataAssets())
        {
            if (!IsValid(Skill)) return false;
            Saved.Skills.Add(FSoftObjectPath(Skill));
        }
        if (Saved.Team == ETeam::Player)
        {
            Saved.CharacterId = Authority->GetCharacterId(Unit);
            Saved.OwnerAccountId = Authority->GetOwnerAccountId(Unit);
            Saved.PartySlot = Authority->GetPartySlot(Unit);
            const FRunPartyMember* Member = Run->GetPartyMembers().FindByPredicate([&Saved](const FRunPartyMember& PartyMember) { return PartyMember.bCreated && PartyMember.SlotIndex == Saved.PartySlot && PartyMember.CharacterId == Saved.CharacterId && PartyMember.OwnerAccountId == Saved.OwnerAccountId; });
            const APlayerUnit* Player = Cast<APlayerUnit>(Unit);
            if (!Member || !Player) return false;
            Saved.PartyControlMode = Player->GetPartyControlMode();
            if (Saved.CharacterName.IsEmpty()) Saved.CharacterName = Member->CharacterName;
        }
        if (Saved.CharacterName.IsEmpty()) Saved.CharacterName = FText::FromString(Unit->GetClass()->GetName());
        FCombatCheckpointRoundPlan& Plan = Candidate.RoundPlans.AddDefaulted_GetRef();
        Plan.UnitId = Entry.UnitId;
        Plan.Command = Saved.bDead ? FCombatRoundCommand() : Entry.Command;
        Plan.Command.UnitId = Entry.UnitId;
        Plan.bHasMovePlan = !Saved.bDead && Entry.bHasMovePlan;
        Plan.MoveDestinationCoord = Plan.bHasMovePlan ? Entry.MoveDestinationCoord : Entry.HomeCoord;
        Plan.bReady = Saved.bDead || Entry.bReady;
    }
    if (!UCombatCheckpointLibrary::Validate(Candidate, Run->GetPartyMembers(), OutError)) return false;
    OutCheckpoint = MoveTemp(Candidate);
    OutError = FText::GetEmpty();
    return true;
}

bool ACombatRoundCoordinator::RestorePlanningCheckpoint(const FCombatCheckpointData& Checkpoint, FText& OutError)
{
    OutError = FText::FromString(TEXT("저장된 준비 계획과 복원된 전투 유닛이 일치하지 않습니다."));
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    const UCombatActionAuthority* Authority = IsValid(CombatManager) ? CombatManager->GetActionAuthority() : nullptr;
    if (!HasAuthority() || !Run || Run->GetPhase() != ERunPhase::Combat || !Authority || Checkpoint.SchemaVersion != UCombatCheckpointLibrary::CurrentSchemaVersion || View.Units.Num() != Checkpoint.Units.Num() || !IsValid(Arena) || !IsValid(Arena->Grid) || !Projectiles.IsEmpty() || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Run->GetRunIdentity(), &Checkpoint.Identity, 0)) return false;
    if (Checkpoint.Identity.Origin == ERunIdentityOrigin::LegacyOffline && GetNetMode() != NM_Standalone) return false;
    if (!UCombatCheckpointLibrary::Validate(Checkpoint, Run->GetPartyMembers(), OutError)) return false;
    OutError = FText::FromString(TEXT("저장된 유닛의 소유권·체력·행동력·배치를 복원하지 못했습니다."));
    FCombatRoundView Restored = View;
    Restored.RoundNumber = Checkpoint.RoundNumber;
    Restored.PlanRevision = Checkpoint.PlanRevision;
    Restored.Phase = ECombatRoundPhase::Planning;
    Restored.ElapsedSeconds = 0.0f;
    Restored.PendingProjectiles = 0;
    float HighestSpeed = 0.0f;
    for (int32 Index = 0; Index < Restored.Units.Num(); ++Index)
    {
        FCombatRoundUnitView& Entry = Restored.Units[Index];
        const FCombatCheckpointUnit& Saved = Checkpoint.Units[Index];
        AUnitBase* Unit = Entry.Unit;
        const FCombatCheckpointRoundPlan* Plan = Checkpoint.RoundPlans.FindByPredicate([&Saved](const FCombatCheckpointRoundPlan& Candidate) { return Candidate.UnitId == Saved.RoundUnitId; });
        if (!IsValid(Unit) || !Unit->GetAttributeSet() || !Plan || FSoftObjectPath(Unit->GetClass()) != Saved.UnitClass || Unit->GetTeam() != Saved.Team || Unit->IsUnitAlive() == Saved.bDead || !FMath::IsNearlyEqual(Unit->GetAttributeSet()->GetHP(), Saved.HP) || Unit->GetCurrentActionPoint() != Saved.AP || Unit->GetCurrentSubActionPoint() != Saved.SubAP) return false;
        if (Saved.Team == ETeam::Player && (Authority->GetPartySlot(Unit) != Saved.PartySlot || Authority->GetCharacterId(Unit) != Saved.CharacterId || Authority->GetOwnerAccountId(Unit) != Saved.OwnerAccountId)) return false;
        if (!Unit->CharacterAppearance || !FCharacterAppearanceSelection::StaticStruct()->CompareScriptStruct(&Unit->CharacterAppearance->Selection, &Saved.Appearance, 0)) return false;
        if (Saved.bHasTile && (Unit->GetCurrentTile() != Arena->Grid->GetTileAtCoord(Saved.GridCoord) || !Unit->GetCurrentTile() || Unit->GetCurrentTile()->GetOccupyingUnit() != Unit)) return false;
        if (!Saved.bHasTile && Unit->GetCurrentTile()) return false;
        Entry.UnitId = Saved.RoundUnitId;
        Entry.HomeCoord = Saved.GridCoord;
        Entry.HP = Saved.HP;
        Entry.Speed = Unit->GetCombatSpeed();
        Entry.Command = Plan->Command;
        Entry.Command.SkillId = UCombatCheckpointLibrary::ResolveSavedSkillId(Entry.Command.SkillId);
        Entry.bHasMovePlan = Plan->bHasMovePlan;
        Entry.MoveDestinationCoord = Plan->MoveDestinationCoord;
        // Explicit resume control changes retain saved commands and allow current AI owners to execute them.
        // 명시적 재개의 조작 모드 변경은 저장 명령을 유지하고 현재 AI 조작 유닛이 실행하게 합니다.
        Entry.bReady = Saved.bDead || Entry.OwnerSlot == 0 || Plan->bReady;
        Entry.ActionPhase = Saved.bDead ? ECombatRoundActionPhase::Cancelled : ECombatRoundActionPhase::Planned;
        Entry.Status = FText::FromString(Saved.bDead ? TEXT("사망") : Entry.bReady ? TEXT("저장된 준비 완료 복구") : TEXT("저장된 계획 복구"));
        if (!Saved.bDead) HighestSpeed = FMath::Max(HighestSpeed, Entry.Speed);
    }
    for (FCombatRoundUnitView& Entry : Restored.Units) Entry.StartDelay = CombatRoundRules::StartDelay(HighestSpeed, Entry.Speed);
    const FCombatRoundView PreviousView = View;
    View = MoveTemp(Restored);
    if (!ValidateDestinations(OutError))
    {
        View = PreviousView;
        return false;
    }
    for (const FCombatRoundUnitView& Entry : View.Units)
    {
        if (!Entry.Unit->IsUnitAlive()) continue;
        TArray<FIntPoint> Path;
        if ((Entry.bReady && !ValidateCommand(Entry.Command, OutError)) || (Entry.bHasMovePlan && !BuildPlanningMovePath(Entry.UnitId, Entry.MoveDestinationCoord, Path, OutError)))
        {
            View = PreviousView;
            return false;
        }
    }
    Actions.Reset();
    Actions.SetNum(View.Units.Num());
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        AUnitBase* Unit = View.Units[Index].Unit;
        Unit->UnitIndex = View.Units[Index].UnitId;
        Actions[Index].OriginalLocation = Unit->GetActorLocation();
        Actions[Index].OriginalRotation = Unit->GetActorRotation();
        Unit->ForceNetUpdate();
    }
    Accumulator = SimulationTime = MontageClock = 0.0;
    bSAPMovementInProgress = false;
    bSAPMovementFailed = false;
    bLockRetryBlocked = false;
    PlanningMoveIndex = INDEX_NONE;
    NextMoveIndex = PlanningMoveStep = 0;
    PlanningMoveElapsed = 0.0;
    PlanningMovePath.Reset();
    View.Message = FText::FromString(TEXT("마지막으로 저장한 준비 상태를 복구했습니다."));
    OutError = FText::GetEmpty();
    PublishState();
    return true;
}
