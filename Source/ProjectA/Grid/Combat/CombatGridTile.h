#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSpriteComponent.h"
#include "Components/BoxComponent.h"
#include "CombatGridTile.generated.h"

UCLASS()
class PROJECTA_API ACombatGridTile : public AActor
{
    GENERATED_BODY()

public:
    ACombatGridTile();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    virtual void NotifyActorOnClicked(FKey ButtonPressed) override;
    virtual void NotifyActorBeginCursorOver() override;
    virtual void NotifyActorEndCursorOver() override;

private:
    FLinearColor OriginalColor;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile")
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile")
    UPaperSpriteComponent* TileSprite;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile")
    FIntPoint GridCoord;
};