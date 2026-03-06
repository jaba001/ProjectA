#include "PlayerUnit.h"

APlayerUnit::APlayerUnit()
{
    InitMaxHP = 200.f;
}

void APlayerUnit::OnTurnStart()
{
    Super::OnTurnStart();

}

void APlayerUnit::OnTurnEnd()
{
    Super::OnTurnEnd();
}
