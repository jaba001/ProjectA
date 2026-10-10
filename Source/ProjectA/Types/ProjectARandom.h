#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

// Draw a full 32-bit seed; FMath::Rand only returns 0-32767 on MSVC and limits saved Run variety.
// 32비트 전체 범위의 시드를 추첨합니다. MSVC의 FMath::Rand는 0~32767만 반환하여 저장되는 Run의 다양성을 제한합니다.
namespace ProjectARandom
{
    inline int32 NewSeed()
    {
        const FGuid Guid = FGuid::NewGuid();
        return static_cast<int32>(HashCombineFast(HashCombineFast(Guid.A, Guid.B), HashCombineFast(Guid.C, Guid.D)));
    }
}
