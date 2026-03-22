#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSpriteComponent.h"
#include "Components/BoxComponent.h"
#include "CombatGridTile.generated.h"

class AUnitBase;

UENUM(BlueprintType)
enum class ETileTerritory : uint8
{
    None,
    Player,
    Enemy
};

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

    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    AUnitBase* GetOccupyingUnit() const    {return OccupyingUnit;}

    void UpdateTileVisual();

    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
	void ApplyMovableTileVisual();

    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    void ApplySkillTargetTileVisual();

    void ClearHighlightVisual();

public:
    void SetProtectedByFront(bool bInProtectedByFront);

    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    bool GetProtectedByFront() const { return bProtectedByFront; }

private:
    FLinearColor OriginalColor;

    UPROPERTY(EditAnywhere, Category = "CombatGridTile")
    FLinearColor ProtectedByFrontColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);

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

    UPROPERTY(EditAnywhere, Category = "TileSprite")
    UPaperSprite* MovableSprite;

    UPROPERTY(EditAnywhere, Category = "TileSprite")
    UPaperSprite* ActiveSprite;
public:
    void SetTerritory(ETileTerritory NewTerritory) { Territory = NewTerritory; }

    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    ETileTerritory GetTerritory() const { return Territory; }

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatGridTile")
    FIntPoint GridCoord;

private:
    UPROPERTY()
    AUnitBase* OccupyingUnit;

    UPROPERTY(EditAnywhere, Category = "CombatGridTile")
    ETileTerritory Territory = ETileTerritory::None;

    UPROPERTY()
    bool bProtectedByFront = false;

private:
    // 현재 이 타일이 이동 가능 타일 하이라이트 상태인지 여부
    UPROPERTY()
    bool bMovableHighlighted = false;

    // 현재 이 타일이 스킬 타겟 하이라이트 상태인지 여부
    UPROPERTY()
    bool bSkillTargetHighlighted = false;

};