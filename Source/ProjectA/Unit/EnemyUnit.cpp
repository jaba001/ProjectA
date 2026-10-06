#include "Unit/EnemyUnit.h"

AEnemyUnit::AEnemyUnit()
{
    InitMaxHP = 150.0f;
    MaxActionPoint = 2;
    // Training enemies use speed 5; snapshot configuration replaces it after spawning.
    // 테스트 적은 기본 속도 5를 사용하며 스냅샷 설정은 생성 후 저장된 값으로 교체합니다.
    AttributeSet->InitSpeed(5.0f);
}

void AEnemyUnit::OnTurnStart()
{
    // A legacy callback must never reactivate the removed automatic turn loop.
    // 기존 콜백이 제거된 자동 턴 루프를 다시 활성화하지 않도록 합니다.
    Super::OnTurnEnd();
}

void AEnemyUnit::SetTurnState(EEnemyTurnState NewState)
{
    UE_LOG(LogTemp, Warning, TEXT("[EnemyUnit] Sequential AI state changes are unsupported."));
}
