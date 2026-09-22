#include "Combat/Library/CombatCollisionPolicy.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Unit/UnitBase.h"

FCollisionResponseParams CombatCollisionPolicy::WorldResponses()
{
    FCollisionResponseParams Responses(ECR_Ignore);
    Responses.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
    Responses.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
    return Responses;
}

FCollisionQueryParams CombatCollisionPolicy::WorldQuery(UWorld* World, const AActor* IgnoredActor)
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatAttackWorld), false, IgnoredActor);
    Params.bFindInitialOverlaps = true;
    Params.bIgnoreTouches = true;
    if (World)
    {
        for (TActorIterator<APawn> It(World); It; ++It) Params.AddIgnoredActor(*It);
    }
    return Params;
}

bool CombatCollisionPolicy::IsLivingEnemy(const UWorld* World, const AUnitBase* Source, ETeam SourceTeam, const AUnitBase* Target)
{
    // Source survival is deliberately excluded so released projectiles outlive their caster.
    // 이미 발사된 투사체가 시전자 사망 후에도 유지되도록 시전자 생존은 검사하지 않습니다.
    return IsValid(Target) && Target->GetWorld() == World && Target != Source && Target->IsUnitAlive() && Target->GetTeam() != SourceTeam;
}

UCapsuleComponent* CombatCollisionPolicy::TargetCapsule(const UWorld* World, const AUnitBase* Source, ETeam SourceTeam, AUnitBase* Target)
{
    if (!IsLivingEnemy(World, Source, SourceTeam, Target) || !Target->GetActorEnableCollision()) return nullptr;
    UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
    return IsValid(Capsule) && Capsule->IsQueryCollisionEnabled() ? Capsule : nullptr;
}

bool CombatCollisionPolicy::IsEarlierContact(float Time, int32 UnitId, float OtherTime, int32 OtherUnitId)
{
    return Time < OtherTime || (Time == OtherTime && UnitId < OtherUnitId);
}

bool CombatCollisionPolicy::IsBlockedByWorld(bool bHitWorld, float WorldTime, float UnitTime)
{
    return bHitWorld && WorldTime <= UnitTime;
}
