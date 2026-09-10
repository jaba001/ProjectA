#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Run/Authority/LocalRunAuthorityRecord.h"
#include "Game/Run/Authority/LocalRunAuthorityStore.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FAuthorityStorageFixture
    {
        FString Namespace = TEXT("T6_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(24);
        FGuid RunId = FGuid::NewGuid();
        FLocalRunAuthorityStore Store{Namespace};
        TUniquePtr<FLocalRunAuthorityLease> Lease;

        ~FAuthorityStorageFixture()
        {
            Lease.Reset();
            const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Store.GetSlotName(RunId) + TEXT(".sav"));
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            PlatformFile.DeleteFile(*Path);
            PlatformFile.DeleteFile(*(Path + TEXT(".lease")));
            PlatformFile.DeleteFile(*(Path + TEXT(".txn")));
        }

        // Only byte persistence is under test; the coordinator separately validates Run game policy.
        // 여기서는 바이트 보존만 검사하며 Run 게임 정책은 조정자가 별도로 검증합니다.
        TArray<uint8> MakePayload(FName Node, int32 Epoch = 1) const
        {
            TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
            Save->Identity.RunId = RunId;
            Save->Identity.HostEpoch = Epoch;
            Save->CurrentNode = Node;
            TArray<uint8> Bytes;
            UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes);
            return Bytes;
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Store.GetSlotName(RunId) + TEXT(".sav"))));
            return Bytes;
        }
    };

    bool SameRecord(const FRunAuthorityRecordData& Left, const FRunAuthorityRecordData& Right)
    {
        return Left.Stamp == Right.Stamp && Left.Payload == Right.Payload;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunAuthorityRoundTripTest, "ProjectA.RunAuthority.RoundTripAndLease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunAuthorityRoundTripTest::RunTest(const FString& Parameters)
{
    FAuthorityStorageFixture Fixture;
    FText Error;
    const TArray<uint8> InitialPayload = Fixture.MakePayload(TEXT("InitialNode"));
    FRunAuthorityRecordData Initial;
    if (!TestTrue(TEXT("Create starts one initial execution"), Fixture.Store.Create(Fixture.RunId, 1, InitialPayload, Fixture.Lease, Initial, Error) == ERunAuthorityResult::Success))
    {
        return false;
    }
    TestTrue(TEXT("Creation returns a live lease"), Fixture.Lease && Fixture.Lease->IsValid());
    TestTrue(TEXT("The initial store stamp is revision one and epoch one"), Initial.Stamp.IsValid() && Initial.Stamp.Revision == 1 && Initial.Stamp.HostEpoch == 1);
    TestTrue(TEXT("Successful creation clears the error"), Error.IsEmpty());

    FLocalRunAuthorityStore SecondStore(Fixture.Namespace.ToUpper());
    FRunAuthorityRecordData Read;
    TestTrue(TEXT("Another store reads while the execution lease is held"), SecondStore.Read(Fixture.RunId, Read, Error) == ERunAuthorityResult::Success);
    TestTrue(TEXT("The complete native payload and stamp round trip"), SameRecord(Initial, Read));
    TStrongObjectPtr<USaveGame> Loaded(UGameplayStatics::LoadGameFromMemory(Read.Payload));
    const URunSaveGame* NativeRun = Cast<URunSaveGame>(Loaded.Get());
    TestTrue(TEXT("The canonical payload restores without an original Host's separate save slot"), NativeRun && NativeRun->Identity.RunId == Fixture.RunId && NativeRun->CurrentNode == TEXT("InitialNode"));

    TUniquePtr<FLocalRunAuthorityLease> OtherLease;
    FRunAuthorityRecordData Output = Initial;
    const TArray<uint8> SuccessorPayload = Fixture.MakePayload(TEXT("SuccessorNode"), 2);
    TestTrue(TEXT("A second handle on the same game thread cannot acquire the run"), SecondStore.Acquire(Initial.Stamp, 2, SuccessorPayload, OtherLease, Output, Error) == ERunAuthorityResult::Busy);
    TestTrue(TEXT("A busy acquisition preserves its output and issues no lease"), !OtherLease && SameRecord(Output, Initial));

    FRunAuthorityRecordData Committed;
    const TArray<uint8> NextPayload = Fixture.MakePayload(TEXT("NextNode"));
    TestTrue(TEXT("The current lease commits once"), Fixture.Store.Commit(*Fixture.Lease, 1, NextPayload, Committed, Error) == ERunAuthorityResult::Success);
    TestTrue(TEXT("A commit advances only the store revision"), Committed.Stamp.Revision == 2 && Committed.Stamp.HostEpoch == 1 && Committed.Stamp.SessionId == Initial.Stamp.SessionId);
    TestTrue(TEXT("The lease stamp advances with the committed record"), Fixture.Lease->GetStamp() == Committed.Stamp);
    Output = Initial;
    TestTrue(TEXT("An old revision cannot be replayed with the current lease"), SecondStore.Commit(*Fixture.Lease, 1, InitialPayload, Output, Error) == ERunAuthorityResult::Conflict);
    TestTrue(TEXT("Rejected replay preserves its output"), SameRecord(Output, Initial));
    FLocalRunAuthorityStore WrongStore(TEXT("T6_OtherNamespace"));
    TestTrue(TEXT("A lease cannot write another namespace"), WrongStore.Commit(*Fixture.Lease, 2, NextPayload, Output, Error) == ERunAuthorityResult::InvalidRequest);

    Fixture.Lease.Reset();
    TestTrue(TEXT("Released execution still leaves the entire canonical record readable"), SecondStore.Read(Fixture.RunId, Read, Error) == ERunAuthorityResult::Success && SameRecord(Read, Committed));
    TestTrue(TEXT("Create never replaces an existing run"), Fixture.Store.Create(Fixture.RunId, 1, InitialPayload, OtherLease, Output, Error) == ERunAuthorityResult::AlreadyExists);
    TestTrue(TEXT("An explicit acquisition rejects a stale expected stamp after release"), SecondStore.Acquire(Initial.Stamp, 2, SuccessorPayload, OtherLease, Output, Error) == ERunAuthorityResult::Conflict);
    TestTrue(TEXT("An explicit acquisition rejects an epoch jump"), SecondStore.Acquire(Committed.Stamp, 3, SuccessorPayload, OtherLease, Output, Error) == ERunAuthorityResult::InvalidRequest);
    FRunAuthorityRecordData Acquired;
    if (!TestTrue(TEXT("An explicit acquisition with the latest stamp wins"), SecondStore.Acquire(Committed.Stamp, 2, SuccessorPayload, OtherLease, Acquired, Error) == ERunAuthorityResult::Success))
    {
        return false;
    }
    TestTrue(TEXT("A new execution changes its session, epoch, and global revision together"), Acquired.Stamp.Revision == 3 && Acquired.Stamp.HostEpoch == 2 && Acquired.Stamp.SessionId != Committed.Stamp.SessionId && Acquired.Payload == SuccessorPayload);
    Fixture.Lease = MoveTemp(OtherLease);
    TestTrue(TEXT("The new owner commits with its own lease"), Fixture.Store.Commit(*Fixture.Lease, 3, SuccessorPayload, Output, Error) == ERunAuthorityResult::Success);
    TestTrue(TEXT("Successor commit keeps its epoch and session"), Output.Stamp.Revision == 4 && Output.Stamp.HostEpoch == 2 && Output.Stamp.SessionId == Acquired.Stamp.SessionId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunAuthorityFailureTest, "ProjectA.RunAuthority.AtomicFailureAndFencing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunAuthorityFailureTest::RunTest(const FString& Parameters)
{
    FAuthorityStorageFixture Fixture;
    FText Error;
    FRunAuthorityRecordData Sentinel;
    Sentinel.Payload = { 91, 92 };
    FRunAuthorityRecordData Output = Sentinel;
    const TArray<uint8> Payload = Fixture.MakePayload(TEXT("Confirmed"));
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestTrue(TEXT("Failed initial publication reports storage failure"), Fixture.Store.Create(Fixture.RunId, 1, Payload, Fixture.Lease, Output, Error) == ERunAuthorityResult::StorageFailure);
    TestTrue(TEXT("Failed create issues no lease and preserves output"), !Fixture.Lease && SameRecord(Output, Sentinel));
    TestTrue(TEXT("Failed create leaves no partial authority record"), Fixture.Store.Read(Fixture.RunId, Output, Error) == ERunAuthorityResult::NotFound);
    FRunAuthorityRecordData Initial;
    if (!TestTrue(TEXT("Failed create releases both locks for a retry"), Fixture.Store.Create(Fixture.RunId, 1, Payload, Fixture.Lease, Initial, Error) == ERunAuthorityResult::Success))
    {
        return false;
    }
    const TArray<uint8> InitialBytes = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestTrue(TEXT("Failed commit reports storage failure"), Fixture.Store.Commit(*Fixture.Lease, 1, Fixture.MakePayload(TEXT("Unconfirmed")), Output, Error) == ERunAuthorityResult::StorageFailure);
    TestTrue(TEXT("Failed commit preserves exact file bytes, output, and lease stamp"), Fixture.ReadBytes() == InitialBytes && SameRecord(Output, Sentinel) && Fixture.Lease->GetStamp() == Initial.Stamp);
    TestFalse(TEXT("Write failure explains why"), Error.IsEmpty());
    FRunAuthorityRecordData Confirmed;
    TestTrue(TEXT("Failed commit releases the transaction lock"), Fixture.Store.Read(Fixture.RunId, Confirmed, Error) == ERunAuthorityResult::Success && SameRecord(Confirmed, Initial));
    FLocalRunAuthorityLease* OriginalLease = Fixture.Lease.Get();
    TestTrue(TEXT("An existing output lease is never replaced by create"), Fixture.Store.Create(Fixture.RunId, 1, Payload, Fixture.Lease, Output, Error) == ERunAuthorityResult::InvalidRequest);
    TestTrue(TEXT("Rejecting an occupied output preserves the execution handle"), Fixture.Lease.Get() == OriginalLease && Fixture.Lease->IsValid());

    // Simulate a stale persisted fence, not a legitimate concurrent writer that bypasses the lease.
    // lease를 우회한 정상 동시 쓰기가 아니라 저장된 권한 세대가 오래된 상황을 모사합니다.
    TStrongObjectPtr<ULocalRunAuthorityRecord> Newer(NewObject<ULocalRunAuthorityRecord>());
    Newer->Stamp = Initial.Stamp;
    Newer->Stamp.HostEpoch = 2;
    Newer->Stamp.Revision = 2;
    Newer->Stamp.SessionId = FGuid::NewGuid();
    Newer->Payload = Fixture.MakePayload(TEXT("NewerAuthority"), 2);
    if (!TestTrue(TEXT("The test installs a newer persisted fence"), FRunCheckpointStorage::Save(Newer.Get(), Fixture.Store.GetSlotName(Fixture.RunId), Error)))
    {
        return false;
    }
    const TArray<uint8> NewerBytes = Fixture.ReadBytes();
    TestTrue(TEXT("An old Host lease cannot commit over a newer persisted session"), Fixture.Store.Commit(*Fixture.Lease, 1, Payload, Output, Error) == ERunAuthorityResult::Conflict);
    TestTrue(TEXT("Fencing rejection leaves the newer file untouched"), Fixture.ReadBytes() == NewerBytes && SameRecord(Output, Sentinel));
    Fixture.Lease.Reset();
    FRunAuthorityRecordData Current;
    if (!TestTrue(TEXT("Latest fenced data remains readable after release"), Fixture.Store.Read(Fixture.RunId, Current, Error) == ERunAuthorityResult::Success))
    {
        return false;
    }
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestTrue(TEXT("A failed acquisition does not publish the next Host epoch"), Fixture.Store.Acquire(Current.Stamp, 3, Payload, Fixture.Lease, Output, Error) == ERunAuthorityResult::StorageFailure);
    TestTrue(TEXT("Failed acquisition keeps old bytes and output and issues no lease"), !Fixture.Lease && Fixture.ReadBytes() == NewerBytes && SameRecord(Output, Sentinel));
    TestTrue(TEXT("Failed acquisition releases both locks for an explicit retry"), Fixture.Store.Acquire(Current.Stamp, 3, Payload, Fixture.Lease, Output, Error) == ERunAuthorityResult::Success);
    TestTrue(TEXT("Successful retry clears the error and advances once"), Error.IsEmpty() && Output.Stamp.Revision == 3 && Output.Stamp.HostEpoch == 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunAuthorityValidationTest, "ProjectA.RunAuthority.RecordValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunAuthorityValidationTest::RunTest(const FString& Parameters)
{
    FAuthorityStorageFixture Fixture;
    FText Error;
    FRunAuthorityRecordData Sentinel;
    Sentinel.Payload = { 81, 82 };
    FRunAuthorityRecordData Output = Sentinel;
    const TArray<FString> UnsafeNamespaces = { TEXT(""), TEXT("../run"), TEXT("a/b"), TEXT("a\\b"), TEXT("CON"), FString::ChrN(33, TEXT('a')), TEXT("한글") };
    for (const FString& Namespace : UnsafeNamespaces)
    {
        FLocalRunAuthorityStore Invalid(Namespace);
        TestFalse(TEXT("An unsafe namespace cannot produce a path"), Invalid.IsValid());
        TestTrue(TEXT("An unsafe namespace read is rejected before file access"), Invalid.Read(Fixture.RunId, Output, Error) == ERunAuthorityResult::InvalidRequest && SameRecord(Output, Sentinel));
    }
    TestTrue(TEXT("An invalid Run ID cannot produce a slot path"), Fixture.Store.GetSlotName(FGuid()).IsEmpty());
    TestTrue(TEXT("A missing authority run preserves the output"), Fixture.Store.Read(Fixture.RunId, Output, Error) == ERunAuthorityResult::NotFound && SameRecord(Output, Sentinel));
    TestTrue(TEXT("An empty payload cannot create a record"), Fixture.Store.Create(Fixture.RunId, 1, {}, Fixture.Lease, Output, Error) == ERunAuthorityResult::InvalidRequest);
    TArray<uint8> Oversized;
    Oversized.SetNumZeroed(FLocalRunAuthorityStore::MaximumPayloadBytes + 1);
    TestTrue(TEXT("An oversized payload cannot create a record"), Fixture.Store.Create(Fixture.RunId, 1, Oversized, Fixture.Lease, Output, Error) == ERunAuthorityResult::InvalidRequest);

    TStrongObjectPtr<ULocalRunAuthorityRecord> Record(NewObject<ULocalRunAuthorityRecord>());
    Record->Stamp.RunId = Fixture.RunId;
    Record->Stamp.Revision = 1;
    Record->Stamp.HostEpoch = 1;
    Record->Stamp.SessionId = FGuid::NewGuid();
    Record->Payload = Fixture.MakePayload(TEXT("Valid"));
    const FRunAuthorityStamp ValidStamp = Record->Stamp;
    const TArray<uint8> ValidPayload = Record->Payload;
    const auto RejectRecord = [this, &Fixture, &Record, &Output, &Sentinel, &Error](const TCHAR* Label)
    {
        TestTrue(FString(Label) + TEXT(" is installed only by the test"), FRunCheckpointStorage::Save(Record.Get(), Fixture.Store.GetSlotName(Fixture.RunId), Error));
        TestTrue(Label, Fixture.Store.Read(Fixture.RunId, Output, Error) == ERunAuthorityResult::InvalidRecord);
        TestTrue(FString(Label) + TEXT(" preserves output"), SameRecord(Output, Sentinel));
    };
    Record->Version = 2;
    RejectRecord(TEXT("Unknown envelope version is rejected"));
    Record->Version = 1;
    Record->Stamp.RunId = FGuid::NewGuid();
    RejectRecord(TEXT("A record for a different Run ID is rejected"));
    Record->Stamp = ValidStamp;
    Record->Stamp.Revision = 0;
    RejectRecord(TEXT("A missing global revision is rejected"));
    Record->Stamp = ValidStamp;
    Record->Stamp.HostEpoch = 0;
    RejectRecord(TEXT("A missing Host epoch is rejected"));
    Record->Stamp = ValidStamp;
    Record->Stamp.SessionId.Invalidate();
    RejectRecord(TEXT("A missing execution session is rejected"));
    Record->Stamp = ValidStamp;
    Record->Payload.Reset();
    RejectRecord(TEXT("An empty canonical payload is rejected"));
    Record->Payload = Oversized;
    RejectRecord(TEXT("An oversized canonical payload is rejected"));

    TStrongObjectPtr<URunSaveGame> WrongClass(NewObject<URunSaveGame>());
    TestTrue(TEXT("The wrong native SaveGame type is installed only by the test"), FRunCheckpointStorage::Save(WrongClass.Get(), Fixture.Store.GetSlotName(Fixture.RunId), Error));
    TestTrue(TEXT("A Run save cannot masquerade as an authority envelope"), Fixture.Store.Read(Fixture.RunId, Output, Error) == ERunAuthorityResult::InvalidRecord && SameRecord(Output, Sentinel));

    Record->Payload = ValidPayload;
    Record->Stamp = ValidStamp;
    Record->Stamp.Revision = MAX_int64;
    TestTrue(TEXT("The maximum revision fixture is installed"), FRunCheckpointStorage::Save(Record.Get(), Fixture.Store.GetSlotName(Fixture.RunId), Error));
    TestTrue(TEXT("Acquisition rejects revision overflow"), Fixture.Store.Acquire(Record->Stamp, 2, ValidPayload, Fixture.Lease, Output, Error) == ERunAuthorityResult::InvalidRequest);
    Record->Stamp = ValidStamp;
    Record->Stamp.HostEpoch = MAX_int32;
    TestTrue(TEXT("Acquisition rejects epoch overflow before addition"), Fixture.Store.Acquire(Record->Stamp, MAX_int32, ValidPayload, Fixture.Lease, Output, Error) == ERunAuthorityResult::InvalidRequest);
    TestTrue(TEXT("All rejected acquisitions issue no execution lease"), !Fixture.Lease);
    return true;
}

namespace
{
    // This holder has no blocking sleep; the owning test process is terminated by the external probe.
    // 이 보유자는 블로킹 대기 없이 실행되며 외부 검증이 해당 테스트 프로세스를 종료합니다.
    class FAuthorityProbeHoldCommand : public IAutomationLatentCommand
    {
    public:
        FAuthorityProbeHoldCommand(FAutomationTestBase* InTest, TUniquePtr<FLocalRunAuthorityLease>&& InLease) : Test(InTest), Lease(MoveTemp(InLease)), Deadline(FPlatformTime::Seconds() + 180.0)
        {
        }

        virtual bool Update() override
        {
            if (FPlatformTime::Seconds() < Deadline)
            {
                return false;
            }
            Lease.Reset();
            Test->AddError(TEXT("Holder reached its 180-second safety limit before the external process-crash probe terminated it."));
            return true;
        }

    private:
        FAutomationTestBase* Test;
        TUniquePtr<FLocalRunAuthorityLease> Lease;
        double Deadline;
    };

    TArray<uint8> MakeProbePayload(const FGuid& RunId, int32 Epoch, FName Node)
    {
        TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
        Save->Identity.RunId = RunId;
        Save->Identity.HostEpoch = Epoch;
        Save->CurrentNode = Node;
        TArray<uint8> Bytes;
        UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes);
        return Bytes;
    }

    bool ReadProbeMarker(const FString& Path, const FString& Namespace, const FGuid& RunId, FRunAuthorityStamp& OutStamp, uint32& OutProcessId)
    {
        const int64 Size = FPlatformFileManager::Get().GetPlatformFile().FileSize(*Path);
        FString Marker;
        FString StoredNamespace;
        FString StoredRunId;
        FString StoredSessionId;
        FRunAuthorityStamp Stamp;
        uint32 ProcessId = 0;
        if (Size <= 0 || Size > 4096 || !FFileHelper::LoadFileToString(Marker, *Path))
        {
            return false;
        }
        if (!FParse::Value(*Marker, TEXT("Namespace="), StoredNamespace) || !FParse::Value(*Marker, TEXT("RunId="), StoredRunId) || !FParse::Value(*Marker, TEXT("Revision="), Stamp.Revision) || !FParse::Value(*Marker, TEXT("HostEpoch="), Stamp.HostEpoch) || !FParse::Value(*Marker, TEXT("SessionId="), StoredSessionId) || !FParse::Value(*Marker, TEXT("ProcessId="), ProcessId))
        {
            return false;
        }
        if (StoredNamespace != Namespace.ToLower() || !FGuid::Parse(StoredRunId, Stamp.RunId) || !FGuid::Parse(StoredSessionId, Stamp.SessionId) || Stamp.RunId != RunId || !Stamp.IsValid() || Stamp.Revision != 1 || Stamp.HostEpoch != 1 || ProcessId == 0)
        {
            return false;
        }
        OutStamp = Stamp;
        OutProcessId = ProcessId;
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunAuthorityProcessProbeTest, "ProjectA.RunAuthority.ProcessProbe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunAuthorityProcessProbeTest::RunTest(const FString& Parameters)
{
    FString Role;
    if (!FParse::Value(FCommandLine::Get(), TEXT("T14AuthorityProbe="), Role))
    {
        AddInfo(TEXT("Process probe is opt-in: use -T14AuthorityProbe=Holder|BusyReader|ResumeReader with T14AuthorityNamespace, T14AuthorityRunId, and T14AuthorityProbeId."));
        return true;
    }
    FString Namespace;
    FString RunIdText;
    FString ProbeIdText;
    FGuid RunId;
    FGuid ProbeId;
    if (!FParse::Value(FCommandLine::Get(), TEXT("T14AuthorityNamespace="), Namespace) || !FParse::Value(FCommandLine::Get(), TEXT("T14AuthorityRunId="), RunIdText) || !FParse::Value(FCommandLine::Get(), TEXT("T14AuthorityProbeId="), ProbeIdText) || !FGuid::Parse(RunIdText, RunId) || !RunId.IsValid() || !FGuid::Parse(ProbeIdText, ProbeId) || !ProbeId.IsValid())
    {
        AddError(TEXT("The opt-in process probe requires a safe namespace and valid Run/Probe GUIDs."));
        return false;
    }
    FLocalRunAuthorityStore Store(Namespace);
    if (!Store.IsValid() || (Role != TEXT("Holder") && Role != TEXT("BusyReader") && Role != TEXT("ResumeReader")))
    {
        AddError(TEXT("The process-probe namespace or role is invalid."));
        return false;
    }
    const FString MarkerPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / (TEXT("T14Authority_") + ProbeId.ToString(EGuidFormats::Digits) + TEXT(".ready")));
    FText Error;
    FRunAuthorityRecordData Record;
    TUniquePtr<FLocalRunAuthorityLease> Lease;
    if (Role == TEXT("Holder"))
    {
        if (!TestFalse(TEXT("A unique probe starts without a stale ready marker"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*MarkerPath)))
        {
            return false;
        }
        if (!TestTrue(TEXT("Holder publishes a canonical record with one execution lease"), Store.Create(RunId, 1, MakeProbePayload(RunId, 1, TEXT("ProbeHost")), Lease, Record, Error) == ERunAuthorityResult::Success))
        {
            AddError(Error.ToString());
            return false;
        }
        // The PID identifies this test process only; it is not an online-presence or approval claim.
        // PID는 이 테스트 프로세스만 식별하며 온라인 접속 상태나 승인을 증명하지 않습니다.
        const FString Marker = FString::Printf(TEXT("Namespace=%s\nRunId=%s\nRevision=%lld\nHostEpoch=%d\nSessionId=%s\nProcessId=%u\n"), *Namespace.ToLower(), *RunId.ToString(EGuidFormats::Digits), Record.Stamp.Revision, Record.Stamp.HostEpoch, *Record.Stamp.SessionId.ToString(EGuidFormats::Digits), FPlatformProcess::GetCurrentProcessId());
        if (!TestTrue(TEXT("Holder publishes its ready marker"), FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(MarkerPath)) && FFileHelper::SaveStringToFile(Marker, *MarkerPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
        {
            return false;
        }
        AddInfo(FString::Printf(TEXT("Holder ready: %s; terminate only its recorded test PID after BusyReader passes."), *MarkerPath));
        ADD_LATENT_AUTOMATION_COMMAND(FAuthorityProbeHoldCommand(this, MoveTemp(Lease)));
        return true;
    }

    FRunAuthorityStamp OriginalStamp;
    uint32 HolderProcessId = 0;
    if (!TestTrue(TEXT("Reader loads the exact original stamp from the safe ready marker"), ReadProbeMarker(MarkerPath, Namespace, RunId, OriginalStamp, HolderProcessId)) || !TestTrue(TEXT("The reader is a separate OS process from the holder"), HolderProcessId != FPlatformProcess::GetCurrentProcessId()))
    {
        return false;
    }
    if (!TestTrue(TEXT("Reader retrieves the canonical body without the original Host's separate save slot"), Store.Read(RunId, Record, Error) == ERunAuthorityResult::Success))
    {
        AddError(Error.ToString());
        return false;
    }
    if (!TestTrue(TEXT("The original confirmed stamp survives across processes"), Record.Stamp == OriginalStamp))
    {
        return false;
    }
    TStrongObjectPtr<USaveGame> Loaded(UGameplayStatics::LoadGameFromMemory(Record.Payload));
    const URunSaveGame* SavedRun = Cast<URunSaveGame>(Loaded.Get());
    if (!TestTrue(TEXT("The retrieved native payload retains Run identity, epoch, and content"), SavedRun && SavedRun->Identity.RunId == RunId && SavedRun->Identity.HostEpoch == 1 && SavedRun->CurrentNode == TEXT("ProbeHost")))
    {
        return false;
    }
    FRunAuthorityRecordData Output;
    Output.Payload = { 71, 72 };
    const FRunAuthorityRecordData Sentinel = Output;
    const TArray<uint8> ResumePayload = MakeProbePayload(RunId, 2, TEXT("ProbeResume"));
    if (Role == TEXT("BusyReader"))
    {
        TestTrue(TEXT("A different process cannot acquire while the Holder owns the OS lease"), Store.Acquire(OriginalStamp, 2, ResumePayload, Lease, Output, Error) == ERunAuthorityResult::Busy);
        TestTrue(TEXT("Busy preserves the reader output and issues no lease"), !Lease && SameRecord(Output, Sentinel));
        FRunAuthorityRecordData Unchanged;
        TestTrue(TEXT("Busy preserves the full canonical record"), Store.Read(RunId, Unchanged, Error) == ERunAuthorityResult::Success && SameRecord(Unchanged, Record));
        return true;
    }

    if (!TestTrue(TEXT("Explicit recovery acquires the OS lease after the Holder process exits"), Store.Acquire(OriginalStamp, 2, ResumePayload, Lease, Output, Error) == ERunAuthorityResult::Success))
    {
        AddError(Error.ToString());
        return false;
    }
    TestTrue(TEXT("Recovery atomically advances epoch and revision and rotates the session"), Lease && Lease->IsValid() && Output.Stamp.Revision == 2 && Output.Stamp.HostEpoch == 2 && Output.Stamp.SessionId.IsValid() && Output.Stamp.SessionId != OriginalStamp.SessionId && Output.Payload == ResumePayload);
    const FRunAuthorityStamp RecoveryStamp = Output.Stamp;
    FRunAuthorityRecordData Committed;
    const TArray<uint8> CommittedPayload = MakeProbePayload(RunId, 2, TEXT("ProbeCommitted"));
    if (!TestTrue(TEXT("The recovered execution performs a real canonical commit"), Store.Commit(*Lease, 2, CommittedPayload, Committed, Error) == ERunAuthorityResult::Success))
    {
        return false;
    }
    TestTrue(TEXT("Recovered commit advances revision while preserving its new epoch and session"), Committed.Stamp.Revision == 3 && Committed.Stamp.HostEpoch == 2 && Committed.Stamp.SessionId == RecoveryStamp.SessionId && Committed.Payload == CommittedPayload);
    FRunAuthorityRecordData ReadBack;
    TestTrue(TEXT("The committed recovered body reads back exactly"), Store.Read(RunId, ReadBack, Error) == ERunAuthorityResult::Success && SameRecord(ReadBack, Committed));
    Lease.Reset();
    TestTrue(TEXT("The original pre-crash stamp cannot acquire again after recovery"), Store.Acquire(OriginalStamp, 2, ResumePayload, Lease, Output, Error) == ERunAuthorityResult::Conflict);
    TestTrue(TEXT("A stale original acquisition leaves the recovered record intact"), !Lease && Store.Read(RunId, ReadBack, Error) == ERunAuthorityResult::Success && SameRecord(ReadBack, Committed));
    return true;
}

#endif
