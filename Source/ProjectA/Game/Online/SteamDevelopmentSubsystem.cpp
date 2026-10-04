#include "Game/Online/SteamDevelopmentSubsystem.h"

#include "Controller/MainMenuPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "Game/Online/SteamDevelopmentLobby.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#if PROJECTA_WITH_STEAM_DEV
#include "OnlineAuthInterfaceUtilsSteam.h"
#include "OnlineSubsystemSteam.h"
#endif

namespace
{
    const FName ProbeKey(TEXT("PROJECTA_PROBE"));
    const FString ProbeValue(TEXT("ProjectA.SteamDev.480.v1"));
    const FString DriverPath(TEXT("/Script/SteamSockets.SteamSocketsNetDriver"));
    const FName MenuMap(TEXT("/Game/User_JeHoon/LEVEL/Core/MainMenu"));
}

bool USteamDevelopmentSubsystem::IsRequested()
{
#if PROJECTA_WITH_STEAM_DEV
    return !IsRunningCommandlet() && IsRunningGame() && FParse::Param(FCommandLine::Get(), TEXT("ProjectASteamDev")) && FConfigCacheIni::GetCustomConfigString() == TEXT("SteamDev");
#else
    return false;
#endif
}

void USteamDevelopmentSubsystem::SetStatus(const FString& Message)
{
    Status = FText::FromString(Message);
}

void USteamDevelopmentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!IsRequested()) return;
    SetStatus(TEXT("Steam 연결 확인을 준비하지 못했습니다. Steam 로그인과 별도 개발 설정을 확인해 주세요."));
#if PROJECTA_WITH_STEAM_DEV
    Steam = IOnlineSubsystem::Get(TEXT("STEAM"));
    if (!Steam || Steam->GetAppId() != TEXT("480")) return;
    Sessions = Steam->GetSessionInterface();
    const IOnlineIdentityPtr Identity = Steam->GetIdentityInterface();
    if (!Sessions || !Identity || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn) return;
    LocalId = Identity->GetUniquePlayerId(0);
    if (!LocalId || !LocalId->IsValid() || LocalId->GetType() != FName(TEXT("STEAM"))) return;
    const FOnlineAuthSteamUtilsPtr Auth = static_cast<FOnlineSubsystemSteam*>(Steam)->GetAuthInterfaceUtils();
    if (!Auth || !Auth->IsSteamAuthEnabled() || Auth->OnAuthenticationResultDelegate.IsBound() || Auth->OnAuthenticationResultWithCodeDelegate.IsBound() || Auth->OverrideFailureDelegate.IsBound())
    {
        SetStatus(TEXT("SteamAuth가 꺼져 있거나 다른 인증 처리기가 사용 중입니다. 연결 확인을 시작하지 않습니다."));
        return;
    }
    // Observe the official verdict without replacing SteamAuth's default failure kick.
    // SteamAuth의 기본 실패 추방을 대체하지 않고 공식 판정만 관측합니다.
    Auth->OnAuthenticationResultDelegate.BindUObject(this, &USteamDevelopmentSubsystem::HandleAuthentication);
    bOwnsAuthenticationDelegate = true;
    InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &USteamDevelopmentSubsystem::HandleInvite));
    if (GEngine)
    {
        NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &USteamDevelopmentSubsystem::HandleNetworkFailure);
        TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &USteamDevelopmentSubsystem::HandleTravelFailure);
    }
    SetStatus(TEXT("Steam 480 개발 연결 준비 · 전송 확인 전용 · Run과 저장에는 접근하지 않습니다."));
#endif
}

void USteamDevelopmentSubsystem::ClearSessionDelegates()
{
    if (!Sessions) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    CreateHandle.Reset();
    FindHandle.Reset();
    JoinHandle.Reset();
    DestroyHandle.Reset();
}

void USteamDevelopmentSubsystem::Deinitialize()
{
    ClearSessionDelegates();
    if (Sessions)
    {
        Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
        if (Operation == EOperation::Finding) Sessions->CancelFindSessions();
        if (HasSession()) Sessions->DestroySession(NAME_GameSession);
    }
#if PROJECTA_WITH_STEAM_DEV
    if (bOwnsAuthenticationDelegate && Steam && IOnlineSubsystem::IsLoaded(TEXT("STEAM")))
    {
        const FOnlineAuthSteamUtilsPtr Auth = static_cast<FOnlineSubsystemSteam*>(Steam)->GetAuthInterfaceUtils();
        if (Auth && Auth->OnAuthenticationResultDelegate.IsBoundToObject(this)) Auth->OnAuthenticationResultDelegate.Unbind();
    }
#endif
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkHandle);
        GEngine->OnTravelFailure().Remove(TravelHandle);
    }
    Sessions.Reset();
    Steam = nullptr;
    Super::Deinitialize();
}

bool USteamDevelopmentSubsystem::IsReady() const
{
    if (!IsRequested() || !Steam || Steam->GetAppId() != TEXT("480") || !Sessions || !LocalId || !LocalId->IsValid() || !bOwnsAuthenticationDelegate || !GEngine) return false;
    const IOnlineIdentityPtr Identity = Steam->GetIdentityInterface();
    const FUniqueNetIdPtr CurrentId = Identity ? Identity->GetUniquePlayerId(0) : nullptr;
    const FNetDriverDefinition* Driver = GEngine->NetDriverDefinitions.FindByPredicate([](const FNetDriverDefinition& Entry) { return Entry.DefName == NAME_GameNetDriver; });
    return Identity && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn && CurrentId && *CurrentId == *LocalId && Driver && Driver->DriverClassName.ToString() == DriverPath && Driver->DriverClassNameFallback.ToString() == DriverPath;
}

bool USteamDevelopmentSubsystem::HasSession() const
{
    return bOwnsSession && Sessions && Sessions->GetNamedSession(NAME_GameSession);
}

bool USteamDevelopmentSubsystem::CanBegin(APlayerController* Controller)
{
    const URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    const UDevelopmentCoopSubsystem* LocalCoop = GetGameInstance()->GetSubsystem<UDevelopmentCoopSubsystem>();
    if (!IsReady() || IsBusy() || Sessions->GetNamedSession(NAME_GameSession) || !Cast<AMainMenuPlayerController>(Controller) || !Controller->IsLocalController() || Controller->GetGameInstance() != GetGameInstance() || Controller->GetNetMode() != NM_Standalone || !Run || Run->GetPhase() != ERunPhase::None || Run->IsManagedRun() || Run->HasManagedLease() || (LocalCoop && LocalCoop->IsPending()))
    {
        SetStatus(TEXT("Steam 로그인·480·SteamSockets 설정과 빈 메인메뉴가 필요합니다. 진행 중 Run이나 기존 방을 대체하지 않습니다."));
        return false;
    }
    RequestController = Controller;
    bLeaveRequested = false;
    return true;
}

bool USteamDevelopmentSubsystem::Host(APlayerController* Controller, int32 Capacity)
{
    if (!CanBegin(Controller)) return false;
    if (Capacity < 2 || Capacity > 4)
    {
        SetStatus(TEXT("Steam 연결 확인 인원은 2~4명입니다."));
        return false;
    }
    FOnlineSessionSettings Settings;
    Settings.NumPublicConnections = Capacity;
    Settings.bIsLANMatch = false;
    Settings.bShouldAdvertise = true;
    Settings.bAllowInvites = true;
    // Steam uses this flag for lobby joinability; this probe never starts a game Run.
    // Steam은 이 플래그로 로비 참가를 허용하며 이 검증 도구는 게임 Run을 시작하지 않습니다.
    Settings.bAllowJoinInProgress = true;
    Settings.bUsesPresence = true;
    Settings.bAllowJoinViaPresence = false;
    Settings.bAllowJoinViaPresenceFriendsOnly = true;
    Settings.bUseLobbiesIfAvailable = true;
    Settings.Set(ProbeKey, ProbeValue, EOnlineDataAdvertisementType::ViaOnlineService);
    SessionCapacity = Capacity;
    AuthenticatedUsers.Reset();
    Operation = EOperation::Creating;
    bOwnsSession = true;
    SetStatus(TEXT("Steam 친구 전용 연결 확인 방을 만드는 중입니다."));
    CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &USteamDevelopmentSubsystem::HandleCreated));
    if (Sessions->CreateSession(*LocalId, NAME_GameSession, Settings)) return true;
    HandleCreated(NAME_GameSession, false);
    return false;
}

void USteamDevelopmentSubsystem::HandleCreated(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession || Operation != EOperation::Creating) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    CreateHandle.Reset();
    Operation = EOperation::Idle;
    if (!bSuccess)
    {
        SetStatus(TEXT("Steam 방 생성에 실패했습니다. 로컬 방으로 대체하지 않습니다."));
        return;
    }
    if (bLeaveRequested || !IsReady() || !RequestController.IsValid())
    {
        Leave();
        return;
    }
    SetStatus(TEXT("SteamSockets 연결 확인 방으로 이동 중입니다."));
    UGameplayStatics::OpenLevel(RequestController.Get(), MenuMap, true, TEXT("listen?game=/Script/ProjectA.SteamDevelopmentLobbyGameMode"));
}

bool USteamDevelopmentSubsystem::Find(APlayerController* Controller)
{
    if (!CanBegin(Controller)) return false;
    Results.Reset();
    ResultLabels.Reset();
    Search = MakeShared<FOnlineSessionSearch>();
    Search->MaxSearchResults = 50;
    Search->bIsLanQuery = false;
    Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    Search->QuerySettings.Set(ProbeKey, ProbeValue, EOnlineComparisonOp::Equals);
    Operation = EOperation::Finding;
    SetStatus(TEXT("같은 ProjectA 480 연결 확인 방을 찾는 중입니다."));
    FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &USteamDevelopmentSubsystem::HandleFound));
    if (Sessions->FindSessions(*LocalId, Search.ToSharedRef())) return true;
    HandleFound(false);
    return false;
}

bool USteamDevelopmentSubsystem::IsCompatible(const FOnlineSessionSearchResult& Result) const
{
    FString Marker;
    const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
    return Result.IsValid() && Result.Session.OwningUserId && Result.Session.OwningUserId->GetType() == FName(TEXT("STEAM")) && Settings.Get(ProbeKey, Marker) && Marker == ProbeValue && Settings.BuildUniqueId == GetBuildUniqueId() && !Settings.bIsLANMatch && Settings.bUseLobbiesIfAvailable && Settings.bUsesPresence && Settings.bAllowJoinViaPresenceFriendsOnly && Settings.NumPublicConnections >= 2 && Settings.NumPublicConnections <= 4 && Result.Session.NumOpenPublicConnections > 0;
}

void USteamDevelopmentSubsystem::HandleFound(bool bSuccess)
{
    if (Operation != EOperation::Finding) return;
    Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    FindHandle.Reset();
    Operation = EOperation::Idle;
    if (bSuccess && Search && IsReady())
    {
        for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
        {
            if (!IsCompatible(Result)) continue;
            Results.Add(Result);
            ResultLabels.Add(FString::Printf(TEXT("%s · %d/%d명"), *Result.Session.OwningUserName.Left(40), Result.Session.SessionSettings.NumPublicConnections - Result.Session.NumOpenPublicConnections, Result.Session.SessionSettings.NumPublicConnections));
        }
    }
    Search.Reset();
    SetStatus(bSuccess ? FString::Printf(TEXT("연결 확인 방 %d개를 찾았습니다. 친구 초대로도 참가할 수 있습니다."), Results.Num()) : TEXT("Steam 방 검색에 실패했습니다."));
    if (bLeaveRequested) Leave();
}

bool USteamDevelopmentSubsystem::Join(APlayerController* Controller, int32 ResultIndex)
{
    if (!Results.IsValidIndex(ResultIndex))
    {
        SetStatus(TEXT("검색 결과에서 방을 선택해 주세요."));
        return false;
    }
    return JoinResult(Controller, Results[ResultIndex]);
}

bool USteamDevelopmentSubsystem::JoinResult(APlayerController* Controller, const FOnlineSessionSearchResult& Result)
{
    if (!CanBegin(Controller)) return false;
    if (!IsCompatible(Result))
    {
        SetStatus(TEXT("이 초대 또는 검색 결과는 호환되는 ProjectA 480 연결 확인 방이 아닙니다."));
        return false;
    }
    SessionCapacity = Result.Session.SessionSettings.NumPublicConnections;
    AuthenticatedUsers.Reset();
    Operation = EOperation::Joining;
    bOwnsSession = true;
    SetStatus(TEXT("Steam 방에 참가하는 중입니다. 게임 Run에는 참가하지 않습니다."));
    JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &USteamDevelopmentSubsystem::HandleJoined));
    if (Sessions->JoinSession(*LocalId, NAME_GameSession, Result)) return true;
    HandleJoined(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
    return false;
}

void USteamDevelopmentSubsystem::HandleJoined(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
    if (Name != NAME_GameSession || Operation != EOperation::Joining) return;
    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    JoinHandle.Reset();
    Operation = EOperation::Idle;
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        SetStatus(FString::Printf(TEXT("Steam 방 참가 실패 (%d). 로컬 계정으로 대체하지 않습니다."), static_cast<int32>(Result)));
        return;
    }
    FString URL;
    if (bLeaveRequested || !IsReady() || !RequestController.IsValid() || !Sessions->GetResolvedConnectString(NAME_GameSession, URL) || URL.IsEmpty())
    {
        Leave();
        return;
    }
    SetStatus(TEXT("SteamSockets 연결 중 · 서버의 공식 SteamAuth 판정을 기다립니다."));
    RequestController->ClientTravel(URL, TRAVEL_Absolute);
}

void USteamDevelopmentSubsystem::HandleInvite(bool bSuccess, int32 LocalUserNum, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
    if (!bSuccess || LocalUserNum != 0 || !IsReady() || !UserId || *UserId != *LocalId)
    {
        SetStatus(TEXT("현재 로그인과 일치하는 유효한 Steam 초대가 아닙니다."));
        return;
    }
    JoinResult(GetGameInstance()->GetFirstLocalPlayerController(), Result);
}

bool USteamDevelopmentSubsystem::ShowInvites()
{
    if (!IsReady() || IsBusy() || !HasSession() || !GetWorld() || GetWorld()->GetNetMode() != NM_ListenServer) return false;
    const IOnlineExternalUIPtr ExternalUI = Steam->GetExternalUIInterface();
    const bool bOpened = ExternalUI && ExternalUI->ShowInviteUI(0, NAME_GameSession);
    if (!bOpened) SetStatus(TEXT("Steam 초대 창을 열지 못했습니다. Steam 오버레이 설정을 확인해 주세요."));
    return bOpened;
}

void USteamDevelopmentSubsystem::Leave()
{
    bLeaveRequested = true;
    if (IsBusy())
    {
        SetStatus(TEXT("진행 중인 Steam 요청이 끝나면 연결 확인 방을 정리합니다."));
        return;
    }
    AuthenticatedUsers.Reset();
    if (!HasSession())
    {
        bOwnsSession = false;
        if (GetWorld() && (GetWorld()->GetNetMode() != NM_Standalone || Cast<ASteamDevelopmentPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))) UGameplayStatics::OpenLevel(GetGameInstance(), MenuMap, true);
        return;
    }
    Operation = EOperation::Destroying;
    SetStatus(TEXT("Steam 연결 확인 방을 나가는 중입니다."));
    DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &USteamDevelopmentSubsystem::HandleDestroyed));
    if (!Sessions->DestroySession(NAME_GameSession)) HandleDestroyed(NAME_GameSession, false);
}

void USteamDevelopmentSubsystem::HandleDestroyed(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession || Operation != EOperation::Destroying) return;
    Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    DestroyHandle.Reset();
    Operation = EOperation::Idle;
    bLeaveRequested = false;
    if (!bSuccess)
    {
        SetStatus(TEXT("Steam 방 정리에 실패했습니다. 다시 나가기를 실행해 주세요."));
        return;
    }
    SessionCapacity = 0;
    bOwnsSession = false;
    SetStatus(TEXT("Steam 연결 확인 방을 정리했습니다. 기존 Run과 저장은 변경하지 않았습니다."));
    if (GetWorld() && (GetWorld()->GetNetMode() != NM_Standalone || Cast<ASteamDevelopmentPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))) UGameplayStatics::OpenLevel(GetGameInstance(), MenuMap, true);
}

void USteamDevelopmentSubsystem::HandleAuthentication(const FUniqueNetId& UserId, bool bSuccess)
{
    if (!CanAcceptProbeConnection() || UserId.GetType() != FName(TEXT("STEAM"))) return;
    if (bSuccess) AuthenticatedUsers.Add(UserId.ToString());
    else AuthenticatedUsers.Remove(UserId.ToString());
    SetStatus(bSuccess ? TEXT("서버가 원격 참가자의 공식 SteamAuth 성공을 확인했습니다.") : TEXT("원격 참가자의 SteamAuth가 실패했습니다. 엔진의 기본 추방 처리를 유지합니다."));
}

bool USteamDevelopmentSubsystem::HasAuthentication(const FUniqueNetId& UserId) const
{
    return CanAcceptProbeConnection() && UserId.GetType() == FName(TEXT("STEAM")) && AuthenticatedUsers.Contains(UserId.ToString());
}

bool USteamDevelopmentSubsystem::IsAllowedFriend(const FUniqueNetId& UserId) const
{
    if (!IsReady() || !UserId.IsValid() || UserId.GetType() != FName(TEXT("STEAM"))) return false;
    if (UserId == *LocalId) return true;
    const IOnlineFriendsPtr Friends = Steam->GetFriendsInterface();
    return Friends && Friends->IsFriend(0, UserId, EFriendsLists::ToString(EFriendsLists::Default));
}

bool USteamDevelopmentSubsystem::CanAcceptProbeConnection() const
{
    const UWorld* World = GetWorld();
    const URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    return IsReady() && HasSession() && World && World->GetNetMode() == NM_ListenServer && World->GetAuthGameMode<ASteamDevelopmentLobbyGameMode>() && World->GetNetDriver() && World->GetNetDriver()->GetClass()->GetPathName() == DriverPath && Run && Run->GetPhase() == ERunPhase::None && !Run->IsManagedRun() && !Run->HasManagedLease();
}

void USteamDevelopmentSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString&)
{
    if (!World || World->GetGameInstance() != GetGameInstance() || !HasSession()) return;
    SetStatus(TEXT("Steam 연결이 실패하거나 끊겼습니다. 나가기로 세션을 정리해 주세요. 자동 Host 승계나 Run 전환은 하지 않습니다."));
    AuthenticatedUsers.Reset();
}

void USteamDevelopmentSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type, const FString&)
{
    if (!World || World->GetGameInstance() != GetGameInstance() || !HasSession()) return;
    SetStatus(TEXT("Steam 연결 확인 화면으로 이동하지 못했습니다. 나가기로 세션을 정리해 주세요."));
    AuthenticatedUsers.Reset();
}
