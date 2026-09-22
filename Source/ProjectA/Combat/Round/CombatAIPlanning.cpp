#include "Combat/Round/CombatAIPlanning.h"
#include "Combat/Round/CombatPlanValidator.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "Unit/UnitBase.h"

int32 CombatAIPlanning::FindNearestEnemy(const FCombatRoundView& View, int32 SourceIndex, TFunctionRef<bool(const FCombatRoundUnitView&)> IsAllowed)
{
    if (!View.Units.IsValidIndex(SourceIndex) || !IsValid(View.Units[SourceIndex].Unit)) return INDEX_NONE;
    const FCombatRoundUnitView& Source = View.Units[SourceIndex];
    int32 Best = INDEX_NONE;
    double Distance = TNumericLimits<double>::Max();
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const FCombatRoundUnitView& Candidate = View.Units[Index];
        if (Candidate.bEnemy == Source.bEnemy || !IsValid(Candidate.Unit) || !Candidate.Unit->IsUnitAlive() || !IsAllowed(Candidate)) continue;
        const double CandidateDistance = FVector::DistSquared2D(Source.Unit->GetActorLocation(), Candidate.Unit->GetActorLocation());
        if (CandidateDistance < Distance)
        {
            Distance = CandidateDistance;
            Best = Index;
        }
    }
    return Best;
}

FCombatRoundCommand CombatAIPlanning::ChooseCommand(const FCombatRoundView& View, const CombatPlanValidation::FState& State, int32 UnitIndex, TFunctionRef<bool(const FCombatRoundCommand&)> IsAllowed)
{
    const FCombatRoundUnitView& Entry = View.Units[UnitIndex];
    FCombatRoundCommand Command = Entry.Command;
    Command.SkillId = NAME_None;
    // Keep authored skill order as policy; tag filters only remove ineligible candidates.
    // 제작된 스킬 순서를 정책으로 유지하고 태그 필터는 부적격 후보만 제거합니다.
    for (FName SkillId : Entry.SkillIds)
    {
        const FCombatRoundSkill* Skill = CombatPlanValidation::FindSkill(State, SkillId);
        if (!Skill || Skill->Kind == ECombatRoundSkillKind::Wait || Skill->bRemainAtDestination) continue;
        const int32 TargetIndex = FindNearestEnemy(View, UnitIndex, [Skill](const FCombatRoundUnitView& Target) { return CombatSkillExecution::CanAffectTarget(Target.Unit, *Skill); });
        if (!View.Units.IsValidIndex(TargetIndex)) continue;
        Command.TargetUnitId = View.Units[TargetIndex].UnitId;
        Command.TargetCoord = View.Units[TargetIndex].HomeCoord;
        Command.SkillId = SkillId;
        Command.DestinationCoord = Entry.HomeCoord;
        if (Skill->Approach == ECombatRoundApproach::Tile)
        {
            int32 BestDistance = MAX_int32;
            for (int32 X = 0; X < 4; ++X)
            {
                for (int32 Y = 0; Y < 4; ++Y)
                {
                    const FIntPoint Coord(X, Y);
                    const int32* Occupant = State.Tiles.Find(Coord);
                    if (!Occupant || (*Occupant != INDEX_NONE && *Occupant != Entry.UnitId) || CombatPlanValidation::IsReservedByOther(State, Entry.UnitId, Coord)) continue;
                    const FIntPoint Offset = Coord - Command.TargetCoord;
                    const int32 Distance = FMath::Abs(Offset.X) + FMath::Abs(Offset.Y);
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        Command.DestinationCoord = Coord;
                    }
                }
            }
        }
        if (IsAllowed(Command)) return Command;
        Command.SkillId = NAME_None;
        Command.DestinationCoord = Entry.HomeCoord;
    }
    return Command;
}
