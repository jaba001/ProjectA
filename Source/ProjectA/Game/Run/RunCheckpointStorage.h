#pragma once

#include "CoreMinimal.h"

class USaveGame;

// Win64 storage keeps the previous file until a fully verified replacement can be published.
// Win64 저장은 검증된 교체 파일을 게시할 때까지 이전 파일을 유지합니다.
class PROJECTA_API FRunCheckpointStorage
{
public:
    static bool IsSafeSlotName(const FString& Slot);
    static bool Save(USaveGame* SaveGame, const FString& Slot, FText& OutError);
    static USaveGame* Load(const FString& Slot, FText& OutError);

#if WITH_DEV_AUTOMATION_TESTS
    static void FailNextWriteForTesting();
#endif
};
