#include "EnemyUnit.h"
#include "Controller/PartyPlayerController.h"
#include "EngineUtils.h"

AEnemyUnit::AEnemyUnit()
{
    InitMaxHP = 150.f;
}

void AEnemyUnit::OnTurnStart()
{
    Super::OnTurnStart();
    ExecuteEnemyTurn();
}

void AEnemyUnit::ExecuteEnemyTurn()
{
    AUnitBase* Target = FindPlayerTarget();

    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] No target found"));
        return;
    }

    APartyPlayerController* PC = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());
    
    //UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s attacks %s"), *GetName(), *Target->GetName());

    //DealDamage(Target, 20.f);

    if (PC)
    {
        PC->RequestEndTurn();
    }
}

AUnitBase* AEnemyUnit::FindPlayerTarget()
{
    for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
    {
        if (It->GetTeam() == ETeam::Player && It->IsUnitAlive())
        {
            return *It;
        }
    }

    return nullptr;
}