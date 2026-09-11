#include "Game/Development/DevelopmentCoopSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Controller/MainMenuPlayerController.h"
#include "Kismet/GameplayStatics.h"

bool UDevelopmentCoopSubsystem::IsAvailable()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return true;
#endif
}

void UDevelopmentCoopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine && IsAvailable())
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UDevelopmentCoopSubsystem::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UDevelopmentCoopSubsystem::HandleTravelFailure);
    }
}

void UDevelopmentCoopSubsystem::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    Super::Deinitialize();
}

bool UDevelopmentCoopSubsystem::NormalizeAddress(const FString& Input, FString& Output)
{
    Output.Empty();
    FString Address = Input.TrimStartAndEnd();
    if (Address.IsEmpty() || Address.Len() > 64) return false;
    FString HostName;
    FString PortText;
    if (!Address.Split(TEXT(":"), &HostName, &PortText)) HostName = Address;
    else if (PortText.IsEmpty() || PortText.Len() > 5 || !PortText.IsNumeric()) return false;
    const int32 Port = PortText.IsEmpty() ? 7777 : FCString::Atoi(*PortText);
    if (Port < 1 || Port > 65535) return false;
    if (HostName.Equals(TEXT("localhost"), ESearchCase::IgnoreCase)) HostName = TEXT("127.0.0.1");
    TArray<FString> Octets;
    HostName.ParseIntoArray(Octets, TEXT("."), false);
    if (Octets.Num() != 4) return false;
    TArray<FString> Normalized;
    for (const FString& Octet : Octets)
    {
        if (Octet.IsEmpty() || Octet.Len() > 3) return false;
        for (TCHAR Character : Octet) if (Character < '0' || Character > '9') return false;
        const int32 Number = FCString::Atoi(*Octet);
        if (Number > 255) return false;
        Normalized.Add(FString::FromInt(Number));
    }
    for (TCHAR Character : PortText) if (Character < '0' || Character > '9') return false;
    Output = FString::Join(Normalized, TEXT(".")) + FString::Printf(TEXT(":%d"), Port);
    return true;
}

bool UDevelopmentCoopSubsystem::CanTravel(APlayerController* Controller, FText& Error) const
{
    Error = FText::FromString(TEXT("독립 실행된 메인메뉴에서 접속을 시작해 주세요."));
    const URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    return IsAvailable() && Cast<AMainMenuPlayerController>(Controller) && Controller->GetGameInstance() == GetGameInstance() && Controller->IsLocalController() && Controller->GetNetMode() == NM_Standalone && !bPending && !bActive && Run && !Run->IsManagedRun() && !Run->HasManagedLease();
}

bool UDevelopmentCoopSubsystem::Host(APlayerController* Controller, int32 Capacity, FText& Error)
{
    if (!CanTravel(Controller, Error)) return false;
    if (Capacity < 2 || Capacity > 4)
    {
        Error = FText::FromString(TEXT("협동 인원은 2~4명입니다."));
        return false;
    }
    GetGameInstance()->GetSubsystem<URunStateSubsystem>()->ResetDevelopmentRun();
    bPending = bActive = bResetAtMenu = true;
    Status = FText::FromString(TEXT("개발용 방을 여는 중입니다."));
    UGameplayStatics::OpenLevel(Controller, TEXT("/Game/User_JeHoon/LEVEL/Gameplay"), true, FString::Printf(TEXT("listen?ProjectADevCoop=%d"), Capacity));
    Error = FText::GetEmpty();
    return true;
}

bool UDevelopmentCoopSubsystem::Join(APlayerController* Controller, const FString& Address, FText& Error)
{
    if (!CanTravel(Controller, Error)) return false;
    FString URL;
    if (!NormalizeAddress(Address, URL))
    {
        Error = FText::FromString(TEXT("IPv4 주소와 포트를 입력해 주세요. 예: 127.0.0.1:7777"));
        return false;
    }
    bPending = bActive = bResetAtMenu = true;
    Status = FText::FromString(TEXT("Host에 접속 중입니다. 응답이 없으면 취소 후 주소를 확인해 주세요."));
    Controller->ClientTravel(URL + TEXT("?ProjectADevClient=1"), TRAVEL_Absolute);
    Error = FText::GetEmpty();
    return true;
}

void UDevelopmentCoopSubsystem::Leave(APlayerController* Controller)
{
    if (!IsAvailable() || !Controller || !Controller->IsLocalController() || Controller->GetGameInstance() != GetGameInstance()) return;
    bPending = bActive = false;
    Status = FText::FromString(TEXT("개발용 협동에서 나왔습니다. 새 방으로 다시 시작할 수 있습니다."));
    UGameplayStatics::OpenLevel(Controller, TEXT("/Game/User_JeHoon/LEVEL/MainMenu"), true);
}

void UDevelopmentCoopSubsystem::ArriveAtMenu()
{
    if (bResetAtMenu)
    {
        GetGameInstance()->GetSubsystem<URunStateSubsystem>()->ResetDevelopmentRun();
        bResetAtMenu = false;
        bPending = bActive = false;
    }
}

void UDevelopmentCoopSubsystem::Connected()
{
    if (!IsAvailable() || (bResetAtMenu && !bPending)) return;
    bPending = false;
    bActive = bResetAtMenu = true;
    Status = FText::GetEmpty();
}

void UDevelopmentCoopSubsystem::RecordFailure(UWorld* World, const FString& Message)
{
    if (!bActive || !World || World->GetGameInstance() != GetGameInstance()) return;
    bPending = bActive = false;
    Status = FText::FromString(TEXT("연결 실패 또는 종료: ") + Message.Left(300));
}

void UDevelopmentCoopSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString& Message)
{
    RecordFailure(World, Message);
}

void UDevelopmentCoopSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type, const FString& Message)
{
    RecordFailure(World, Message);
}
