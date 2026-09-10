#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunIdentityTypes.h"
#include "RunParticipationTypes.generated.h"

// Participation records the control roster; original ownership and legacy consent metadata stay unchanged.
// 참여 상태는 조작 참가자를 기록하며 원래 소유권과 기존 동의 호환 정보는 변경하지 않습니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunParticipationData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Run|Participation")
    int32 SchemaVersion = 1;

    // Omitted original participants use server AI after the Host authorizes resume; prior consent is not required.
    // 목록에 없는 원래 참가자는 Host가 재개를 승인하면 서버 AI를 사용하며 사전 동의는 요구하지 않습니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Run|Participation")
    TArray<FRunAccountId> HumanParticipants;
};
