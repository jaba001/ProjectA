#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Game/Run/RunLevelDesignTypes.h"
#include "RunPveDifficultyTypes.generated.h"

// Freeze player-selected multipliers without changing the underlying monster roster or skill rules.
// 원본 몬스터 편성과 스킬 규칙을 바꾸지 않고 플레이어가 고르는 배율을 고정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunPveDifficultyRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FGameplayTag DifficultyTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    FName ArenaId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float HPScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float SpeedScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    float GoldScale = 1.f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunPveDifficultyState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 SchemaVersion = 0;
    // Older saves retain their original arena; new Runs freeze the presentation profile separately from balance.
    // 이전 저장은 원래 무대를 유지하며 새 Run은 밸런스와 별도로 연출 프로필을 고정합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 PresentationVersion = 0;
    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FRunPveDifficultyRule> Rules;
    // One entry per started PvE, including the current unfinished combat.
    // 현재 미완료 전투를 포함하여 시작한 PvE마다 한 항목을 보관합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FGameplayTag> SelectedTags;
};

// Preview only derived values; the server still validates the requested difficulty tag.
// 파생 값만 미리 표시하며 요청한 난이도 태그는 서버가 검증합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunPveDifficultyOffer
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag DifficultyTag;
    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;
    UPROPERTY(BlueprintReadOnly)
    FName ArenaId;
    UPROPERTY(BlueprintReadOnly)
    TArray<FRunMonsterDefinition> EnemyRoster;
    UPROPERTY(BlueprintReadOnly)
    float HPScale = 1.f;
    UPROPERTY(BlueprintReadOnly)
    float SpeedScale = 1.f;
    UPROPERTY(BlueprintReadOnly)
    float GoldScale = 1.f;
    UPROPERTY(BlueprintReadOnly)
    int32 EnemyCount = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 GoldMin = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 GoldMax = 0;
    UPROPERTY(BlueprintReadOnly)
    float TotalEnemyHP = 0.f;
};
