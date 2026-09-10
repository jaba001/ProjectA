#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Game/Run/Authority/RunAuthorityTypes.h"
#include "LocalRunAuthorityRecord.generated.h"

// One atomic file contains both the fencing stamp and the entire canonical Run payload.
// 하나의 원자적 파일에 이전 권한을 차단하는 스탬프와 전체 기준 Run 본문을 함께 저장합니다.
UCLASS()
class PROJECTA_API ULocalRunAuthorityRecord : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 Version = 1;
    UPROPERTY(SaveGame)
    FRunAuthorityStamp Stamp;
    UPROPERTY(SaveGame)
    TArray<uint8> Payload;
};
