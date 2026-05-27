#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class ACombatManager;

UCLASS()
class PROJECTA_API UCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "BattleHUD")
    FText GetTurnInfoText() const;

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY()
    ACombatManager* CachedCombatManager = nullptr;
};
