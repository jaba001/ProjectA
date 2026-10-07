#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SteamDevelopmentSubsystem.generated.h"

class APlayerController;
class IOnlineSubsystem;
class FOnlineAuthUtilsSteam;

// This transport probe never creates, loads, or grants authority over a Run.
// 이 전송 검증 도구는 Run을 생성하거나 읽거나 권한을 부여하지 않습니다.
UCLASS()
class PROJECTA_API USteamDevelopmentSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static bool IsRequested();
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool Host(APlayerController* Controller, int32 Capacity);
    bool Find(APlayerController* Controller);
    bool Join(APlayerController* Controller, int32 ResultIndex);
    bool ShowInvites();
    void Leave();
    bool IsBusy() const { return Operation != EOperation::Idle; }
    bool HasSession() const;
    bool IsReady() const;
    bool HasAuthentication(const FUniqueNetId& UserId) const;
    void ForgetAuthentication(const FUniqueNetId& UserId);
    bool IsAllowedFriend(const FUniqueNetId& UserId) const;
    bool CanAcceptProbeConnection() const;
    const FText& GetStatus() const { return Status; }
    const TArray<FString>& GetResultLabels() const { return ResultLabels; }
    int32 GetCapacity() const { return SessionCapacity; }

private:
    enum class EOperation : uint8 { Idle, Creating, ReadingFriends, Finding, Joining, Destroying };
    bool CanBegin(APlayerController* Controller);
    bool JoinResult(APlayerController* Controller, const FOnlineSessionSearchResult& Result);
    bool IsCompatible(const FOnlineSessionSearchResult& Result) const;
    void SetStatus(const FString& Message);
    void HandleCreated(FName Name, bool bSuccess);
    void HandleFriendsRead(int32 LocalUserNum, bool bSuccess, const FString& ListName, const FString& Error, uint64 RequestId);
    void FindNextFriendSession();
    void HandleFriendSessionFound(int32 LocalUserNum, bool bSuccess, const TArray<FOnlineSessionSearchResult>& FriendResults, uint64 RequestId);
    void FinishFinding(bool bSuccess);
    void HandleJoined(FName Name, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroyed(FName Name, bool bSuccess);
    void HandleInvite(bool bSuccess, int32 LocalUserNum, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result);
    void HandleAuthentication(const FUniqueNetId& UserId, bool bSuccess);
    void HandleNetworkFailure(UWorld* World, class UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Message);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Message);
    void ClearSessionDelegates();

    IOnlineSubsystem* Steam = nullptr;
    IOnlineSessionPtr Sessions;
    FUniqueNetIdPtr LocalId;
    TArray<FUniqueNetIdRef> SearchFriends;
    TArray<FOnlineSessionSearchResult> Results;
    TArray<FString> ResultLabels;
    TSet<FString> AuthenticatedUsers;
    TWeakObjectPtr<APlayerController> RequestController;
    FDelegateHandle CreateHandle;
    FDelegateHandle FindHandle;
    FDelegateHandle JoinHandle;
    FDelegateHandle DestroyHandle;
    FDelegateHandle InviteHandle;
    FDelegateHandle NetworkHandle;
    FDelegateHandle TravelHandle;
    EOperation Operation = EOperation::Idle;
    int32 SessionCapacity = 0;
    int32 NextSearchFriend = 0;
    uint64 FriendSearchRequestId = 0;
    bool bFriendRequestPending = false;
    bool bLeaveRequested = false;
    bool bOwnsAuthenticationDelegate = false;
    bool bOwnsSession = false;
    FText Status;
};
