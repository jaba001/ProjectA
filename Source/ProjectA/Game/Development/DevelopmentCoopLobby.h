#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "DevelopmentCoopLobby.generated.h"

class AGameplayPlayerController;

USTRUCT()
struct FDevelopmentCoopMember
{
    GENERATED_BODY()
    UPROPERTY()
    int32 PlayerId = INDEX_NONE;
    UPROPERTY()
    bool bAssigned = false;
    UPROPERTY()
    bool bConnected = false;
    UPROPERTY()
    bool bReady = false;
};

// Replicate presentation data; server connection references stay outside the serialized roster.
// 표시 데이터만 복제하며 서버 연결 참조는 직렬화된 참가 목록 밖에 둡니다.
UCLASS(NotBlueprintable)
class PROJECTA_API ADevelopmentCoopLobby : public AInfo
{
    GENERATED_BODY()

public:
    ADevelopmentCoopLobby();
    void InitializeLobby(int32 Capacity);
    bool AddParticipant(AGameplayPlayerController* Controller);
    void RemoveParticipant(AGameplayPlayerController* Controller);
    bool CanAdmit() const;
    bool CanStart() const;
    void SetReady(AGameplayPlayerController* Controller, bool bReady);
    bool Start(AGameplayPlayerController* Controller);
    const TArray<FDevelopmentCoopMember>& GetMembers() const { return Members; }
    const FText& GetMessage() const { return Message; }
    bool HasStarted() const { return bStarted; }
    bool IsClosed() const { return bClosed; }

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void Publish();
    UFUNCTION()
    void OnRep_Lobby();
    int32 FindParticipant(const AGameplayPlayerController* Controller) const;
    TArray<TWeakObjectPtr<AGameplayPlayerController>> Connections;
    int32 NextOrdinal = 2;
    UPROPERTY(ReplicatedUsing = OnRep_Lobby)
    bool bClosed = false;
    UPROPERTY(ReplicatedUsing = OnRep_Lobby)
    TArray<FDevelopmentCoopMember> Members;
    UPROPERTY(ReplicatedUsing = OnRep_Lobby)
    bool bStarted = false;
    UPROPERTY(ReplicatedUsing = OnRep_Lobby)
    FText Message;
};
