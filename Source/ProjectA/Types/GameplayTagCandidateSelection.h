#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayTagCandidateSelection.generated.h"

// Share tag filtering and weighted sampling across content catalogs.
// 콘텐츠 카탈로그가 태그 필터와 가중치 추첨을 공통으로 사용합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FGameplayTagWeightedCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    FGameplayTagContainer Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection", meta = (ClampMin = "0"))
    float BaseWeight = 1.0f;
};

namespace GameplayTagCandidateSelection
{
    PROJECTA_API bool Select(TConstArrayView<FGameplayTagWeightedCandidate> Candidates, const FGameplayTagQuery& Query, int32 Count, bool bAllowDuplicates, FRandomStream& Random, TArray<int32>& OutIndices);
}
