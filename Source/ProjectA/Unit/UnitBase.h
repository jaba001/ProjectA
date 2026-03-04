#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attribute/AS_Unit.h"

#include "UnitBase.generated.h"

class ACombatGridTile;

UCLASS()
class PROJECTA_API AUnitBase : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

    public:
    AUnitBase();

    
public: // === GAS Interface ===
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem;

    UPROPERTY()
    UAS_Unit* AttributeSet;

public:
    virtual void BeginPlay() override;

    virtual void Tick(float DeltaTime) override;

public: // === Unit Identity / State ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
    int32 UnitIndex;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
    ACombatGridTile* CurrentTile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn")
    bool bIsActiveTurn;
    
public: // === Unit Actions ===
    UFUNCTION(BlueprintCallable, Category = "Unit")
    void SetCurrentTile(ACombatGridTile* NewTile);

    UFUNCTION(BlueprintCallable, Category = "Unit")
    void MoveToTile(ACombatGridTile* TargetTile);

    UFUNCTION(BlueprintCallable, Category = "Turn")
    virtual void OnTurnStart();

    UFUNCTION(BlueprintCallable, Category = "Turn")
    virtual void OnTurnEnd();
};
