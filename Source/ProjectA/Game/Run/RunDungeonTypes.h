#pragma once

#include "CoreMinimal.h"
#include "RunDungeonTypes.generated.h"

// Each ordered visit joins the next visit after one of its three encounter rooms.
// 각 순차 방문은 세 인카운터 방 중 하나를 거친 뒤 다음 방문으로 합류합니다.
USTRUCT()
struct PROJECTA_API FRunDungeonVisit
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    int32 AfterCompletedNodeCount = 0;
    UPROPERTY(SaveGame)
    int32 VisitIndex = 0;
    UPROPERTY(SaveGame)
    int32 LayoutVariant = 0;
    UPROPERTY(SaveGame)
    TArray<FName> OfferIds;
};

// Freeze the complete logical dungeon without serializing scenery or consuming gameplay randomness.
// 장식 액터를 직렬화하거나 게임플레이 난수를 소비하지 않고 전체 논리 던전을 고정합니다.
USTRUCT()
struct PROJECTA_API FRunDungeonState
{
    GENERATED_BODY()

    // Missing data keeps the original fixed layout of an existing Run.
    // 데이터가 없는 기존 Run은 원래 고정 배치를 유지합니다.
    UPROPERTY(SaveGame)
    int32 SchemaVersion = 0;
    UPROPERTY(SaveGame)
    int32 Seed = 0;
    UPROPERTY(SaveGame)
    TArray<FRunDungeonVisit> Visits;
};
