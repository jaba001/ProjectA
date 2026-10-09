#include "Game/Run/RunDungeonPlan.h"

#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"

namespace RunDungeonPlanInternal
{
    // Version one uses fixed integer mixing and eight frozen patterns, independent of engine RNG state.
    // 버전 1은 엔진 난수 상태와 독립적인 고정 정수 혼합과 8개 고정 패턴을 사용합니다.
    uint32 Mix(uint32 Value)
    {
        Value ^= Value >> 16;
        Value *= 0x7feb352du;
        Value ^= Value >> 15;
        Value *= 0x846ca68bu;
        return Value ^ (Value >> 16);
    }

    int32 SeedForRun(const FGuid& RunId)
    {
        return static_cast<int32>(Mix(RunId.A ^ Mix(RunId.B) ^ Mix(RunId.C + 0x9e3779b9u) ^ Mix(RunId.D + 0x444e4731u)));
    }

    bool ReadOfferIds(TConstArrayView<FRunEncounterOffer> Offers, TArray<FName>& OutIds)
    {
        if (Offers.Num() != 3) return false;
        for (const FRunEncounterOffer& Offer : Offers)
        {
            if (Offer.EncounterId.IsNone() || OutIds.Contains(Offer.EncounterId)) return false;
            OutIds.Add(Offer.EncounterId);
        }
        return true;
    }
}

bool RunDungeonPlan::Build(const URunSaveGame& Save, FRunDungeonState& OutState, FText& OutError)
{
    OutError = NSLOCTEXT("RunDungeon", "BuildFailed", "Run의 경로·시드·인카운터 후보로 던전을 구성할 수 없습니다.");
    const FRunRouteDefinition* Route = RunProgressRules::GetRouteForNodes(Save.Nodes);
    if (!Route || !Save.Identity.RunId.IsValid()) return false;
    const bool bTarget = Save.TargetRun.SchemaVersion == 1;
    if (bTarget != Route->bTargetRun || (bTarget ? Save.EncounterProgress.SchemaVersion != 2 : Save.EncounterProgress.SchemaVersion != 1)) return false;

    FRunDungeonState Plan;
    Plan.SchemaVersion = 1;
    Plan.Seed = RunDungeonPlanInternal::SeedForRun(Save.Identity.RunId);
    const int32 FirstBoundary = bTarget ? 0 : Route->EncounterAfterCompletedNodes;
    const int32 EndBoundary = bTarget || Route->bRepeatEncounters ? Route->Nodes.Num() : FirstBoundary + 1;
    const int32 VisitsPerBoundary = bTarget ? 3 : 1;
    for (int32 Boundary = FirstBoundary; Boundary < EndBoundary; ++Boundary)
    {
        for (int32 VisitIndex = 0; VisitIndex < VisitsPerBoundary; ++VisitIndex)
        {
            TArray<FRunEncounterOffer> Offers;
            if (bTarget)
            {
                // Freeze the existing tag-weighted selection at creation without changing its stream or order.
                // 기존 태그 가중치 추첨의 난수 흐름과 순서를 바꾸지 않고 생성 시 결과를 고정합니다.
                if (!UTargetRunDefinitionDataAsset::BuildOffers(Save.TargetRun, Boundary, VisitIndex, Offers)) return false;
            }
            else Offers = Save.EncounterProgress.Offers;

            FRunDungeonVisit Visit;
            Visit.AfterCompletedNodeCount = Boundary;
            Visit.VisitIndex = VisitIndex;
            Visit.LayoutVariant = static_cast<int32>(RunDungeonPlanInternal::Mix(static_cast<uint32>(Plan.Seed) ^ (0x9e3779b9u * static_cast<uint32>(Plan.Visits.Num() + 1))) % 8u);
            if (!RunDungeonPlanInternal::ReadOfferIds(Offers, Visit.OfferIds)) return false;
            Plan.Visits.Add(MoveTemp(Visit));
        }
    }
    if (Plan.Visits.IsEmpty() || Plan.Visits.Num() > 60) return false;
    OutState = MoveTemp(Plan);
    OutError = FText::GetEmpty();
    return true;
}

int32 RunDungeonPlan::FindVisit(const FRunDungeonState& State, const FRunEncounterProgress& Progress)
{
    if (State.SchemaVersion != 1) return INDEX_NONE;
    return State.Visits.IndexOfByPredicate([&Progress](const FRunDungeonVisit& Visit) { return Visit.AfterCompletedNodeCount == Progress.AfterCompletedNodeCount && Visit.VisitIndex == Progress.VisitIndex; });
}

bool RunDungeonPlan::Validate(const URunSaveGame& Save, FText& OutError)
{
    OutError = NSLOCTEXT("RunDungeon", "InvalidPlan", "저장된 던전 버전·배치·인카운터 경로가 Run과 일치하지 않습니다. 기존 저장을 유지합니다.");
    if (Save.DungeonState.SchemaVersion == 0)
    {
        if (Save.DungeonState.Seed != 0 || !Save.DungeonState.Visits.IsEmpty()) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (Save.DungeonState.SchemaVersion != 1 || Save.DungeonState.Visits.Num() > 60) return false;
    FRunDungeonState Expected;
    FText BuildError;
    if (!Build(Save, Expected, BuildError) || !FRunDungeonState::StaticStruct()->CompareScriptStruct(&Save.DungeonState, &Expected, 0)) return false;
    const int32 Index = FindVisit(Save.DungeonState, Save.EncounterProgress);
    if (!Save.DungeonState.Visits.IsValidIndex(Index)) return false;
    TArray<FName> CurrentIds;
    if (!RunDungeonPlanInternal::ReadOfferIds(Save.EncounterProgress.Offers, CurrentIds) || CurrentIds != Save.DungeonState.Visits[Index].OfferIds) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool RunDungeonPlan::ResolveOffers(const URunSaveGame& Save, int32 CompletedCount, int32 VisitIndex, TArray<FRunEncounterOffer>& OutOffers, FText& OutError)
{
    if (Save.DungeonState.SchemaVersion == 0)
    {
        if (UTargetRunDefinitionDataAsset::BuildOffers(Save.TargetRun, CompletedCount, VisitIndex, OutOffers))
        {
            OutError = FText::GetEmpty();
            return true;
        }
    }
    else if (Save.DungeonState.SchemaVersion == 1)
    {
        FRunEncounterProgress Progress;
        Progress.AfterCompletedNodeCount = CompletedCount;
        Progress.VisitIndex = VisitIndex;
        const int32 Index = FindVisit(Save.DungeonState, Progress);
        if (Save.DungeonState.Visits.IsValidIndex(Index))
        {
            const FRunDungeonVisit& Visit = Save.DungeonState.Visits[Index];
            const TArray<FRunEncounterOffer>& Pool = Save.TargetRun.SchemaVersion == 1 ? Save.TargetRun.EncounterPool : Save.EncounterProgress.Offers;
            TArray<FRunEncounterOffer> Offers;
            for (const FName Id : Visit.OfferIds)
            {
                const FRunEncounterOffer* Offer = Pool.FindByPredicate([Id](const FRunEncounterOffer& Entry) { return Entry.EncounterId == Id; });
                if (!Offer) break;
                Offers.Add(*Offer);
            }
            if (Visit.OfferIds.Num() == 3 && Offers.Num() == 3)
            {
                OutOffers = MoveTemp(Offers);
                OutError = FText::GetEmpty();
                return true;
            }
        }
    }
    OutError = NSLOCTEXT("RunDungeon", "MissingVisit", "저장된 던전에서 다음 인카운터 후보를 찾을 수 없습니다.");
    return false;
}
