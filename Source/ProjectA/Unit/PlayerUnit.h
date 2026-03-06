// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Unit/UnitBase.h"
#include "PlayerUnit.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTA_API APlayerUnit : public AUnitBase
{
	GENERATED_BODY()

public:
	APlayerUnit();

public:
	virtual void OnTurnStart() override;
	virtual void OnTurnEnd() override;

};
