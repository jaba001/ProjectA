#include "Combat/CombatManager.h"
#include "Grid/Combat/CombatGridTile.h"

// Preserve reflected names and inert behavior for existing assets; new combat uses round planning.
// 기존 에셋의 리플렉션 이름과 비활성 동작을 유지하며 새 전투는 라운드 계획을 사용합니다.

bool ACombatManager::IsAwaitingTurnCheckpoint() const
{
    return false;
}

bool ACombatManager::RetryTurnCheckpoint()
{
    UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Sequential turn checkpoint retry is unsupported by timed-round combat."));
    return false;
}

bool ACombatManager::RestoreCombatFromBoundary(int32 CompletedTurnSerial, int32 NextTurnIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Sequential turn checkpoint restoration is unsupported by timed-round combat."));
    return false;
}

void ACombatManager::RequestEndTurn()
{
    UE_LOG(LogTemp, Warning, TEXT("[CombatManager] End-turn input is unsupported; submit timed-round readiness instead."));
}

bool ACombatManager::RequestEndTurnForUnit(AUnitBase* RequestingUnit)
{
    return false;
}

void ACombatManager::RefreshReachableMoveTiles()
{
    // Old active-turn highlights are inert; the planning widget owns selection previews.
    // 기존 활성 턴 강조는 비활성화하며 계획 위젯이 선택 미리보기를 소유합니다.
    ClearMovableTilesHighlight();
    ReachableMoveTiles.Reset();
}

bool ACombatManager::IsReachableMoveTile(ACombatGridTile* Tile) const
{
    if (!Tile)
    {
        return false;
    }

    return ReachableMoveTiles.Contains(Tile);
}

void ACombatManager::HighlightMovableTiles()
{
    for (ACombatGridTile* Tile : ReachableMoveTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ApplyMovableTileVisual();
    }
}

void ACombatManager::ClearMovableTilesHighlight()
{
    for (ACombatGridTile* Tile : ReachableMoveTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ClearHighlightVisual();
    }
}

void ACombatManager::RefreshSkillTargetTiles()
{
    ClearSkillTargetTilesHighlight();
    SkillTargetTiles.Reset();
}

bool ACombatManager::IsSkillTargetTile(ACombatGridTile* Tile) const
{
    if (!Tile)
    {
        return false;
    }

    return SkillTargetTiles.Contains(Tile);
}

void ACombatManager::HighlightSkillTargetTiles()
{
    for (ACombatGridTile* Tile : SkillTargetTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ApplySkillTargetTileVisual();
    }
}

void ACombatManager::ClearSkillTargetTilesHighlight()
{
    for (ACombatGridTile* Tile : SkillTargetTiles)
    {
        if (!Tile)
        {
            continue;
        }

        Tile->ClearHighlightVisual();
    }

    SkillTargetTiles.Empty();
}
