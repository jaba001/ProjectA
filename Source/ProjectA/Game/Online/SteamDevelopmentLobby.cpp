#include "Game/Online/SteamDevelopmentLobby.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Online/SteamDevelopmentSubsystem.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "UI/MainMenu/DevelopmentCoopWidget.h"

void ASteamDevelopmentPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    bShowMouseCursor = true;
    SetInputMode(FInputModeUIOnly());
    Panel = CreateWidget<UDevelopmentCoopWidget>(this);
    if (Panel)
    {
        Panel->AddToViewport();
        Panel->ActivateWidget();
    }
}

void ASteamDevelopmentPlayerController::ClientSetProbeStatus_Implementation(const FText& Message)
{
    ProbeStatus = Message;
}

ASteamDevelopmentLobbyGameMode::ASteamDevelopmentLobbyGameMode()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = ASteamDevelopmentPlayerController::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
}

void ASteamDevelopmentLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    const USteamDevelopmentSubsystem* Probe = GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>();
    if (!Probe || !Probe->IsReady() || !Probe->HasSession()) ErrorMessage = TEXT("This connection-only lobby requires an explicitly configured Steam 480 session.");
    if (GameSession && Probe) GameSession->MaxPlayers = Probe->GetCapacity();
}

void ASteamDevelopmentLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    const USteamDevelopmentSubsystem* Probe = GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>();
    if (!Probe || !Probe->CanAcceptProbeConnection() || !UniqueId.IsValid() || UniqueId.GetType() != FName(TEXT("STEAM")) || !Probe->IsAllowedFriend(*UniqueId.GetUniqueNetId())) ErrorMessage = TEXT("The isolated development lobby requires a Steam friend and SteamAuth authentication.");
}

void ASteamDevelopmentLobbyGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    USteamDevelopmentSubsystem* Probe = GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>();
    if (!Probe) return;
    int32 Connected = 0;
    int32 Authenticated = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* Controller = It->Get();
        if (!Controller) continue;
        ++Connected;
        if (!Controller->IsLocalController() && Controller->PlayerState && Controller->PlayerState->GetUniqueId().IsValid() && Probe->HasAuthentication(*Controller->PlayerState->GetUniqueId().GetUniqueNetId())) ++Authenticated;
    }
    const FText Message = FText::FromString(FString::Printf(TEXT("서버 연결 %d/%d명 · 원격 SteamAuth 성공 %d명\n연결 확인 전용이며 캐릭터·Run·MMR·관리 저장은 생성하지 않습니다."), Connected, Probe->GetCapacity(), Authenticated));
    if (Message.EqualTo(LastStatus)) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ASteamDevelopmentPlayerController* Controller = Cast<ASteamDevelopmentPlayerController>(It->Get())) Controller->ClientSetProbeStatus(Message);
    }
    LastStatus = Message;
}

void ASteamDevelopmentLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    // New connections receive the current verdict on the next tick even when totals are unchanged.
    // 새 연결은 집계가 같아도 다음 tick에서 현재 판정을 받습니다.
    LastStatus = FText::GetEmpty();
}

void ASteamDevelopmentLobbyGameMode::Logout(AController* Exiting)
{
    // A later connection must receive a fresh verdict instead of reusing the previous observation.
    // 같은 계정의 다음 연결은 이전 관측값 대신 새로운 인증 판정을 받아야 합니다.
    USteamDevelopmentSubsystem* Probe = GetGameInstance()->GetSubsystem<USteamDevelopmentSubsystem>();
    if (Probe && Exiting && Exiting->PlayerState && Exiting->PlayerState->GetUniqueId().IsValid()) Probe->ForgetAuthentication(*Exiting->PlayerState->GetUniqueId().GetUniqueNetId());
    Super::Logout(Exiting);
    LastStatus = FText::GetEmpty();
}
