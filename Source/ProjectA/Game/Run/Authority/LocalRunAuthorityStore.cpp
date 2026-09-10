#include "Game/Run/Authority/LocalRunAuthorityStore.h"
#include "Game/Run/Authority/LocalRunAuthorityRecord.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    ERunAuthorityResult Fail(ERunAuthorityResult Result, FText& OutError)
    {
        switch (Result)
        {
        case ERunAuthorityResult::InvalidRequest:
            OutError = NSLOCTEXT("RunAuthority", "InvalidRequest", "권위 저장 요청의 이름, 버전 또는 실행 권한이 올바르지 않습니다.");
            break;
        case ERunAuthorityResult::NotFound:
            OutError = NSLOCTEXT("RunAuthority", "NotFound", "권위 저장소에 해당 Run이 없습니다.");
            break;
        case ERunAuthorityResult::AlreadyExists:
            OutError = NSLOCTEXT("RunAuthority", "AlreadyExists", "해당 Run의 권위 저장이 이미 존재합니다.");
            break;
        case ERunAuthorityResult::Busy:
            OutError = NSLOCTEXT("RunAuthority", "Busy", "다른 실행 또는 저장 작업이 해당 Run의 잠금을 보유하고 있습니다.");
            break;
        case ERunAuthorityResult::Conflict:
            OutError = NSLOCTEXT("RunAuthority", "Conflict", "권위 저장 버전이 변경되었습니다. 최신 기록을 다시 확인해야 합니다.");
            break;
        case ERunAuthorityResult::InvalidRecord:
            OutError = NSLOCTEXT("RunAuthority", "InvalidRecord", "권위 저장 파일이 손상되었거나 지원하지 않는 형식입니다.");
            break;
        case ERunAuthorityResult::UnsupportedPlatform:
            OutError = NSLOCTEXT("RunAuthority", "UnsupportedPlatform", "현재 로컬 권위 저장소는 Win64에서만 지원합니다.");
            break;
        default:
            OutError = NSLOCTEXT("RunAuthority", "StorageFailure", "권위 저장을 완료하지 못했습니다. 이전 확정 기록을 유지합니다.");
            break;
        }
        return Result;
    }

    bool IsSafeNamespace(const FString& Namespace)
    {
        return !Namespace.IsEmpty() && Namespace.Len() <= 32 && FRunCheckpointStorage::IsSafeSlotName(Namespace);
    }

    bool IsValidPayload(const TArray<uint8>& Payload)
    {
        return !Payload.IsEmpty() && Payload.Num() <= FLocalRunAuthorityStore::MaximumPayloadBytes;
    }

    FString GetSavePath(const FString& Slot)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav")));
    }

    // Lock files are never replaced or removed while an execution may hold their handles.
    // 실행이 핸들을 보유할 수 있는 동안 잠금 파일은 교체하거나 제거하지 않습니다.
    class FExclusiveFileHandle
    {
    public:
        ~FExclusiveFileHandle()
        {
#if PLATFORM_WINDOWS
            if (Handle)
            {
                ::CloseHandle(Handle);
            }
#endif
        }

        void* Detach()
        {
            void* Result = Handle;
            Handle = nullptr;
            return Result;
        }

        void* Handle = nullptr;
    };

    ERunAuthorityResult LockExclusive(const FString& Path, FExclusiveFileHandle& OutHandle, FText& OutError)
    {
#if !PLATFORM_WINDOWS
        return Fail(ERunAuthorityResult::UnsupportedPlatform, OutError);
#else
        if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path)))
        {
            return Fail(ERunAuthorityResult::StorageFailure, OutError);
        }
        // A share-zero file handle also excludes another handle opened by the same thread.
        // 공유 권한이 없는 파일 핸들은 같은 스레드에서 연 다른 핸들도 배제합니다.
        HANDLE Handle = ::CreateFileW(*Path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (Handle == INVALID_HANDLE_VALUE)
        {
            const DWORD Error = ::GetLastError();
            return Fail(Error == ERROR_SHARING_VIOLATION || Error == ERROR_LOCK_VIOLATION ? ERunAuthorityResult::Busy : ERunAuthorityResult::StorageFailure, OutError);
        }
        OutHandle.Handle = Handle;
        return ERunAuthorityResult::Success;
#endif
    }

    ERunAuthorityResult ReadUnlocked(const FString& Slot, const FGuid& RunId, FRunAuthorityRecordData& OutRecord, FText& OutError)
    {
        if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*GetSavePath(Slot)))
        {
            return Fail(ERunAuthorityResult::NotFound, OutError);
        }
        TStrongObjectPtr<USaveGame> Loaded(FRunCheckpointStorage::Load(Slot, OutError));
        const ULocalRunAuthorityRecord* Record = Cast<ULocalRunAuthorityRecord>(Loaded.Get());
        if (!Record || Record->GetClass() != ULocalRunAuthorityRecord::StaticClass() || Record->Version != 1 || !Record->Stamp.IsValid() || Record->Stamp.RunId != RunId || !IsValidPayload(Record->Payload))
        {
            return Fail(ERunAuthorityResult::InvalidRecord, OutError);
        }
        FRunAuthorityRecordData Candidate;
        Candidate.Stamp = Record->Stamp;
        Candidate.Payload = Record->Payload;
        OutRecord = MoveTemp(Candidate);
        OutError = FText::GetEmpty();
        return ERunAuthorityResult::Success;
    }

    ERunAuthorityResult WriteUnlocked(const FString& Slot, const FRunAuthorityRecordData& Candidate, FText& OutError)
    {
        TStrongObjectPtr<ULocalRunAuthorityRecord> Record(NewObject<ULocalRunAuthorityRecord>());
        Record->Stamp = Candidate.Stamp;
        Record->Payload = Candidate.Payload;
        if (!FRunCheckpointStorage::Save(Record.Get(), Slot, OutError))
        {
            return Fail(ERunAuthorityResult::StorageFailure, OutError);
        }
        OutError = FText::GetEmpty();
        return ERunAuthorityResult::Success;
    }
}

FLocalRunAuthorityLease::FLocalRunAuthorityLease(void* InHandle, const FString& InNamespace, const FRunAuthorityStamp& InStamp) : Handle(InHandle), StoreNamespace(InNamespace), Stamp(InStamp)
{
}

FLocalRunAuthorityLease::~FLocalRunAuthorityLease()
{
#if PLATFORM_WINDOWS
    if (Handle)
    {
        ::CloseHandle(Handle);
    }
#endif
    Handle = nullptr;
}

bool FLocalRunAuthorityLease::IsValid() const
{
    return Handle != nullptr && Stamp.IsValid();
}

const FRunAuthorityStamp& FLocalRunAuthorityLease::GetStamp() const
{
    return Stamp;
}

FLocalRunAuthorityStore::FLocalRunAuthorityStore(const FString& InNamespace)
{
    if (IsSafeNamespace(InNamespace))
    {
        StoreNamespace = InNamespace.ToLower();
    }
}

bool FLocalRunAuthorityStore::IsValid() const
{
    return !StoreNamespace.IsEmpty();
}

FString FLocalRunAuthorityStore::GetSlotName(const FGuid& RunId) const
{
    return IsValid() && RunId.IsValid() ? TEXT("ProjectA_Authority_") + StoreNamespace + TEXT("_") + RunId.ToString(EGuidFormats::Digits) : FString();
}

ERunAuthorityResult FLocalRunAuthorityStore::Create(const FGuid& RunId, int32 InitialHostEpoch, const TArray<uint8>& Payload, TUniquePtr<FLocalRunAuthorityLease>& OutLease, FRunAuthorityRecordData& OutRecord, FText& OutError) const
{
    const FString Slot = GetSlotName(RunId);
    if (!IsInGameThread() || Slot.IsEmpty() || InitialHostEpoch <= 0 || !IsValidPayload(Payload) || OutLease)
    {
        return Fail(ERunAuthorityResult::InvalidRequest, OutError);
    }
    FExclusiveFileHandle ExecutionLock;
    ERunAuthorityResult Result = LockExclusive(GetSavePath(Slot) + TEXT(".lease"), ExecutionLock, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    FExclusiveFileHandle TransactionLock;
    Result = LockExclusive(GetSavePath(Slot) + TEXT(".txn"), TransactionLock, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*GetSavePath(Slot)))
    {
        return Fail(ERunAuthorityResult::AlreadyExists, OutError);
    }
    FRunAuthorityRecordData Candidate;
    Candidate.Stamp.RunId = RunId;
    Candidate.Stamp.Revision = 1;
    Candidate.Stamp.HostEpoch = InitialHostEpoch;
    Candidate.Stamp.SessionId = FGuid::NewGuid();
    Candidate.Payload = Payload;
    Result = WriteUnlocked(Slot, Candidate, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    OutLease.Reset(new FLocalRunAuthorityLease(ExecutionLock.Detach(), StoreNamespace, Candidate.Stamp));
    OutRecord = MoveTemp(Candidate);
    return ERunAuthorityResult::Success;
}

ERunAuthorityResult FLocalRunAuthorityStore::Read(const FGuid& RunId, FRunAuthorityRecordData& OutRecord, FText& OutError) const
{
    const FString Slot = GetSlotName(RunId);
    if (!IsInGameThread() || Slot.IsEmpty())
    {
        return Fail(ERunAuthorityResult::InvalidRequest, OutError);
    }
    FExclusiveFileHandle TransactionLock;
    const ERunAuthorityResult Result = LockExclusive(GetSavePath(Slot) + TEXT(".txn"), TransactionLock, OutError);
    return Result == ERunAuthorityResult::Success ? ReadUnlocked(Slot, RunId, OutRecord, OutError) : Result;
}

ERunAuthorityResult FLocalRunAuthorityStore::Acquire(const FRunAuthorityStamp& ExpectedStamp, int32 NewHostEpoch, const TArray<uint8>& Payload, TUniquePtr<FLocalRunAuthorityLease>& OutLease, FRunAuthorityRecordData& OutRecord, FText& OutError) const
{
    const FString Slot = GetSlotName(ExpectedStamp.RunId);
    if (!IsInGameThread() || Slot.IsEmpty() || !ExpectedStamp.IsValid() || ExpectedStamp.HostEpoch == MAX_int32 || ExpectedStamp.Revision == MAX_int64 || NewHostEpoch != ExpectedStamp.HostEpoch + 1 || !IsValidPayload(Payload) || OutLease)
    {
        return Fail(ERunAuthorityResult::InvalidRequest, OutError);
    }
    FExclusiveFileHandle ExecutionLock;
    ERunAuthorityResult Result = LockExclusive(GetSavePath(Slot) + TEXT(".lease"), ExecutionLock, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    FExclusiveFileHandle TransactionLock;
    Result = LockExclusive(GetSavePath(Slot) + TEXT(".txn"), TransactionLock, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    FRunAuthorityRecordData Current;
    Result = ReadUnlocked(Slot, ExpectedStamp.RunId, Current, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    if (Current.Stamp != ExpectedStamp)
    {
        return Fail(ERunAuthorityResult::Conflict, OutError);
    }
    FRunAuthorityRecordData Candidate;
    Candidate.Stamp = ExpectedStamp;
    ++Candidate.Stamp.Revision;
    Candidate.Stamp.HostEpoch = NewHostEpoch;
    Candidate.Stamp.SessionId = FGuid::NewGuid();
    Candidate.Payload = Payload;
    Result = WriteUnlocked(Slot, Candidate, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    OutLease.Reset(new FLocalRunAuthorityLease(ExecutionLock.Detach(), StoreNamespace, Candidate.Stamp));
    OutRecord = MoveTemp(Candidate);
    return ERunAuthorityResult::Success;
}

ERunAuthorityResult FLocalRunAuthorityStore::Commit(FLocalRunAuthorityLease& Lease, int64 ExpectedRevision, const TArray<uint8>& Payload, FRunAuthorityRecordData& OutRecord, FText& OutError) const
{
    const FString Slot = GetSlotName(Lease.Stamp.RunId);
    if (!IsInGameThread() || Slot.IsEmpty() || !Lease.IsValid() || Lease.StoreNamespace != StoreNamespace || ExpectedRevision <= 0 || ExpectedRevision == MAX_int64 || !IsValidPayload(Payload))
    {
        return Fail(ERunAuthorityResult::InvalidRequest, OutError);
    }
    if (ExpectedRevision != Lease.Stamp.Revision)
    {
        return Fail(ERunAuthorityResult::Conflict, OutError);
    }
    FExclusiveFileHandle TransactionLock;
    ERunAuthorityResult Result = LockExclusive(GetSavePath(Slot) + TEXT(".txn"), TransactionLock, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    FRunAuthorityRecordData Current;
    Result = ReadUnlocked(Slot, Lease.Stamp.RunId, Current, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    if (Current.Stamp != Lease.Stamp)
    {
        return Fail(ERunAuthorityResult::Conflict, OutError);
    }
    FRunAuthorityRecordData Candidate;
    Candidate.Stamp = Current.Stamp;
    ++Candidate.Stamp.Revision;
    Candidate.Payload = Payload;
    Result = WriteUnlocked(Slot, Candidate, OutError);
    if (Result != ERunAuthorityResult::Success)
    {
        return Result;
    }
    Lease.Stamp = Candidate.Stamp;
    OutRecord = MoveTemp(Candidate);
    return ERunAuthorityResult::Success;
}
