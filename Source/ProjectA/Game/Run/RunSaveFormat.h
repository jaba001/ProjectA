#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"

// Interpret historical envelopes without coupling their numbering to the current route.
// 과거 저장 번호를 현재 경로와 결합하지 않고 저장 형식을 해석합니다.
struct PROJECTA_API FRunSaveFormat
{
    bool bLegacyOffline = false;
    bool bManaged = false;
    bool bRequiresCombat = false;
    bool bSupportsCombat = false;
    bool bRequiresCurrentCheckpoint = false;
    bool bRejectsCurrentCheckpoint = false;

    static bool Resolve(int32 Version, FRunSaveFormat& OutFormat);
    static bool IsManaged(int32 Version);
    static int32 Select(bool bManaged, ERunIdentityOrigin Origin, ERunPhase Phase);
};
