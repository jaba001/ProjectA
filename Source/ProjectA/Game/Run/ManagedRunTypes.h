#pragma once

#include "CoreMinimal.h"
#include "Game/Run/Authority/RunAuthorityTypes.h"
#include "Game/Run/RunParticipationTypes.h"
#include "Game/Run/RunTypes.h"

// Trusted C++ fixtures assign this local context; it is not account authentication or player-editable input.
// 신뢰하는 C++ 개발 경로가 배정하는 로컬 문맥이며 계정 인증이나 플레이어가 편집하는 입력이 아닙니다.
struct PROJECTA_API FLocalDevelopmentCallerContext
{
    FString StoreNamespace = TEXT("LocalDevelopment");
    FRunAccountId AccountId;
};

// Reading a preview neither acquires an execution lease nor changes the current Run.
// 미리보기 조회는 실행 lease를 획득하거나 현재 Run을 변경하지 않습니다.
struct PROJECTA_API FManagedRunPreview
{
    FRunAuthorityStamp Stamp;
    FRunIdentityData Identity;
    FRunParticipationData Participation;
    ERunPhase Phase = ERunPhase::None;
    bool bHasCombatCheckpoint = false;
    int32 CompletedNodeCount = 0;
    int32 TotalNodeCount = 0;
};
