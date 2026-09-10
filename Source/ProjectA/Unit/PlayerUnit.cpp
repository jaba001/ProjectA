#include "PlayerUnit.h"
#include "Combat/CombatManager.h"
#include "Combat/AI/PartyAutoCombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

APlayerUnit::APlayerUnit()
{
    InitMaxHP = 200.f;
    AutoCombat = CreateDefaultSubobject<UPartyAutoCombatComponent>(TEXT("AutoCombat"));
}

void APlayerUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APlayerUnit, PartyControlMode);
}

void APlayerUnit::InitializeAutoCombat(ACombatManager* CombatManager)
{
    if (HasAuthority() && AutoCombat)
    {
        AutoCombat->InitializeCombat(CombatManager);
    }
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
    if (AutoCombat)
    {
        AutoCombat->Stop();
    }
    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    PartyControlMode = Mode;
    AIControlSessionId = Mode == EPartyControlMode::ServerAI ? FGuid::NewGuid() : FGuid();
    ForceNetUpdate();
    OnRep_PartyControlMode();
    return true;
}

void APlayerUnit::OnRep_PartyControlMode()
{
    // Reuse unit presentation refresh; replication never starts local AI or changes combat state.
    // 유닛 화면 갱신을 재사용하며 복제로 로컬 AI를 시작하거나 전투 상태를 바꾸지 않습니다.
    OnRep_Team();
}

void APlayerUnit::OnTurnStart()
{
    if (!HasAuthority())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    Super::OnTurnStart();
    if (IsServerAIControlled() && AutoCombat)
    {
        AutoCombat->StartTurn();
    }
}

void APlayerUnit::OnTurnEnd()
{
    if (!HasAuthority())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    if (AutoCombat)
    {
        AutoCombat->Stop();
    }
    Super::OnTurnEnd();
}

void APlayerUnit::OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
    Super::OnUnitActionCompleted(ActionType, Result);
    if (IsServerAIControlled())
    {
        if (HasAuthority() && AutoCombat)
        {
            AutoCombat->HandleActionCompleted(ActionType, Result);
        }
        return;
    }
    if (!HasAuthority() || !IsActiveTurn() || !IsUnitAlive() || GetCurrentActionPoint() > 0 || GetCurrentSubActionPoint() > 0)
    {
        return;
    }
    const uint32 CompletedSerial = CurrentActionSerial;
    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    ExhaustedTurnTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, CompletedSerial]()
    {
        if (IsServerAIControlled() || CurrentActionSerial != CompletedSerial || IsBusy() || GetCurrentActionPoint() > 0 || GetCurrentSubActionPoint() > 0)
        {
            return;
        }
        ACombatManager* Combat = Cast<ACombatManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ACombatManager::StaticClass()));
        if (Combat)
        {
            Combat->RequestEndTurnForUnit(this);
        }
    }));
}

void APlayerUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    if (AutoCombat)
    {
        AutoCombat->Stop();
    }
    Super::EndPlay(EndPlayReason);
}
