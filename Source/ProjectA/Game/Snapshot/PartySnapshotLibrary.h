#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Game/Snapshot/PartySnapshotTypes.h"
#include "PartySnapshotLibrary.generated.h"

UCLASS()
class PROJECTA_API UPartySnapshotLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Snapshot")
    static bool ValidateSnapshot(const FPartySnapshot& Snapshot, FText& OutError);

    // Synchronous slot operations are intended for small snapshots outside active combat.
    // 동기 슬롯 저장 작업은 진행 중인 전투 밖에서 작은 Snapshot을 처리할 때 사용합니다.
    UFUNCTION(BlueprintCallable, Category = "Snapshot")
    static bool SaveSnapshot(FName SlotId, const FPartySnapshot& Snapshot, FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "Snapshot")
    static bool LoadSnapshot(FName SlotId, FPartySnapshot& OutSnapshot, FText& OutError);

    UFUNCTION(BlueprintPure, Category = "Snapshot")
    static FString GetSaveSlotName(FName SlotId);
};
