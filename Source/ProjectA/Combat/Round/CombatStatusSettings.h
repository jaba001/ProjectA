#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CombatStatusSettings.generated.h"

UENUM(BlueprintType)
enum class ECombatBurnDecay : uint8
{
    // NextBurn = floor(B / 2)
    HalfFloor,
    // NextBurn = max(0, floor(B / 2) - 1)
    HalfFloorMinusOne
};

// Tunable status numbers live in project config so balance changes never require code edits.
// 조정 가능한 상태 수치는 프로젝트 설정에 두어 밸런스 변경에 코드 수정이 필요하지 않게 합니다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ProjectA Status Effects"))
class PROJECTA_API UProjectAStatusEffectSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    UPROPERTY(Config, EditAnywhere, Category = "Burn")
    ECombatBurnDecay BurnDecay = ECombatBurnDecay::HalfFloorMinusOne;

    UPROPERTY(Config, EditAnywhere, Category = "Duration", meta = (ClampMin = "1", ClampMax = "20"))
    int32 BleedingRounds = 2;

    UPROPERTY(Config, EditAnywhere, Category = "Duration", meta = (ClampMin = "1", ClampMax = "20"))
    int32 ChillRounds = 2;

    UPROPERTY(Config, EditAnywhere, Category = "Duration", meta = (ClampMin = "1", ClampMax = "20"))
    int32 ShockRounds = 2;

    // Speed removed per chill point; the existing 0.1 second per speed schedule is unchanged.
    // 냉각 1당 감소하는 속도이며 기존 속도 1당 0.1초 시각표는 그대로입니다.
    UPROPERTY(Config, EditAnywhere, Category = "Chill", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float ChillSpeedPerPoint = 1.f;

    UPROPERTY(Config, EditAnywhere, Category = "Chill", meta = (ClampMin = "0.1", ClampMax = "100.0"))
    float MinimumChilledSpeed = 1.f;
};
