#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

// Draw a full 32-bit seed; FMath::Rand only returns 0-32767 on MSVC and limits saved Run variety.
// 32비트 전체 범위의 시드를 추첨합니다. MSVC의 FMath::Rand는 0~32767만 반환하여 저장되는 Run의 다양성을 제한합니다.
namespace ProjectARandom
{
    PROJECTA_API int32 NewSeed();
    PROJECTA_API FGuid NewRunGuid();
    PROJECTA_API bool IsBalanceSimulation();

#if WITH_EDITOR
    // Restrict deterministic seeds and memory storage to the headless measurement commandlet.
    // 결정적 시드와 메모리 저장을 화면 없는 측정 commandlet에 한정합니다.
    class PROJECTA_API FScopedBalanceSimulation
    {
    public:
        explicit FScopedBalanceSimulation(int32 Seed);
        ~FScopedBalanceSimulation();
        FScopedBalanceSimulation(const FScopedBalanceSimulation&) = delete;
        FScopedBalanceSimulation& operator=(const FScopedBalanceSimulation&) = delete;
    };
#endif
}
