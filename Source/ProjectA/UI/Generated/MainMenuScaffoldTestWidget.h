// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuScaffoldTestWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class PROJECTA_API UMainMenuScaffoldTestWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Start;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Start;
};
