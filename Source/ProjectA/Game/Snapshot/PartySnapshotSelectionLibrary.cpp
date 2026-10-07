#include "Game/Snapshot/PartySnapshotSelectionLibrary.h"

#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "Types/GameplayTagCandidateSelection.h"

bool UPartySnapshotSelectionLibrary::SelectOpponent(const TArray<FPartySnapshotCandidate>& Candidates, const UOpponentSnapshotCatalogDataAsset* Catalog, int32 ProgressStage, const FGameplayTagQuery& Query, int32 FormationSlotCount, FRandomStream& Random, FPartySnapshot& OutSnapshot, FText& OutError)
{
    OutError = NSLOCTEXT("SnapshotSelection", "InvalidRequest", "상대 후보 선정에 유효한 카탈로그·진행 단계·1~4개 배치 슬롯이 필요합니다.");
    if (!IsValid(Catalog) || Catalog->ContentVersion <= 0 || ProgressStage < 0 || FormationSlotCount < 1 || FormationSlotCount > 4) return false;

    TArray<FGameplayTagWeightedCandidate> Eligible;
    TArray<int32> CandidateIndices;
    TSet<FName> SnapshotIds;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        const FPartySnapshotCandidate& Candidate = Candidates[Index];
        if (Candidate.ProgressStage != ProgressStage || Candidate.Snapshot.ContentVersion != Catalog->ContentVersion || (!Query.IsEmpty() && !Query.Matches(Candidate.Tags))) continue;
        FText ValidationError;
        if (!Catalog->ValidateForEncounter(Candidate.Snapshot, FormationSlotCount, ValidationError)) continue;
        if (SnapshotIds.Contains(Candidate.Snapshot.SnapshotId))
        {
            OutError = NSLOCTEXT("SnapshotSelection", "DuplicateId", "동일한 Snapshot 식별자가 적격 후보에 중복되어 균등 추첨할 수 없습니다.");
            return false;
        }
        SnapshotIds.Add(Candidate.Snapshot.SnapshotId);
        FGameplayTagWeightedCandidate& Entry = Eligible.AddDefaulted_GetRef();
        Entry.Tags = Candidate.Tags;
        Entry.BaseWeight = 1.0f;
        CandidateIndices.Add(Index);
    }

    FRandomStream SelectedRandom = Random;
    TArray<int32> SelectedIndices;
    if (!GameplayTagCandidateSelection::Select(Eligible, Query, 1, false, SelectedRandom, SelectedIndices))
    {
        OutError = NSLOCTEXT("SnapshotSelection", "NoCandidate", "동일 콘텐츠 버전·진행 단계·태그 조건을 만족하는 생존 상대 후보가 없습니다.");
        return false;
    }
    OutSnapshot = Candidates[CandidateIndices[SelectedIndices[0]]].Snapshot;
    Random = SelectedRandom;
    OutError = FText::GetEmpty();
    return true;
}
