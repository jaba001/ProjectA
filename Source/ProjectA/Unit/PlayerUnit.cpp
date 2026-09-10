#include "PlayerUnit.h"
#include "Combat/CombatManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APlayerUnit::APlayerUnit()
{
    InitMaxHP = 200.f;
}

void APlayerUnit::OnTurnStart()
{
    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    Super::OnTurnStart();

}

void APlayerUnit::OnTurnEnd()
{
    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    Super::OnTurnEnd();
}

void APlayerUnit::OnUnitActionCompleted(EUnitActionType ActionType, EUnitActionResult Result)
{
    Super::OnUnitActionCompleted(ActionType, Result);
    if (!HasAuthority() || !IsActiveTurn() || !IsUnitAlive() || GetCurrentActionPoint() > 0 || GetCurrentSubActionPoint() > 0)
    {
        return;
    }
    const uint32 CompletedSerial = CurrentActionSerial;
    GetWorldTimerManager().ClearTimer(ExhaustedTurnTimer);
    ExhaustedTurnTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, CompletedSerial]()
    {
        if (CurrentActionSerial != CompletedSerial || IsBusy() || GetCurrentActionPoint() > 0 || GetCurrentSubActionPoint() > 0)
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
    Super::EndPlay(EndPlayReason);
}
