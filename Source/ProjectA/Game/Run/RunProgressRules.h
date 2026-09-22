#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunEncounterTypes.h"
#include "Game/Run/RunTypes.h"
#include "Types/CombatResult.h"

// The prototype route is a content contract independent of save versions and storage.
// 시험용 경로는 저장 버전과 저장소에서 분리된 콘텐츠 계약입니다.
struct PROJECTA_API FRunRouteDefinition
{
    TArray<FRunNodeDefinition> Nodes;
    int32 EncounterAfterCompletedNodes = INDEX_NONE;
    int32 EncounterOfferCount = 0;
};

// Borrow runtime values without owning actors, loading assets or interpreting a save format.
// 액터 소유, 에셋 로드, 저장 형식 해석 없이 런타임 값을 참조합니다.
struct PROJECTA_API FRunProgressView
{
    const TArray<FRunNodeDefinition>& Nodes;
    const TArray<FName>& CompletedNodes;
    FName CurrentNode;
    FName CurrentEncounter;
    ERunPhase Phase;
    ECombatResult Result;
};

namespace RunProgressRules
{
    PROJECTA_API const FRunRouteDefinition& GetPrototypeRoute();
    PROJECTA_API bool ValidateNodes(const FRunRouteDefinition& Route, const FRunProgressView& Progress);
    PROJECTA_API bool ValidateEncounterProgress(const FRunRouteDefinition& Route, const FRunProgressView& Progress, const FRunEncounterProgress& Encounter);
    PROJECTA_API bool ValidatePhase(const FRunProgressView& Progress, bool bHasCreatedMember, bool bHasLivingMember);
    PROJECTA_API ERunPhase GetContinuationPhase(const FRunRouteDefinition& Route, int32 CompletedNodeCount, const FRunEncounterProgress& Encounter);
}
