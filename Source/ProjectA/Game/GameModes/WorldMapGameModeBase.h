#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WorldMapGameModeBase.generated.h"

// Deprecated compatibility class retained for the unused WorldMap asset; runs use GameplayGameModeBase.
// 미사용 WorldMap 에셋 호환을 위해 보존하는 클래스이며 실제 진행은 GameplayGameModeBase를 사용합니다.
UCLASS()
class PROJECTA_API AWorldMapGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    // Disables automatic pawn and spectator creation for the WorldMap test bed.
    // WorldMap 테스트 베드의 자동 폰과 관전자 생성을 비활성화합니다.
    AWorldMapGameModeBase();

    // Leaves player pawn creation to the future party spawn flow.
    // 플레이어 폰 생성을 향후 파티 스폰 흐름에 맡깁니다.
    virtual void RestartPlayer(AController* NewPlayer) override;
};
