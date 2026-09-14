#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Turn/TurnManager.h"
#include "Combat/Network/CombatViewTypes.h"
#include "CombatManager.generated.h"

class UTurnManager;
class AUnitBase;
class ACombatGridManager;
class ACombatGridTile;
class UCombatActionAuthority;
class ACombatRoundCoordinator;

DECLARE_MULTICAST_DELEGATE(FOnCombatViewChanged);

// Connects persistent encounter ownership to the authoritative timed-round combat session.
// 영속 인카운터 소유권을 서버 권위의 시간차 라운드 전투에 연결합니다.
UCLASS()
class PROJECTA_API ACombatManager : public AActor
{
    GENERATED_BODY()

public:
    FOnCombatResult OnCombatResult;
    FOnCombatViewChanged OnCombatViewChanged;
    FCommitCombatTurnBoundary CommitTurnBoundary;
    bool IsAwaitingTurnCheckpoint() const;
    bool RetryTurnCheckpoint();
    bool RestoreCombatFromBoundary(int32 CompletedTurnSerial, int32 NextTurnIndex);
    void SuspendCombatForRecovery();
    bool IsCombatActive() const;
    void EndCombat();
    void ResetCombat();
    void SetCombatGrid(ACombatGridManager* Grid);
    const TArray<AUnitBase*>& GetRegisteredUnits() const { return CombatUnits; }
    UCombatActionAuthority* GetActionAuthority() const { return ActionAuthority; }
    ACombatRoundCoordinator* GetRoundCoordinator() const { return RoundCoordinator; }
    ACombatGridManager* GetCombatGrid() const { return CombatGridManager; }
    const FCombatViewState& GetCombatViewState() const { return CombatView; }
    FGuid GetCombatInstanceId() const { return CombatView.CombatInstanceId; }
    FGuid GetRunId() const { return CombatView.RunId; }
    int32 GetHostEpoch() const { return CombatView.HostEpoch; }
    int32 GetTurnSerial() const;
    ECombatResult GetCombatResult() const;
    FGuid GetRuntimeUnitId(const AUnitBase* Unit) const;
    AUnitBase* ResolveRuntimeUnit(FGuid UnitId) const;
    FGuid GetCharacterId(const AUnitBase* Unit) const;
    FRunAccountId GetOwnerAccountId(const AUnitBase* Unit) const;
    bool IsPartyAIControlled(const AUnitBase* Unit) const;

    // Publish from server state changes; clients can only consume the replicated view.
    // 서버 상태 변경 시 게시하며 클라이언트는 복제된 뷰만 읽습니다.
    void PublishCombatView();

    // Sets combat manager defaults.
    // 전투 매니저 기본값을 설정합니다.
    ACombatManager();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // Initializes combat references after actor startup.
    // 액터 시작 후 전투 참조를 초기화합니다.
    virtual void BeginPlay() override;

    // Registers replicated combat state.
    // 복제되는 전투 상태를 등록합니다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


private:
    UPROPERTY(ReplicatedUsing = OnRep_CombatView)
    FCombatViewState CombatView;

    UFUNCTION()
    void OnRep_CombatView();

    UFUNCTION()
    void OnRep_CombatGrid();

    UFUNCTION()
    void OnRep_RoundCoordinator();

    bool bSuspendedForRecovery = false;
    UPROPERTY(VisibleAnywhere, Category = "Combat|Commands")
    TObjectPtr<UCombatActionAuthority> ActionAuthority;

    void HandleUnitDied(AUnitBase* Unit);
    void HandleCombatResult(ECombatResult Result);
    // Player unit classes
    // 플레이어 유닛 클래스 목록입니다.
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> PlayerUnitClasses;

    // Enemy unit classes
    // 적 유닛 클래스 목록입니다.
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TArray<TSubclassOf<AUnitBase>> EnemyUnitClasses;

private:
    // Clients discover the same server-owned round coordinator through this reference.
    // 클라이언트는 이 참조로 동일한 서버 소유 라운드 조정자를 찾습니다.
    UPROPERTY(ReplicatedUsing = OnRep_RoundCoordinator)
    TObjectPtr<ACombatRoundCoordinator> RoundCoordinator = nullptr;

    // Combat unit list (owned by server)
    UPROPERTY()
    TArray<AUnitBase*> CombatUnits;

    // The server caches its index; clients read the same value from the combat view.
    // 서버는 턴 인덱스를 캐시하며 클라이언트는 전투 뷰의 같은 값을 읽습니다.
    UPROPERTY()
    int32 CurrentTurnIndex = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_CombatGrid)
    ACombatGridManager* CombatGridManager;

    UPROPERTY()
    TArray<ACombatGridTile*> ReachableMoveTiles;

public:
    // Serialized Blueprint references remain readable but cannot start sequential combat.
    // 직렬화된 블루프린트 참조는 유지하지만 순차 전투를 시작할 수 없습니다.
    UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Sequential combat was replaced by timed-round combat."))
    UTurnManager* GetTurnManager() const { return nullptr; }

    UFUNCTION(Server, Reliable)
    void Server_StartCombat();

    void StartCombat_Internal();

    // Old end-turn requests are rejected; readiness belongs to round planning.
    // 기존 턴 종료 요청은 거절하며 준비 완료는 라운드 계획에서 처리합니다.
    UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use timed-round planning readiness."))
    void RequestEndTurn();

    // Internal callers must identify the living, idle unit whose turn they are ending.
    // 내부 호출자는 종료할 턴의 생존하고 행동 중이 아닌 유닛을 명시해야 합니다.
    bool RequestEndTurnForUnit(AUnitBase* RequestingUnit);

    // Register units (server only)
    void RegisterUnits(const TArray<AUnitBase*>& Units);

    int32 GetCurrentTurnIndex() const { return CurrentTurnIndex; }

    UFUNCTION(BlueprintCallable)
    AUnitBase* GetCurrentUnit() const;

private:
    void ClearPlayerSelection();

public:
    // Move
    UFUNCTION(BlueprintCallable, Category = "Move")
    void RefreshReachableMoveTiles();

    UFUNCTION(BlueprintCallable, Category = "Move")
    bool IsReachableMoveTile(ACombatGridTile* Tile) const;

    UFUNCTION(BlueprintCallable, Category = "Move")
    void HighlightMovableTiles();

    UFUNCTION(BlueprintCallable, Category = "Move")
    void ClearMovableTilesHighlight();

    UFUNCTION(BlueprintCallable, Category = "Move")
    const TArray<ACombatGridTile*>& GetReachableMoveTiles() const { return ReachableMoveTiles; }

    // Skill
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void RefreshSkillTargetTiles();

    UFUNCTION(BlueprintCallable, Category = "Skill")
    bool IsSkillTargetTile(ACombatGridTile* Tile) const;

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void HighlightSkillTargetTiles();

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void ClearSkillTargetTilesHighlight();

    // Tile
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatGridTile* GetTileByCoord(FIntPoint Coord) const;

    // Clears retired front-line protection presentation.
    // 제거된 전열 보호 표시를 해제합니다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RefreshTileProtectedByFront();

private:
    UPROPERTY()
    TArray<ACombatGridTile*> SkillTargetTiles;

};
