#include "Combat/Round/CombatPlanValidator.h"

namespace
{
    bool Fail(FText& OutError, const TCHAR* Message)
    {
        OutError = FText::FromString(Message);
        return false;
    }
}

const FCombatRoundSkill* CombatPlanValidation::FindSkill(const FState& State, FName SkillId)
{
    return State.Skills.FindByPredicate([SkillId](const FCombatRoundSkill& Skill) { return Skill.SkillId == SkillId; });
}

bool CombatPlanValidation::IsReservedByOther(const FState& State, int32 UnitId, FIntPoint Coord)
{
    for (const FUnit& Unit : State.Units)
    {
        if (Unit.UnitId == UnitId || !Unit.bAlive) continue;
        if (Unit.HomeCoord == Coord || (Unit.bHasMovePlan && Unit.MoveDestinationCoord == Coord)) return true;
        const FCombatRoundSkill* Skill = FindSkill(State, Unit.Command.SkillId);
        if (Skill && Skill->Approach == ECombatRoundApproach::Tile && Unit.Command.DestinationCoord == Coord) return true;
    }
    return false;
}

bool CombatPlanValidation::ValidateCommand(const FState& State, const FCombatRoundCommand& Command, FText& OutError)
{
    OutError = FText::GetEmpty();
    const FUnit* Unit = State.Units.FindByPredicate([&Command](const FUnit& Candidate) { return Candidate.UnitId == Command.UnitId; });
    const FCombatRoundSkill* Skill = FindSkill(State, Command.SkillId);
    if (!Unit || !Unit->bAlive || (!Command.SkillId.IsNone() && !Skill)) return Fail(OutError, TEXT("행동할 유닛 또는 스킬이 올바르지 않습니다."));
    if (!Command.SkillId.IsNone() && !Unit->SkillIds.Contains(Command.SkillId)) return Fail(OutError, TEXT("이 유닛에게 부여된 스킬이 아닙니다."));
    const int32 APCost = Skill ? Skill->ActionPointCost : 0;
    const int32 SAPCost = (Skill ? Skill->SubActionPointCost : 0) + (Unit->bHasMovePlan ? 1 : 0);
    if (Unit->AP < APCost || Unit->SAP < SAPCost) return Fail(OutError, TEXT("행동 AP 또는 예약 이동과 스킬의 합산 SAP가 부족합니다."));
    if (!Skill || Skill->Kind == ECombatRoundSkillKind::Wait) return true;
    if (Skill->Approach == ECombatRoundApproach::Tile)
    {
        const int32* Occupant = State.Tiles.Find(Command.DestinationCoord);
        if (!Occupant || (*Occupant != INDEX_NONE && *Occupant != Unit->UnitId) || IsReservedByOther(State, Unit->UnitId, Command.DestinationCoord)) return Fail(OutError, TEXT("다른 유닛의 복귀 칸이나 예약 목적지가 아닌 빈 접근 칸을 선택하세요."));
        if (Skill->bRemainAtDestination && !CombatRoundRules::IsOwnTerritory(Unit->bEnemy, Command.DestinationCoord)) return Fail(OutError, TEXT("이동 공격의 최종 위치는 자기 진영이어야 합니다."));
    }
    if (Skill->Kind == ECombatRoundSkillKind::GroundAttack)
    {
        return State.Tiles.Contains(Command.TargetCoord) || Fail(OutError, TEXT("공격할 지점 칸을 선택하세요."));
    }
    const FUnit* Target = State.Units.FindByPredicate([&Command](const FUnit& Candidate) { return Candidate.UnitId == Command.TargetUnitId; });
    if (!Target || !Target->bAlive) return Fail(OutError, TEXT("살아 있는 대상 유닛을 선택하세요."));
    return Target->bEnemy != Unit->bEnemy || Fail(OutError, TEXT("스킬의 대상 진영이 올바르지 않습니다."));
}

bool CombatPlanValidation::ValidateMoveDestination(const FState& State, const FUnit& Unit, FIntPoint Destination, FText& OutError)
{
    const int32* Occupant = State.Tiles.Find(Destination);
    if (!Unit.bAlive || !Occupant || *Occupant != INDEX_NONE || Destination == Unit.HomeCoord || !CombatRoundRules::IsOwnTerritory(Unit.bEnemy, Destination) || IsReservedByOther(State, Unit.UnitId, Destination)) return Fail(OutError, TEXT("이동할 아군 진영의 예약되지 않은 빈칸을 선택하세요."));
    return true;
}

bool CombatPlanValidation::ValidateDestinations(const FState& State, FText& OutError)
{
    TMap<FIntPoint, int32> Reserved;
    for (const FUnit& Unit : State.Units)
    {
        if (Unit.bAlive) Reserved.Add(Unit.HomeCoord, Unit.UnitId);
    }
    for (const FUnit& Unit : State.Units)
    {
        if (!Unit.bAlive) continue;
        const FCombatRoundSkill* Skill = FindSkill(State, Unit.Command.SkillId);
        TArray<FIntPoint> Destinations;
        if (Unit.bHasMovePlan) Destinations.Add(Unit.MoveDestinationCoord);
        if (Skill && Skill->Approach == ECombatRoundApproach::Tile) Destinations.Add(Unit.Command.DestinationCoord);
        for (FIntPoint Destination : Destinations)
        {
            const int32* Existing = Reserved.Find(Destination);
            if (Existing && *Existing != Unit.UnitId) return Fail(OutError, TEXT("목적지가 다른 유닛의 원래 칸, SAP 이동 또는 AP 접근 목적지와 겹칩니다."));
            Reserved.Add(Destination, Unit.UnitId);
        }
    }
    return true;
}
