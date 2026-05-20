#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDWidget.generated.h"

class ACombatManager;

UCLASS()
class PROJECTA_API UBattleHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "BattleHUD")
    FText GetTurnInfoText() const;

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY()
    ACombatManager* CachedCombatManager = nullptr;
};