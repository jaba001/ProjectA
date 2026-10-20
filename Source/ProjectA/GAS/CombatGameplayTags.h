#pragma once

#include "NativeGameplayTags.h"

// Shared effect and content tags keep authored skills connected to GAS execution.
// 공통 효과·콘텐츠 태그로 제작 스킬과 GAS 실행을 연결합니다.
namespace ProjectACombatTags
{
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Damage);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Heal);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Shield);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Heal);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Shield);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Element_Physical);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Element_Fire);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Element_Cold);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Element_Lightning);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Element_Chaos);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_Slash);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_Projectile);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_Area);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_Beam);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Shape_Chain);
    // Attack categories select the primary stat added to direct damage; bows carry neither.
    // 공격 분류는 직접 피해에 더할 기본 능력치를 고르며 활 계열은 어느 쪽도 갖지 않습니다.
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attack_Melee);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Attack_Magic);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_Release);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Burning);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Poisoned);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Bleeding);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Chilled);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frozen);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Stunned);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Confused);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Shocked);
}
