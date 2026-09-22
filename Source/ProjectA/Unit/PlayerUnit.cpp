#include "Unit/PlayerUnit.h"
#include "Net/UnrealNetwork.h"

APlayerUnit::APlayerUnit()
{
    InitMaxHP = 200.0f;
}

void APlayerUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APlayerUnit, PartyControlMode);
}

void APlayerUnit::InitializeAutoCombat(ACombatManager* CombatManager)
{
    // Compatibility entry point; round AI is owned by the combat coordinator.
    // 호환 진입점이며 라운드 AI는 전투 조정자가 소유합니다.
}

bool APlayerUnit::ApplyPartyControlMode(EPartyControlMode Mode)
{
    if (!HasAuthority() || IsActiveTurn() || IsBusy() || (Mode != EPartyControlMode::Human && Mode != EPartyControlMode::ServerAI) || (Mode == EPartyControlMode::ServerAI && GetTeam() != ETeam::Player))
    {
        return false;
    }
    if (PartyControlMode == Mode)
    {
        return true;
    }
    PartyControlMode = Mode;
    AIControlSessionId = Mode == EPartyControlMode::ServerAI ? FGuid::NewGuid() : FGuid();
    ForceNetUpdate();
    OnRep_PartyControlMode();
    return true;
}

void APlayerUnit::OnRep_PartyControlMode()
{
    // Ownership presentation never activates local AI or mutates combat state.
    // 소유권 표시는 로컬 AI를 활성화하거나 전투 상태를 변경하지 않습니다.
    OnRep_Team();
}

void APlayerUnit::OnTurnStart()
{
    // Reject activation through the removed sequential entry point.
    // 제거된 순차 진입점을 통한 활성화를 차단합니다.
    Super::OnTurnEnd();
}

void APlayerUnit::OnTurnEnd()
{
    Super::OnTurnEnd();
}
