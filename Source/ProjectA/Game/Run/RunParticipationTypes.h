#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunIdentityTypes.h"
#include "RunParticipationTypes.generated.h"

// Participation records the control roster; identity and consent remain in the original Run identity.
// 참여 상태는 조작 참가자를 기록하며 식별 정보와 동의는 원래 Run 식별 정보에 유지합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunParticipationData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Run|Participation")
    int32 SchemaVersion = 1;

    // Omitted original participants use server AI only after a separate resume approval succeeds.
    // 목록에 없는 원래 참가자는 별도 재개 승인이 성공한 뒤에만 서버 AI를 사용합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Run|Participation")
    TArray<FRunAccountId> HumanParticipants;
};
