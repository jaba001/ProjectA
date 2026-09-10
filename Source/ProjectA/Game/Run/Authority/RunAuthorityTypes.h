#pragma once

#include "CoreMinimal.h"
#include "RunAuthorityTypes.generated.h"

// The store revision orders all commits independently of a combat checkpoint's turn revision.
// 저장소 리비전은 전투 체크포인트의 턴 리비전과 독립적으로 모든 저장의 순서를 정합니다.
USTRUCT()
struct PROJECTA_API FRunAuthorityStamp
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid RunId;
    UPROPERTY(SaveGame)
    int64 Revision = 0;
    UPROPERTY(SaveGame)
    int32 HostEpoch = 0;
    UPROPERTY(SaveGame)
    FGuid SessionId;

    bool IsValid() const { return RunId.IsValid() && Revision > 0 && HostEpoch > 0 && SessionId.IsValid(); }
    bool operator==(const FRunAuthorityStamp& Other) const { return RunId == Other.RunId && Revision == Other.Revision && HostEpoch == Other.HostEpoch && SessionId == Other.SessionId; }
    bool operator!=(const FRunAuthorityStamp& Other) const { return !(*this == Other); }
};

// Native SaveGame bytes remain opaque here; the Run coordinator validates their game policy.
// 여기서는 네이티브 SaveGame 바이트를 불투명 데이터로 다루며 Run 조정자가 게임 정책을 검증합니다.
struct PROJECTA_API FRunAuthorityRecordData
{
    FRunAuthorityStamp Stamp;
    TArray<uint8> Payload;
};

enum class ERunAuthorityResult : uint8
{
    Success,
    InvalidRequest,
    NotFound,
    AlreadyExists,
    Busy,
    Conflict,
    InvalidRecord,
    StorageFailure,
    UnsupportedPlatform
};
