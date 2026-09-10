#include "Game/Run/RunCheckpointStorage.h"
#include "GameFramework/SaveGame.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    constexpr int64 MaximumSaveBytes = 4 * 1024 * 1024;

#if WITH_DEV_AUTOMATION_TESTS
    bool bFailNextWrite = false;
#endif

    FString GetSlotPath(const FString& Slot)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav")));
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

USaveGame* FRunCheckpointStorage::Load(const FString& Slot, FText& OutError)
{
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
        OutError = FText::GetEmpty();
    }
    return Save;
}

#if WITH_DEV_AUTOMATION_TESTS
void FRunCheckpointStorage::FailNextWriteForTesting()
{
    bFailNextWrite = true;
}
#endif
