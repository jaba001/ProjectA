#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSpriteComponent.h"
#include "Components/BoxComponent.h"
#include "CombatGridTile.generated.h"

class AUnitBase;

UCLASS()
class PROJECTA_API ACombatGridTile : public AActor
{
    GENERATED_BODY()

public:
    ACombatGridTile();

protected:
    virtual void BeginPlay() override;

public:
    //virtual void Tick(float DeltaTime) override;
    virtual void NotifyActorOnClicked(FKey ButtonPressed) override;
    virtual void NotifyActorBeginCursorOver() override;
    virtual void NotifyActorEndCursorOver() override;

public:
    void SetOccupyingUnit(AUnitBase* NewUnit);

    AUnitBase* GetOccupyingUnit() const    {return OccupyingUnit;}

    void UpdateTileVisual();

private:
    FLinearColor OriginalColor;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile", meta = (AllowPrivateAccess = "true"))
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile", meta = (AllowPrivateAccess = "true"))
    UPaperSpriteComponent* TileSprite;

    UPROPERTY(EditAnywhere, Category = "TileSprite")
    UPaperSprite* EmptySprite;

    UPROPERTY(EditAnywhere, Category = "TileSprite")
    UPaperSprite* PlayerSprite;

    UPROPERTY(EditAnywhere, Category = "TileSprite")
    UPaperSprite* EnemySprite;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile")
    FIntPoint GridCoord;

private:
    UPROPERTY()
    AUnitBase* OccupyingUnit;
};