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
    static USaveGame* Load(const FString& Slot, FText& OutError, FString* OutToken = nullptr);
    // Delete only the exact file shown for confirmation while denying concurrent replacement or writes.
    // 동시 교체 및 쓰기를 차단한 상태에서 확인 대상으로 표시한 동일 파일만 삭제합니다.
    static bool DeleteIfUnchanged(const FString& Slot, const FString& ExpectedToken, FText& OutError);

#if WITH_DEV_AUTOMATION_TESTS
    static void FailNextWriteForTesting();
    static void FailNextDeleteForTesting();
#endif
};
