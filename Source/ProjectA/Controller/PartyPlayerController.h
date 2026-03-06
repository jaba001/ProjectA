#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PartyPlayerController.generated.h"

class AUnitBase;
class ACombatManager;
class ACombatGridTile;

UCLASS()
class PROJECTA_API APartyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APartyPlayerController();

protected:
    virtual void BeginPlay() override;

public:
    UFUNCTION(BlueprintCallable)
    AUnitBase* GetActiveUnit() const;

    UFUNCTION(BlueprintCallable)
    void RequestEndTurn();

    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    ACombatGridTile* SelectedTile;

    UFUNCTION(BlueprintCallable)
    void SetSelectedTile(ACombatGridTile* InTile);

private:
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    UUserWidget* HUDWidget;

    UPROPERTY()
    ACombatManager* CombatManager;


};
