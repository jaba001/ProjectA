#pragma once

#include "CoreMinimal.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Game/Run/RunTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatCheckpointLibrary.generated.h"

// Validate persisted values and supported assets before changing a Run or spawning actors.
// Run 변경이나 액터 생성 전에 저장 값과 지원 에셋을 검증합니다.
UCLASS()
class PROJECTA_API UCombatCheckpointLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentContentVersion = 1;
    static bool Validate(const FCombatCheckpointData& Checkpoint, const TArray<FRunPartyMember>& Party, FText& OutError);
};
