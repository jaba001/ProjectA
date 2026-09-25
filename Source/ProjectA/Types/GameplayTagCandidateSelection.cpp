#include "Types/GameplayTagCandidateSelection.h"

bool GameplayTagCandidateSelection::Select(TConstArrayView<FGameplayTagWeightedCandidate> Candidates, const FGameplayTagQuery& Query, int32 Count, bool bAllowDuplicates, FRandomStream& Random, TArray<int32>& OutIndices)
{
    if (Count <= 0) return false;
    TArray<int32> EligibleIndices;
    double TotalWeight = 0.0;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        const FGameplayTagWeightedCandidate& Candidate = Candidates[Index];
        if (!FMath::IsFinite(Candidate.BaseWeight) || Candidate.BaseWeight < 0.0f) return false;
        if (Candidate.BaseWeight == 0.0f || (!Query.IsEmpty() && !Query.Matches(Candidate.Tags))) continue;
        EligibleIndices.Add(Index);
        TotalWeight += Candidate.BaseWeight;
    }
    if (EligibleIndices.IsEmpty() || (!bAllowDuplicates && EligibleIndices.Num() < Count)) return false;

    TArray<int32> SelectedIndices;
    SelectedIndices.Reserve(Count);
    for (int32 Selection = 0; Selection < Count; ++Selection)
    {
        const double Draw = static_cast<double>(Random.FRand()) * TotalWeight;
        double CumulativeWeight = 0.0;
        int32 SelectedPosition = EligibleIndices.Num() - 1;
        for (int32 Position = 0; Position < EligibleIndices.Num(); ++Position)
        {
            CumulativeWeight += Candidates[EligibleIndices[Position]].BaseWeight;
            if (Draw < CumulativeWeight)
            {
                SelectedPosition = Position;
                break;
            }
        }
        const int32 SelectedIndex = EligibleIndices[SelectedPosition];
        SelectedIndices.Add(SelectedIndex);
        if (!bAllowDuplicates)
        {
            TotalWeight -= Candidates[SelectedIndex].BaseWeight;
            EligibleIndices.RemoveAt(SelectedPosition);
        }
    }
    OutIndices = MoveTemp(SelectedIndices);
    return true;
}
