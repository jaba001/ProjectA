#include "Combat/Round/CombatPlanValidator.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

bool CombatPlanValidation::ValidateCheckpointPlans(const FCombatCheckpointData& Checkpoint, FText& OutError)
{
    OutError = NSLOCTEXT("CombatCheckpoint", "RoundPlans", "저장된 준비 계획·대상·이동 예약·비용이 현재 전투 상태와 일치하지 않습니다.");
    FState State;
    for (int32 X = 0; X < 4; ++X)
    {
        for (int32 Y = 0; Y < 4; ++Y) State.Tiles.Add(FIntPoint(X, Y), INDEX_NONE);
    }
    TSet<int32> PlannedUnits;
    for (const FCombatCheckpointRoundPlan& Plan : Checkpoint.RoundPlans)
    {
        const FCombatCheckpointUnit* Saved = Checkpoint.Units.FindByPredicate([&Plan](const FCombatCheckpointUnit& Unit) { return Unit.RoundUnitId == Plan.UnitId; });
        if (!Saved || PlannedUnits.Contains(Plan.UnitId) || Plan.Command.UnitId != Plan.UnitId) return false;
        if (Plan.Command.TargetUnitId != INDEX_NONE && !Checkpoint.Units.ContainsByPredicate([&Plan](const FCombatCheckpointUnit& Unit) { return Unit.RoundUnitId == Plan.Command.TargetUnitId; })) return false;
        PlannedUnits.Add(Plan.UnitId);
        FUnit& Unit = State.Units.AddDefaulted_GetRef();
        Unit.UnitId = Saved->RoundUnitId;
        Unit.bAlive = !Saved->bDead;
        Unit.bEnemy = Saved->Team == ETeam::Enemy;
        Unit.AP = Saved->AP;
        Unit.SAP = Saved->SubAP;
        Unit.HomeCoord = Saved->GridCoord;
        Unit.Command = Plan.Command;
        Unit.Command.SkillId = UCombatCheckpointLibrary::ResolveSavedSkillId(Unit.Command.SkillId);
        Unit.bHasMovePlan = Plan.bHasMovePlan;
        Unit.MoveDestinationCoord = Plan.MoveDestinationCoord;
        if (!Unit.bAlive && (Unit.bHasMovePlan || !Unit.Command.SkillId.IsNone())) return false;
        if (Unit.bAlive) State.Tiles.Add(Unit.HomeCoord, Unit.UnitId);
        for (const FSoftObjectPath& Path : Saved->Skills)
        {
            const USkillDefinitionDataAsset* Definition = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
            FCombatRoundSkill Skill;
            if (!Definition || !Definition->ResolveRoundSkill(Skill, OutError)) return false;
            Unit.SkillIds.Add(Skill.SkillId);
            if (!FindSkill(State, Skill.SkillId)) State.Skills.Add(MoveTemp(Skill));
        }
    }
    if (PlannedUnits.Num() != Checkpoint.Units.Num()) return false;
    // Checkpoint structure is validated without inventing saved GAS state; live tag conditions are checked on restore and release.
    // 저장되지 않은 GAS 상태를 가정하지 않고 체크포인트 구조를 검사하며 실제 태그 조건은 복원과 발동 시 검사합니다.
    for (const FUnit& Unit : State.Units)
    {
        if (!Unit.bAlive) continue;
        if (!ValidateCommand(State, Unit.Command, OutError)) return false;
        if (Unit.bHasMovePlan && !ValidateMoveDestination(State, Unit, Unit.MoveDestinationCoord, OutError)) return false;
    }
    return ValidateDestinations(State, OutError);
}
