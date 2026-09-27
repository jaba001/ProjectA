#pragma once

#include "NativeGameplayTags.h"

// Shared effect and content tags keep authored skills connected to GAS execution.
// 공통 효과·콘텐츠 태그로 제작 스킬과 GAS 실행을 연결합니다.
namespace ProjectACombatTags
{
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Damage);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Heal);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Effect_Shield);
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
}
