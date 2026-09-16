#include "Game/Development/DevelopmentCoopLobby.h"

#include "Controller/GameplayPlayerController.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/GameState/GameplayGameState.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ADevelopmentCoopLobby::ADevelopmentCoopLobby()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(10.0f);
}

void ADevelopmentCoopLobby::Publish()
{
    ForceNetUpdate();
    OnRep_Lobby();
}

void ADevelopmentCoopLobby::OnRep_Lobby()
{
    if (AGameplayGameState* State = GetWorld()->GetGameState<AGameplayGameState>()) State->OnGameplayViewChanged.Broadcast();
}

void ADevelopmentCoopLobby::InitializeLobby(int32 Capacity)
{
    if (!HasAuthority() || !UDevelopmentCoopSubsystem::IsAvailable() || Capacity < 2 || Capacity > 4 || !Members.IsEmpty()) return;
    Members.SetNum(Capacity);
    Connections.SetNum(Capacity);
    Message = FText::FromString(TEXT("모든 참가자가 준비하면 Host가 시작합니다. 캐릭터는 각자 궁수 1명입니다."));
    Publish();
}

int32 ADevelopmentCoopLobby::FindParticipant(const AGameplayPlayerController* Controller) const
{
    if (!Controller) return INDEX_NONE;
    return Connections.IndexOfByPredicate([Controller](const TWeakObjectPtr<AGameplayPlayerController>& Entry) { return Entry.Get() == Controller; });
}

bool ADevelopmentCoopLobby::CanAdmit() const
{
    return UDevelopmentCoopSubsystem::IsAvailable() && !bStarted && !bClosed && NextOrdinal <= Members.Num();
}

bool ADevelopmentCoopLobby::AddParticipant(AGameplayPlayerController* Controller)
{
    if (!HasAuthority() || !UDevelopmentCoopSubsystem::IsAvailable() || bStarted || bClosed || !Controller || Controller->GetWorld() != GetWorld() || !Controller->PlayerState) return false;
    if (FindParticipant(Controller) != INDEX_NONE) return true;
    const int32 Index = Controller->IsLocalController() ? 0 : NextOrdinal - 1;
    if (!Members.IsValidIndex(Index) || Members[Index].bAssigned) return false;
    if (!Controller->IsLocalController()) ++NextOrdinal;
    Connections[Index] = Controller;
    Members[Index].PlayerId = Controller->PlayerState->GetPlayerId();
    Members[Index].bAssigned = Members[Index].bConnected = true;
    Publish();
    return true;
}

void ADevelopmentCoopLobby::RemoveParticipant(AGameplayPlayerController* Controller)
{
    if (!HasAuthority()) return;
    const int32 Index = FindParticipant(Controller);
    if (Index == INDEX_NONE) return;
    Connections[Index].Reset();
    Members[Index].bConnected = Members[Index].bReady = false;
    bClosed = true;
    Message = FText::FromString(TEXT("참가자가 나갔습니다. 자동 Host 승계·AI 전환·대체 참가는 하지 않습니다. 메뉴로 나가 새 방을 만들어 주세요."));
    Publish();
}

bool ADevelopmentCoopLobby::CanStart() const
{
    return !bStarted && !bClosed && Members.Num() >= 2 && Members.Num() <= 4 && !Members.ContainsByPredicate([](const FDevelopmentCoopMember& Member) { return !Member.bConnected || !Member.bReady; });
}

void ADevelopmentCoopLobby::SetReady(AGameplayPlayerController* Controller, bool bReady)
{
    if (!HasAuthority() || !UDevelopmentCoopSubsystem::IsAvailable() || bStarted || bClosed) return;
    const int32 Index = FindParticipant(Controller);
    if (Index == INDEX_NONE || !Members[Index].bConnected) return;
    Members[Index].bReady = bReady;
    Publish();
}

bool ADevelopmentCoopLobby::Start(AGameplayPlayerController* Controller)
{
    if (!HasAuthority() || !UDevelopmentCoopSubsystem::IsAvailable() || !Controller || !Controller->IsLocalController() || FindParticipant(Controller) != 0 || !CanStart() || GetNetMode() != NM_ListenServer) return false;
    AGameplayGameModeBase* Mode = GetWorld()->GetAuthGameMode<AGameplayGameModeBase>();
    URunStateSubsystem* Run = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    FProfessionDefinition Profession;
    if (!Mode || !Mode->GetEncounterManager() || !Mode->PartyDefinition || !Mode->PartyDefinition->ResolveProfession(TEXT("Archer"), Profession) || !Run || Run->IsManagedRun())
    {
        Message = FText::FromString(TEXT("Gameplay와 궁수 파티 설정을 확인해 주세요."));
        Publish();
        return false;
    }
    for (const TWeakObjectPtr<AGameplayPlayerController>& Connection : Connections) if (!Connection.IsValid()) return false;
    FRunIdentityData Identity;
    Identity.SchemaVersion = URunIdentityLibrary::CurrentSchemaVersion;
    Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
    Identity.RunId = FGuid::NewGuid();
    Identity.HostEpoch = 1;
    TArray<FRunPartyMember> Party;
    for (int32 Index = 0; Index < Members.Num(); ++Index)
    {
        FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
        Participant.JoinOrdinal = Index + 1;
        Participant.AccountId.Provider = TEXT("Development");
        Participant.AccountId.Subject = Identity.RunId.ToString(EGuidFormats::Digits) + FString::Printf(TEXT("_%d"), Index + 1);
        FRunPartyMember& Member = Party.AddDefaulted_GetRef();
        Member.SlotIndex = Index;
        Member.bCreated = true;
        Member.CharacterId = FGuid::NewGuid();
        Member.OwnerAccountId = Participant.AccountId;
        Member.ClassId = TEXT("Archer");
        Member.CharacterName = FText::FromString(FString::Printf(TEXT("Player %d"), Index + 1));
    }
    Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
    Run->PartyDefinition = Mode->PartyDefinition;
    // Separate scratch checkpoints from ordinary and managed saves; this room has no resume UI.
    // 일반·관리 저장과 개발용 체크포인트를 분리하며 이 방은 이어하기 UI를 제공하지 않습니다.
    Run->EnableCheckpointSaving(TEXT("ProjectA_DevCoop_") + Identity.RunId.ToString(EGuidFormats::Digits));
    if (!Run->InitializeRunWithIdentity(Party, Identity, Message))
    {
        Publish();
        return false;
    }
    for (int32 Index = 0; Index < Members.Num(); ++Index)
    {
        if (!Mode->AssignRunParticipant(Connections[Index].Get(), Identity.OriginalParticipants[Index].AccountId))
        {
            bClosed = true;
            Message = FText::FromString(TEXT("참가자 배정에 실패했습니다. 메뉴로 나가 새 방을 만들어 주세요."));
            Publish();
            return false;
        }
    }
    bStarted = true;
    Message = FText::FromString(TEXT("개발용 협동 · 본인 캐릭터만 조작 · 진행 선택은 Host"));
    Publish();
    return true;
}

void ADevelopmentCoopLobby::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADevelopmentCoopLobby, Members);
    DOREPLIFETIME(ADevelopmentCoopLobby, bStarted);
    DOREPLIFETIME(ADevelopmentCoopLobby, Message);
    DOREPLIFETIME(ADevelopmentCoopLobby, bClosed);
}
