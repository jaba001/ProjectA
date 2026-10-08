#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Game/Snapshot/PartySnapshotTypes.h"
#include "PartySnapshotSelectionLibrary.generated.h"

class UOpponentSnapshotCatalogDataAsset;

// Local candidate metadata is value data and does not establish remote publication authority.
// 로컬 후보 메타데이터는 값 데이터이며 원격 게시의 권위를 증명하지 않습니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FPartySnapshotCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FPartySnapshot Snapshot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 ProgressStage = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FGameplayTagContainer Tags;
};

// Local slots provide stored value data only; caller metadata is not online publication authority.
// 로컬 슬롯은 저장된 값 데이터만 제공하며 호출자 메타데이터는 온라인 게시 권위가 아닙니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FPartySnapshotSlotCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FName SlotId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    int32 ProgressStage = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Snapshot")
    FGameplayTagContainer Tags;
};

UCLASS()
class PROJECTA_API UPartySnapshotSelectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Select a validated value copy uniformly; failure preserves the previous output and random stream.
    // 검증된 값 사본을 균등 추첨하며 실패하면 이전 출력과 난수 상태를 보존합니다.
    UFUNCTION(BlueprintCallable, Category = "Snapshot|Development")
    static bool SelectOpponent(const TArray<FPartySnapshotCandidate>& Candidates, const UOpponentSnapshotCatalogDataAsset* Catalog, int32 ProgressStage, const FGameplayTagQuery& Query, int32 FormationSlotCount, UPARAM(ref) FRandomStream& Random, FPartySnapshot& OutSnapshot, FText& OutError);

    // Read matching local slots and reuse the shared selector without saving or changing a Run's fixed opponent.
    // 일치하는 로컬 슬롯을 읽어 공통 선별기를 사용하며 저장하거나 Run의 고정 상대를 변경하지 않습니다.
    UFUNCTION(BlueprintCallable, Category = "Snapshot|Development")
    static bool LoadAndSelectOpponent(const TArray<FPartySnapshotSlotCandidate>& Slots, const UOpponentSnapshotCatalogDataAsset* Catalog, int32 ProgressStage, const FGameplayTagQuery& Query, int32 FormationSlotCount, UPARAM(ref) FRandomStream& Random, FPartySnapshot& OutSnapshot, FText& OutError);
};
