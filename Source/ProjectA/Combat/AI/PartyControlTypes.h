#pragma once

#include "CoreMinimal.h"
#include "PartyControlTypes.generated.h"

// Control mode changes who submits actions, never the character's original owner or team.
// 조작 모드는 행동 요청 주체만 바꾸며 캐릭터의 원래 소유자나 팀을 바꾸지 않습니다.
UENUM(BlueprintType)
enum class EPartyControlMode : uint8
{
    Human,
    ServerAI
};
