#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"

class AUnitBase;
class UCapsuleComponent;
class UWorld;
enum class ETeam : uint8;

// Share eligibility and wall rules while each attack retains its authored geometry.
// 공격별 제작 지오메트리는 유지하고 대상 자격과 벽 판정 규칙을 공유합니다.
namespace CombatCollisionPolicy
{
    PROJECTA_API FCollisionResponseParams WorldResponses();
    PROJECTA_API FCollisionQueryParams WorldQuery(UWorld* World, const AActor* IgnoredActor);
    PROJECTA_API bool IsLivingEnemy(const UWorld* World, const AUnitBase* Source, ETeam SourceTeam, const AUnitBase* Target);
    PROJECTA_API UCapsuleComponent* TargetCapsule(const UWorld* World, const AUnitBase* Source, ETeam SourceTeam, AUnitBase* Target);
    PROJECTA_API bool IsEarlierContact(float Time, int32 UnitId, float OtherTime, int32 OtherUnitId);
    PROJECTA_API bool IsBlockedByWorld(bool bHitWorld, float WorldTime, float UnitTime);
}
