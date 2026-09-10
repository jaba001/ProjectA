#pragma once

#include "CoreMinimal.h"
#include "Game/Run/Authority/RunAuthorityTypes.h"

class FLocalRunAuthorityStore;

// The owned OS handle excludes another execution, including a second PIE world in this process.
// 소유한 OS 핸들은 같은 프로세스의 두 번째 PIE 월드를 포함한 다른 실행을 배제합니다.
class PROJECTA_API FLocalRunAuthorityLease
{
public:
    ~FLocalRunAuthorityLease();
    FLocalRunAuthorityLease(const FLocalRunAuthorityLease&) = delete;
    FLocalRunAuthorityLease& operator=(const FLocalRunAuthorityLease&) = delete;

    bool IsValid() const;
    const FRunAuthorityStamp& GetStamp() const;

private:
    friend class FLocalRunAuthorityStore;
    FLocalRunAuthorityLease(void* InHandle, const FString& InNamespace, const FRunAuthorityStamp& InStamp);
    void* Handle = nullptr;
    FString StoreNamespace;
    FRunAuthorityStamp Stamp;
};

// This Win64 development store coordinates processes on one machine; it is not an online backend.
// 이 Win64 개발용 저장소는 한 컴퓨터의 프로세스를 조정하며 온라인 백엔드가 아닙니다.
// Call on the game thread because native SaveGame serialization creates and reads UObjects.
// 네이티브 SaveGame 직렬화가 UObject를 생성하고 읽으므로 게임 스레드에서 호출합니다.
class PROJECTA_API FLocalRunAuthorityStore
{
public:
    static constexpr int32 MaximumPayloadBytes = 3 * 1024 * 1024;

    explicit FLocalRunAuthorityStore(const FString& InNamespace = TEXT("LocalDevelopment"));
    bool IsValid() const;
    FString GetSlotName(const FGuid& RunId) const;

    // A successful creation starts the initial execution without increasing its supplied Host epoch.
    // 생성 성공 시 입력한 Host 에포크를 증가시키지 않고 최초 실행을 시작합니다.
    ERunAuthorityResult Create(const FGuid& RunId, int32 InitialHostEpoch, const TArray<uint8>& Payload, TUniquePtr<FLocalRunAuthorityLease>& OutLease, FRunAuthorityRecordData& OutRecord, FText& OutError) const;
    ERunAuthorityResult Read(const FGuid& RunId, FRunAuthorityRecordData& OutRecord, FText& OutError) const;

    // Acquisition is explicit and requires the caller to prepare approved data for exactly the next epoch.
    // 획득은 명시적이며 호출자가 정확히 다음 에포크의 승인된 데이터를 준비해야 합니다.
    ERunAuthorityResult Acquire(const FRunAuthorityStamp& ExpectedStamp, int32 NewHostEpoch, const TArray<uint8>& Payload, TUniquePtr<FLocalRunAuthorityLease>& OutLease, FRunAuthorityRecordData& OutRecord, FText& OutError) const;
    ERunAuthorityResult Commit(FLocalRunAuthorityLease& Lease, int64 ExpectedRevision, const TArray<uint8>& Payload, FRunAuthorityRecordData& OutRecord, FText& OutError) const;

private:
    FString StoreNamespace;
};
