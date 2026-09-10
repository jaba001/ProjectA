#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSpriteComponent.h"
#include "Components/BoxComponent.h"
#include "CombatGridTile.generated.h"

class AUnitBase;
class ACombatGridManager;

// Territory owner assigned to a combat grid tile.
// 전투 그리드 타일에 지정되는 영역 소유자입니다.
UENUM(BlueprintType)
enum class ETileTerritory : uint8
{
    None,
    Player,
    Enemy
};

// Actor representing one selectable tile on the combat grid.
// 전투 그리드에서 선택 가능한 단일 타일을 나타내는 액터입니다.
UCLASS()
class PROJECTA_API ACombatGridTile : public AActor
{
    GENERATED_BODY()

public:
    // Sets tile defaults and component references.
    // 타일 기본값과 컴포넌트 참조를 설정합니다.
    ACombatGridTile();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // Caches the original visual state after startup.
    // 시작 후 원래 시각 상태를 캐시합니다.
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    //virtual void Tick(float DeltaTime) override;
    // Handles mouse click selection for this tile.
    // 이 타일의 마우스 클릭 선택을 처리합니다.
    virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

    // Applies hover visual feedback.
    // 마우스 오버 시각 피드백을 적용합니다.
    virtual void NotifyActorBeginCursorOver() override;

    // Clears hover visual feedback.
    // 마우스 오버 시각 피드백을 해제합니다.
    virtual void NotifyActorEndCursorOver() override;

public:
    // Assigns the unit currently occupying this tile.
    // 현재 이 타일을 점유한 유닛을 지정합니다.
    void SetOccupyingUnit(AUnitBase* NewUnit);

    // Returns the unit currently occupying this tile.
    // 현재 이 타일을 점유한 유닛을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    AUnitBase* GetOccupyingUnit() const    {return OccupyingUnit;}

    // Updates the tile sprite and color from current state.
    // 현재 상태에 따라 타일 스프라이트와 색상을 갱신합니다.
    void UpdateTileVisual();

    // Applies the reachable movement visual.
    // 이동 가능 타일 시각 효과를 적용합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
	void ApplyMovableTileVisual();

    // Applies the skill target visual.
    // 스킬 대상 타일 시각 효과를 적용합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    void ApplySkillTargetTileVisual();

    // Clears movement and skill target highlight visuals.
    // 이동 및 스킬 대상 하이라이트 시각 효과를 해제합니다.
    void ClearHighlightVisual();

public:
    // Marks whether this tile is protected by a front-line unit.
    // 이 타일이 전열 유닛에 의해 보호되는지 표시합니다.
    void SetProtectedByFront(bool bInProtectedByFront);

    // Returns whether this tile is protected by a front-line unit.
    // 이 타일이 전열 유닛에 의해 보호되는지 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    bool GetProtectedByFront() const { return bProtectedByFront; }

private:
    FLinearColor OriginalColor = FLinearColor::White;
    bool bOriginalColorCached = false;

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
    // Sets tile territory ownership.
    // 타일의 영역 소유자를 설정합니다.
    void SetTerritory(ETileTerritory NewTerritory);

    // Assign the authoritative grid identity before indexing a generated tile.
    // 생성된 타일을 색인하기 전에 서버 그리드 식별 정보를 지정합니다.
    void InitializeGridTile(ACombatGridManager* Manager, const FIntPoint& Coord, ETileTerritory NewTerritory);
    ACombatGridManager* GetGridManager() const { return GridManager; }

    // Returns tile territory ownership.
    // 타일의 영역 소유자를 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "CombatGridTile")
    ETileTerritory GetTerritory() const { return Territory; }

public:
    // Grid coordinate of this tile.
    // 이 타일의 그리드 좌표입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_GridIndex, Category = "CombatGridTile")
    FIntPoint GridCoord = FIntPoint::ZeroValue;

private:
    UPROPERTY(ReplicatedUsing = OnRep_TileState)
    AUnitBase* OccupyingUnit;

    UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_TileState, Category = "CombatGridTile")
    ETileTerritory Territory = ETileTerritory::None;

    UPROPERTY(ReplicatedUsing = OnRep_TileState)
    bool bProtectedByFront = false;

    UPROPERTY(ReplicatedUsing = OnRep_GridIndex)
    TObjectPtr<ACombatGridManager> GridManager = nullptr;

    TWeakObjectPtr<ACombatGridManager> RegisteredGridManager;

    UFUNCTION()
    void OnRep_GridIndex();

    UFUNCTION()
    void OnRep_TileState();

private:
    // Whether this tile is currently highlighted as a reachable movement tile
    UPROPERTY()
    bool bMovableHighlighted = false;

    // Whether this tile is currently highlighted as a skill target tile
    UPROPERTY()
    bool bSkillTargetHighlighted = false;

};
