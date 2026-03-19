#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PartyPlayerController.generated.h"

class AUnitBase;
class ACombatManager;
class ACombatGridTile;
class UUserWidget;
class UGameplayAbility;

UENUM(BlueprintType)
enum class ETileInputMode : uint8
{
    None,
    Skill,
    Move,
    Item
};

UCLASS()
class PROJECTA_API APartyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APartyPlayerController();

protected:
    virtual void BeginPlay() override;

private:
    void InitializeCombatManager();
    void InitializeHUD();

public:
    UFUNCTION(BlueprintCallable, Category = "Combat")
    AUnitBase* GetActiveUnit() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RequestEndTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatManager* GetCombatManager() const { return CombatManager; }

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanUseActiveUnitAction() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanUseActiveUnitActionPoint(int32 Cost) const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanUseActiveUnitSubActionPoint(int32 Cost) const;

public:
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetSelectedTile(ACombatGridTile* InTile);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ACombatGridTile* GetSelectedTile() const;

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void ClearSelectedTile();

public:
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetTileInputMode(ETileInputMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void EnterMoveMode();

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void EnterSkillMode(TSubclassOf<UGameplayAbility> AbilityClass, bool bMoveToTarget, int32 ActionPointCost);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void CancelTileInputMode();

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ETileInputMode GetTileInputMode() const { return CurrentTileInputMode; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsMoveInputMode() const { return CurrentTileInputMode == ETileInputMode::Move; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsSkillInputMode() const { return CurrentTileInputMode == ETileInputMode::Skill; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    TSubclassOf<UGameplayAbility> GetPendingSkillInputAbilityClass() const { return PendingSkillInputAbilityClass; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool GetPendingSkillInputMoveToTarget() const { return bPendingSkillInputMoveToTarget; }

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (AllowPrivateAccess = "true"))
    ACombatGridTile* SelectedTile = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (AllowPrivateAccess = "true"))
    ETileInputMode CurrentTileInputMode = ETileInputMode::None;

    UPROPERTY()
    TSubclassOf<UGameplayAbility> PendingSkillInputAbilityClass = nullptr;

    UPROPERTY()
    bool bPendingSkillInputMoveToTarget = true;


private:
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    UUserWidget* HUDWidget = nullptr;

    UPROPERTY()
    ACombatManager* CombatManager = nullptr;
};