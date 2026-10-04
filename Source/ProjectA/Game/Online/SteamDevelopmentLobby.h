#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "SteamDevelopmentLobby.generated.h"

class UDevelopmentCoopWidget;

UCLASS()
class PROJECTA_API ASteamDevelopmentPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    UFUNCTION(Client, Reliable)
    void ClientSetProbeStatus(const FText& Message);
    const FText& GetProbeStatus() const { return ProbeStatus; }

private:
    UPROPERTY(Transient)
    TObjectPtr<UDevelopmentCoopWidget> Panel;
    FText ProbeStatus;
};

// Reuse the menu World solely for connection checks; no combat or Run authority is present.
// 메뉴 World를 연결 확인에만 재사용하며 전투와 Run 권한은 두지 않습니다.
UCLASS()
class PROJECTA_API ASteamDevelopmentLobbyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASteamDevelopmentLobbyGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    FText LastStatus;
};
