// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "EnemyUnit.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTA_API AEnemyUnit : public AUnitBase
{
	GENERATED_BODY()

public:
	AEnemyUnit();

public:
	virtual void OnTurnStart() override;

	void ExecuteEnemyTurn();
	AUnitBase* FindPlayerTarget();
};
