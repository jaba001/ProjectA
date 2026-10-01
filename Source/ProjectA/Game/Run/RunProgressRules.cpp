#include "Game/Run/RunProgressRules.h"

const FRunRouteDefinition& RunProgressRules::GetLegacyPrototypeRoute()
{
    static const FRunRouteDefinition Route = []()
    {
        FRunRouteDefinition Definition;
        Definition.EncounterAfterCompletedNodes = 1;
        Definition.EncounterOfferCount = 3;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FRunNodeDefinition& Node = Definition.Nodes.AddDefaulted_GetRef();
            Node.NodeId = FName(*FString::Printf(TEXT("Combat_%02d"), Index + 1));
            Node.DisplayName = FText::FromString(FString::Printf(TEXT("Combat %d / 전투 %d"), Index + 1, Index + 1));
            Node.EncounterId = TEXT("DefaultEncounter");
        }
        return Definition;
    }();
    return Route;
}

const FRunRouteDefinition& RunProgressRules::GetPrototypeRoute()
{
    static const FRunRouteDefinition Route = []()
    {
        FRunRouteDefinition Definition = GetLegacyPrototypeRoute();
        Definition.bRepeatEncounters = true;
        for (int32 Index = Definition.Nodes.Num(); Index < 10; ++Index)
        {
            FRunNodeDefinition& Node = Definition.Nodes.AddDefaulted_GetRef();
            Node.NodeId = FName(*FString::Printf(TEXT("Combat_%02d"), Index + 1));
            Node.DisplayName = FText::FromString(FString::Printf(TEXT("Combat %d / 전투 %d"), Index + 1, Index + 1));
            Node.EncounterId = TEXT("DefaultEncounter");
        }
        return Definition;
    }();
    return Route;
}

const FRunRouteDefinition* RunProgressRules::GetRouteForNodes(TConstArrayView<FRunNodeDefinition> Nodes)
{
    // Preserve saved route lengths; node identity and ordering are validated separately.
    // 저장된 경로 길이를 보존하며 노드 식별자와 순서는 별도로 검증합니다.
    if (Nodes.Num() == GetPrototypeRoute().Nodes.Num()) return &GetPrototypeRoute();
    if (Nodes.Num() == GetLegacyPrototypeRoute().Nodes.Num()) return &GetLegacyPrototypeRoute();
    return nullptr;
}

bool RunProgressRules::ValidateNodes(const FRunRouteDefinition& Route, const FRunProgressView& Progress)
{
    if (Route.Nodes.IsEmpty() || Progress.Nodes.Num() != Route.Nodes.Num() || Progress.CompletedNodes.Num() > Progress.Nodes.Num()) return false;
    for (int32 Index = 0; Index < Route.Nodes.Num(); ++Index)
    {
        const FRunNodeDefinition& Expected = Route.Nodes[Index];
        const FRunNodeDefinition& Actual = Progress.Nodes[Index];
        if (Actual.NodeId != Expected.NodeId || Actual.EncounterId != Expected.EncounterId || Actual.NodeType != Expected.NodeType || (Progress.CompletedNodes.IsValidIndex(Index) && Progress.CompletedNodes[Index] != Expected.NodeId)) return false;
    }
    return true;
}

bool RunProgressRules::ValidateEncounterProgress(const FRunRouteDefinition& Route, const FRunProgressView& Progress, const FRunEncounterProgress& Encounter)
{
    const bool bChoice = Progress.Phase == ERunPhase::EncounterChoice;
    const bool bShop = Progress.Phase == ERunPhase::Shop;
    if (Encounter.SchemaVersion == 0) return !bChoice && !bShop && Encounter.Offers.IsEmpty() && Encounter.SelectedEncounterId.IsNone() && !Encounter.bCompleted;
    const int32 Boundary = Route.bRepeatEncounters ? Encounter.AfterCompletedNodeCount : Route.EncounterAfterCompletedNodes;
    if (Encounter.SchemaVersion != 1 || Boundary < 1 || !Progress.Nodes.IsValidIndex(Boundary) || Encounter.Offers.Num() != Route.EncounterOfferCount) return false;
    if (Route.bRepeatEncounters)
    {
        const bool bAfterBattle = Progress.Phase == ERunPhase::Result || Progress.Phase == ERunPhase::Complete;
        const int32 ExpectedBoundary = FMath::Max(Progress.CompletedNodes.Num() - (bAfterBattle ? 1 : 0), Route.EncounterAfterCompletedNodes);
        if (Boundary != ExpectedBoundary) return false;
    }
    TSet<FName> Ids;
    for (const FRunEncounterOffer& Offer : Encounter.Offers)
    {
        if (Offer.EncounterId.IsNone() || Ids.Contains(Offer.EncounterId) || Offer.DisplayName.ToString().TrimStartAndEnd().IsEmpty() || !Offer.IsSupportedShop()) return false;
        Ids.Add(Offer.EncounterId);
    }
    const bool bSelected = !Encounter.SelectedEncounterId.IsNone();
    if ((bSelected && !Ids.Contains(Encounter.SelectedEncounterId)) || (Encounter.bCompleted && !bSelected)) return false;
    const int32 Completed = Progress.CompletedNodes.Num();
    if (Completed < Boundary) return !bChoice && !bShop && !bSelected && !Encounter.bCompleted;
    if (bChoice || bShop) return Completed == Boundary && Progress.CurrentNode == Progress.Nodes[Completed - 1].NodeId && Progress.CurrentEncounter.IsNone() && Progress.Result == ECombatResult::Victory && !Encounter.bCompleted && bSelected == bShop;
    if (Progress.Phase == ERunPhase::Result && Completed == Boundary) return !bSelected && !Encounter.bCompleted;
    return Encounter.bCompleted;
}

bool RunProgressRules::ValidatePhase(const FRunProgressView& Progress, bool bHasCreatedMember, bool bHasLivingMember)
{
    if (!bHasCreatedMember || Progress.Nodes.IsEmpty() || Progress.CompletedNodes.Num() > Progress.Nodes.Num()) return false;
    const int32 Completed = Progress.CompletedNodes.Num();
    const bool bHasNext = Progress.Nodes.IsValidIndex(Completed);
    const FRunNodeDefinition* Previous = Completed > 0 ? &Progress.Nodes[Completed - 1] : nullptr;
    const FRunNodeDefinition* Next = bHasNext ? &Progress.Nodes[Completed] : nullptr;
    const bool bMapEntry = Progress.Result == ECombatResult::None && Progress.CurrentNode.IsNone();
    const bool bMapContinue = Previous && Progress.Result == ECombatResult::Victory && Progress.CurrentNode == Previous->NodeId;
    const bool bMap = Progress.Phase == ERunPhase::Map && bHasNext && Progress.CurrentEncounter.IsNone() && (bMapEntry || bMapContinue);
    const bool bResult = Progress.Phase == ERunPhase::Result && Previous && Progress.Result == ECombatResult::Victory && Progress.CurrentNode == Previous->NodeId && Progress.CurrentEncounter == Previous->EncounterId;
    const bool bComplete = Progress.Phase == ERunPhase::Complete && Completed == Progress.Nodes.Num() && Progress.Result == ECombatResult::Victory && Progress.CurrentNode == Progress.Nodes.Last().NodeId && Progress.CurrentEncounter.IsNone();
    const bool bDefeat = Progress.Phase == ERunPhase::Defeat && Next && Progress.Result == ECombatResult::Defeat && !bHasLivingMember && Progress.CurrentNode == Next->NodeId && Progress.CurrentEncounter == Next->EncounterId;
    const bool bCombat = Progress.Phase == ERunPhase::Combat && Next && Progress.Result == ECombatResult::None && Progress.CurrentNode == Next->NodeId && Progress.CurrentEncounter == Next->EncounterId;
    const bool bRunEncounter = Progress.Phase == ERunPhase::EncounterChoice || Progress.Phase == ERunPhase::Shop;
    return (bMap || bResult || bComplete || bDefeat || bCombat || bRunEncounter) && (bDefeat || bHasLivingMember);
}

ERunPhase RunProgressRules::GetContinuationPhase(const FRunRouteDefinition& Route, int32 CompletedNodeCount, const FRunEncounterProgress& Encounter)
{
    if (CompletedNodeCount >= Route.Nodes.Num()) return ERunPhase::Complete;
    const int32 Boundary = Route.bRepeatEncounters ? Encounter.AfterCompletedNodeCount : Route.EncounterAfterCompletedNodes;
    if (Encounter.SchemaVersion == 1 && !Encounter.bCompleted && CompletedNodeCount == Boundary) return ERunPhase::EncounterChoice;
    return ERunPhase::Map;
}
