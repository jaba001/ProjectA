#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EncounterDefinitionDataAsset.generated.h"

class AEnemyUnit;
class UOpponentSnapshotCatalogDataAsset;

UCLASS(BlueprintType)
class PROJECTA_API UEncounterDefinitionDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter")
    TArray<TSubclassOf<AEnemyUnit>> EnemyUnitClasses;

    // Resolve optional debug choices on demand without loading the full monster catalog in normal Runs.
    // 선택적인 디버그 목록은 필요할 때 해석하며 일반 Run에서 전체 몬스터 카탈로그를 로드하지 않습니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter|DebugCatalog")
    TArray<TSoftClassPtr<AEnemyUnit>> EnemyCatalogClasses;

    // A configured snapshot slot replaces the PvE enemy list and must load successfully.
    // 스냅샷 슬롯이 지정되면 PvE 적 목록을 대체하며 반드시 정상적으로 불러와야 합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter|Snapshot")
    FName OpponentSnapshotSlot;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter|Snapshot")
    TObjectPtr<UOpponentSnapshotCatalogDataAsset> SnapshotCatalog;
};
