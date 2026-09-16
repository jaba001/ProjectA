#include "Game/Run/RunCheckpointStorage.h"
#include "GameFramework/SaveGame.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    constexpr int64 MaximumSaveBytes = 4 * 1024 * 1024;

#if WITH_DEV_AUTOMATION_TESTS
    bool bFailNextWrite = false;
    bool bFailNextDelete = false;
#endif

    FString GetSlotPath(const FString& Slot)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav")));
    }

    FString MakeConfirmationToken(const FString& Slot, const TArray<uint8>& Bytes)
    {
        return Slot + TEXT(":") + FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString();
    }
}

bool FRunCheckpointStorage::IsSafeSlotName(const FString& Slot)
{
    if (Slot.IsEmpty() || Slot.Len() > 128)
    {
        return false;
    }
    for (const TCHAR Character : Slot)
    {
        const bool bLetterOrDigit = (Character >= TEXT('A') && Character <= TEXT('Z')) || (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('0') && Character <= TEXT('9'));
        if (!bLetterOrDigit && Character != TEXT('_') && Character != TEXT('-'))
        {
            return false;
        }
    }
    // Windows device names remain reserved even with a .sav or temporary-file extension.
    // Windows 장치 이름은 .sav 또는 임시 파일 확장자를 붙여도 예약된 이름입니다.
    const FString Upper = Slot.ToUpper();
    const bool bReservedPort = Upper.Len() == 4 && (Upper.StartsWith(TEXT("COM")) || Upper.StartsWith(TEXT("LPT"))) && Upper[3] >= TEXT('1') && Upper[3] <= TEXT('9');
    if (Upper == TEXT("CON") || Upper == TEXT("PRN") || Upper == TEXT("AUX") || Upper == TEXT("NUL") || bReservedPort)
    {
        return false;
    }
    return true;
}

bool FRunCheckpointStorage::Save(USaveGame* SaveGame, const FString& Slot, FText& OutError)
{
    OutError = NSLOCTEXT("RunCheckpoint", "Write", "진행을 저장하지 못했습니다. 이전 확정 저장을 유지합니다.");
    if (!IsSafeSlotName(Slot) || !IsValid(SaveGame))
    {
        return false;
    }
#if !PLATFORM_WINDOWS
    OutError = NSLOCTEXT("RunCheckpoint", "Platform", "현재 체크포인트 파일 교체는 Win64에서만 지원합니다.");
    return false;
#else
    TArray<uint8> Bytes;
    if (!UGameplayStatics::SaveGameToMemory(SaveGame, Bytes) || Bytes.IsEmpty() || Bytes.Num() > MaximumSaveBytes)
    {
        return false;
    }
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString Destination = GetSlotPath(Slot);
    if (!PlatformFile.CreateDirectoryTree(*FPaths::GetPath(Destination)))
    {
        return false;
    }
    const FString Temporary = Destination + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    bool bWritten = false;
    {
        TUniquePtr<IFileHandle> Handle(PlatformFile.OpenWrite(*Temporary));
        bWritten = Handle && Handle->Write(Bytes.GetData(), Bytes.Num()) && Handle->Flush(true);
    }
    TArray<uint8> ReadBack;
    if (!bWritten || !FFileHelper::LoadFileToArray(ReadBack, *Temporary) || ReadBack != Bytes)
    {
        PlatformFile.DeleteFile(*Temporary);
        return false;
    }
#if WITH_DEV_AUTOMATION_TESTS
    if (bFailNextWrite)
    {
        bFailNextWrite = false;
        PlatformFile.DeleteFile(*Temporary);
        return false;
    }
#endif
    // Both paths share a directory; never delete the destination before replacement.
    // 두 경로는 같은 디렉터리이며 교체 전에 목적지 파일을 삭제하지 않습니다.
    const bool bReplaced = ::MoveFileExW(*Temporary, *Destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!bReplaced)
    {
        PlatformFile.DeleteFile(*Temporary);
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
#endif
}

USaveGame* FRunCheckpointStorage::Load(const FString& Slot, FText& OutError, FString* OutToken)
{
    if (OutToken) OutToken->Reset();
    OutError = NSLOCTEXT("RunCheckpoint", "Read", "이어할 저장이 없거나 파일이 손상되었습니다.");
    if (!IsSafeSlotName(Slot))
    {
        return nullptr;
    }
    const FString Path = GetSlotPath(Slot);
    const int64 Size = FPlatformFileManager::Get().GetPlatformFile().FileSize(*Path);
    if (Size <= 0 || Size > MaximumSaveBytes)
    {
        return nullptr;
    }
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != Size)
    {
        return nullptr;
    }
    USaveGame* Save = UGameplayStatics::LoadGameFromMemory(Bytes);
    if (Save)
    {
        if (OutToken) *OutToken = MakeConfirmationToken(Slot, Bytes);
        OutError = FText::GetEmpty();
    }
    return Save;
}

bool FRunCheckpointStorage::DeleteIfUnchanged(const FString& Slot, const FString& ExpectedToken, FText& OutError)
{
    OutError = NSLOCTEXT("RunCheckpoint", "Delete", "진행을 포기하지 못했습니다. 기존 저장을 유지하며 다시 시도할 수 있습니다.");
    if (!IsSafeSlotName(Slot) || ExpectedToken.IsEmpty()) return false;
#if !PLATFORM_WINDOWS
    OutError = NSLOCTEXT("RunCheckpoint", "DeletePlatform", "현재 진행 포기는 Win64에서만 지원합니다.");
    return false;
#else
    const FString Path = GetSlotPath(Slot);
    // Use exclusive access so closing this handle completes deletion before the next save.
    // 이 핸들을 닫으면 다음 저장 전에 삭제가 완료되도록 독점 접근을 사용합니다.
    HANDLE Handle = ::CreateFileW(*Path, GENERIC_READ | DELETE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (Handle == INVALID_HANDLE_VALUE) return false;
    bool bDeleted = false;
    LARGE_INTEGER Size;
    BY_HANDLE_FILE_INFORMATION Info;
    if (::GetFileInformationByHandle(Handle, &Info) && !(Info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) && ::GetFileSizeEx(Handle, &Size) && Size.QuadPart > 0 && Size.QuadPart <= MaximumSaveBytes)
    {
        TArray<uint8> Bytes;
        Bytes.SetNumUninitialized(static_cast<int32>(Size.QuadPart));
        DWORD ReadBytes = 0;
        if (::ReadFile(Handle, Bytes.GetData(), static_cast<DWORD>(Bytes.Num()), &ReadBytes, nullptr) && ReadBytes == static_cast<DWORD>(Bytes.Num()))
        {
            if (MakeConfirmationToken(Slot, Bytes) != ExpectedToken)
            {
                OutError = NSLOCTEXT("RunCheckpoint", "DeleteChanged", "확인 후 저장이 변경되었습니다. 최신 진행을 확인한 뒤 다시 포기해 주세요.");
            }
            else
            {
                bool bAllowDelete = true;
#if WITH_DEV_AUTOMATION_TESTS
                if (bFailNextDelete)
                {
                    bFailNextDelete = false;
                    bAllowDelete = false;
                }
#endif
                if (bAllowDelete)
                {
                    FILE_DISPOSITION_INFO Disposition = { 1 };
                    bDeleted = ::SetFileInformationByHandle(Handle, FileDispositionInfo, &Disposition, sizeof(Disposition)) != 0;
                }
            }
        }
    }
    ::CloseHandle(Handle);
    if (bDeleted) OutError = FText::GetEmpty();
    return bDeleted;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
void FRunCheckpointStorage::FailNextWriteForTesting()
{
    bFailNextWrite = true;
}

void FRunCheckpointStorage::FailNextDeleteForTesting()
{
    bFailNextDelete = true;
}
#endif
