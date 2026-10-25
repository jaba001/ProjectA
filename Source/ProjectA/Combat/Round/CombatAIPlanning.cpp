#include "Combat/Round/CombatAIPlanning.h"
#include "Combat/Round/CombatPlanValidator.h"
#include "Combat/Round/CombatSkillExecutor.h"
#include "GAS/CombatGameplayTags.h"
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

namespace
{
    bool IsPlannableSkill(const FCombatRoundSkill* Skill)
    {
        return Skill && Skill->Kind != ECombatRoundSkillKind::Wait && !Skill->bRemainAtDestination;
    }

    FCombatRoundCommand BuildCommand(const FCombatRoundView& View, const CombatPlanValidation::FState& State, int32 UnitIndex, FName SkillId, const FCombatRoundSkill& Skill, int32 TargetIndex)
    {
        const FCombatRoundUnitView& Entry = View.Units[UnitIndex];
        FCombatRoundCommand Command = Entry.Command;
        Command.TargetUnitId = View.Units[TargetIndex].UnitId;
        Command.TargetCoord = View.Units[TargetIndex].HomeCoord;
        Command.SkillId = SkillId;
        Command.DestinationCoord = Entry.HomeCoord;
        if (Skill.Approach != ECombatRoundApproach::Tile) return Command;
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
        return Command;
    }
}

FCombatRoundCommand CombatAIPlanning::ChooseCommand(const FCombatRoundView& View, const CombatPlanValidation::FState& State, int32 UnitIndex, TFunctionRef<bool(const FCombatRoundCommand&)> IsAllowed)
{
    const FCombatRoundUnitView& Entry = View.Units[UnitIndex];
    FCombatRoundCommand Command = Entry.Command;
    Command.SkillId = NAME_None;
    // Try ordinary skills in authored order; retain the first legal fallback until all ordinary skills fail.
    // 일반 스킬을 작성 순서대로 검토하며 모두 사용할 수 없을 때를 위해 첫 유효 대체 행동을 보관합니다.
    for (FName SkillId : Entry.SkillIds)
    {
        const FCombatRoundSkill* Skill = CombatPlanValidation::FindSkill(State, SkillId);
        if (!IsPlannableSkill(Skill)) continue;
        int32 TargetIndex = INDEX_NONE;
        double TargetDistance = TNumericLimits<double>::Max();
        for (int32 CandidateIndex = 0; CandidateIndex < View.Units.Num(); ++CandidateIndex)
        {
            const FCombatRoundUnitView& Candidate = View.Units[CandidateIndex];
            if (!CombatSkillExecution::IsValidEffectTarget(Entry.Unit, Candidate.Unit, *Skill)) continue;
            const FCombatRoundCommand CandidateCommand = BuildCommand(View, State, UnitIndex, SkillId, *Skill, CandidateIndex);
            if (!IsAllowed(CandidateCommand)) continue;
            const double Distance = FVector::DistSquared2D(Entry.Unit->GetActorLocation(), Candidate.Unit->GetActorLocation());
            if (Distance < TargetDistance)
            {
                TargetIndex = CandidateIndex;
                TargetDistance = Distance;
            }
        }
        if (!View.Units.IsValidIndex(TargetIndex)) continue;
        const FCombatRoundCommand Candidate = BuildCommand(View, State, UnitIndex, SkillId, *Skill, TargetIndex);
        if (!Skill->EffectTags.HasTag(ProjectACombatTags::Skill_AI_Fallback)) return Candidate;
        if (Command.SkillId.IsNone()) Command = Candidate;
    }
    return Command;
}

TArray<FCombatRoundCommand> CombatAIPlanning::CollectCommands(const FCombatRoundView& View, const CombatPlanValidation::FState& State, int32 UnitIndex, TFunctionRef<bool(const FCombatRoundCommand&)> IsAllowed)
{
    TArray<FCombatRoundCommand> Commands;
    const FCombatRoundUnitView& Entry = View.Units[UnitIndex];
    for (FName SkillId : Entry.SkillIds)
    {
        const FCombatRoundSkill* Skill = CombatPlanValidation::FindSkill(State, SkillId);
        if (!IsPlannableSkill(Skill)) continue;
        for (int32 CandidateIndex = 0; CandidateIndex < View.Units.Num(); ++CandidateIndex)
        {
            if (!CombatSkillExecution::IsValidEffectTarget(Entry.Unit, View.Units[CandidateIndex].Unit, *Skill)) continue;
            const FCombatRoundCommand Candidate = BuildCommand(View, State, UnitIndex, SkillId, *Skill, CandidateIndex);
            if (IsAllowed(Candidate)) Commands.Add(Candidate);
        }
    }
    return Commands;
}
