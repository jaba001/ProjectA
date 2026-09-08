#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PartyPlayerController.generated.h"

class AUnitBase;
class ACombatManager;
class ACombatGridTile;
class UUserWidget;
class USkillDefinitionDataAsset;

// Tile input mode selected by the player controller.
// 플레이어 컨트롤러에서 선택한 타일 입력 모드입니다.
UENUM(BlueprintType)
enum class ETileInputMode : uint8
{
    None,
    Skill,
    Move,
    Item
};

// Player controller that bridges combat UI input and combat actions.
// 전투 UI 입력과 전투 행동을 연결하는 플레이어 컨트롤러입니다.
UCLASS()
class PROJECTA_API APartyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    // Sets controller defaults for combat input.
    // 전투 입력을 위한 컨트롤러 기본값을 설정합니다.
    APartyPlayerController();

protected:
    // Finds required combat actors and initializes HUD.
    // 필요한 전투 액터를 찾고 HUD를 초기화합니다.
    virtual void BeginPlay() override;

    virtual bool ShouldCreateCombatHUD() const { return true; }

private:
    // Finds and caches the combat manager.
    // 전투 매니저를 찾아 캐시합니다.
    void InitializeCombatManager();

    // Creates and stores the combat HUD widget.
    // 전투 HUD 위젯을 생성하고 보관합니다.
    void InitializeHUD();

public:
    void SetCombatContext(ACombatManager* InManager, bool bEnableInput);
    // Returns the currently active combat unit.
    // 현재 활성화된 전투 유닛을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    AUnitBase* GetActiveUnit() const;

    // Requests the active unit's turn end.
    // 활성 유닛의 턴 종료를 요청합니다.
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
    void EnterSkillMode(USkillDefinitionDataAsset* SkillData);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void CancelTileInputMode();

    UFUNCTION(BlueprintCallable, Category = "Tile")
    ETileInputMode GetTileInputMode() const { return CurrentTileInputMode; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsMoveInputMode() const { return CurrentTileInputMode == ETileInputMode::Move; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsSkillInputMode() const { return CurrentTileInputMode == ETileInputMode::Skill; }

    UFUNCTION(BlueprintCallable, Category = "Tile")
    USkillDefinitionDataAsset* GetPendingSkillData() const { return PendingSkillData; }

    bool IsValidTileForPendingSkill(ACombatGridTile* Tile) const;

private:
    bool bCombatInputEnabled = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (AllowPrivateAccess = "true"))
    ACombatGridTile* SelectedTile = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile", meta = (AllowPrivateAccess = "true"))
    ETileInputMode CurrentTileInputMode = ETileInputMode::None;

    // Skill definition data currently pending in skill input mode
    UPROPERTY()
    TObjectPtr<USkillDefinitionDataAsset> PendingSkillData = nullptr;

private:
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    UUserWidget* HUDWidget = nullptr;

    UPROPERTY()
    ACombatManager* CombatManager = nullptr;
};
