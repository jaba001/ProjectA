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
    static constexpr int32 CurrentSchemaVersion = 3;
    static constexpr int32 CurrentContentVersion = 1;
    // Resolve persisted command identifiers through the engine's configured primary asset redirects.
    // 엔진에 설정한 기본 에셋 리다이렉트로 저장된 명령 식별자를 해석합니다.
    static FName ResolveSavedSkillId(FName SkillId);
    static bool Validate(const FCombatCheckpointData& Checkpoint, const TArray<FRunPartyMember>& Party, FText& OutError);
};
