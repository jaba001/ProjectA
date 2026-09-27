#pragma once

#include "CoreMinimal.h"

class AActor;
class UFXSystemComponent;
class UWorld;
struct FCombatSkillVfx;

// Cosmetic systems reference original assets and never provide authoritative hit results.
// 표현 시스템은 원본 에셋을 참조하며 권한 있는 피격 결과를 제공하지 않습니다.
namespace CombatSkillPresentation
{
    void Attach(AActor* Owner, const FCombatSkillVfx& Visual, TArray<TObjectPtr<UFXSystemComponent>>& Components);
    void Destroy(TArray<TObjectPtr<UFXSystemComponent>>& Components);
    void Impact(UWorld* World, const FCombatSkillVfx& Visual, const FTransform& Transform);
}
