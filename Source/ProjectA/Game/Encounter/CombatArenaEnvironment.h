#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CombatArenaEnvironment.generated.h"

// Reference authored regions without copying their assets or changing combat coordinates.
// 에셋을 복제하거나 전투 좌표를 바꾸지 않고 작성된 지역을 참조합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatArenaEnvironmentProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ArenaId;
    UPROPERTY(BlueprintReadOnly)
    FName SourceRegionTag;
    UPROPERTY(BlueprintReadOnly)
    FVector SourceOffset = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;
    UPROPERTY(BlueprintReadOnly)
    FText Description;
    UPROPERTY(BlueprintReadOnly)
    FGameplayTagQuery DifficultyQuery;
};

namespace CombatArenaEnvironment
{
    PROJECTA_API const TArray<FCombatArenaEnvironmentProfile>& GetProfiles();
    PROJECTA_API const FCombatArenaEnvironmentProfile* Find(FName ArenaId);
    PROJECTA_API FTransform RebaseTransform(const FTransform& Source, const FCombatArenaEnvironmentProfile& Profile);
    PROJECTA_API bool IsGroundBounds(const FBox& Bounds);
    PROJECTA_API bool ShouldKeepBounds(const FBox& Bounds, const FBox& CombatClearance, const FVector& CameraPosition, const TArray<FVector>& CombatSamples);
}
