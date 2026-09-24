#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Snapshot/PartySnapshotTypes.h"
#include "OpponentSnapshotCatalogDataAsset.generated.h"

class AEnemyUnit;
class USkillDefinitionDataAsset;

// Resolve external identifiers through trusted content, never through serialized actor paths.
// 외부 식별자는 직렬화한 액터 경로 대신 신뢰된 콘텐츠 목록에서 해석합니다.
UCLASS(BlueprintType)
class PROJECTA_API UOpponentSnapshotCatalogDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snapshot")
    int32 ContentVersion = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snapshot")
    TMap<FName, TSubclassOf<AEnemyUnit>> EnemyClasses;

    // Preserve declared older checkpoint actors only when no cosmetic selection was saved; new encounters use EnemyClasses.
    // 저장한 의상 선택이 없을 때만 명시한 이전 체크포인트 액터를 허용하며 새 전투는 EnemyClasses를 사용합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snapshot")
    TMap<FName, TSubclassOf<AEnemyUnit>> LegacyEnemyClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snapshot")
    TMap<FName, TObjectPtr<USkillDefinitionDataAsset>> Skills;

    bool ValidateForEncounter(const FPartySnapshot& Snapshot, int32 FormationSlotCount, FText& OutError) const;
    bool ResolveSkills(const FPartySnapshotMember& Member, TArray<TObjectPtr<USkillDefinitionDataAsset>>& OutSkills, FText& OutError) const;
    bool MatchesSavedUnitClass(const FPartySnapshotMember& Member, const FSoftObjectPath& SavedClass) const;
};
