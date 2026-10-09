#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "RunItemSaleTypes.generated.h"

// Bind a sale to one owned copy and both revisions so delayed requests cannot reuse a shifted inventory index.
// 지연된 요청이 이동한 인벤토리 인덱스를 재사용하지 못하도록 판매를 보유 사본과 두 버전에 연결합니다.
USTRUCT()
struct PROJECTA_API FRunItemSaleCommand
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid CharacterId;

    UPROPERTY()
    int32 ItemIndex = INDEX_NONE;

    UPROPERTY()
    FSoftObjectPath Asset;

    UPROPERTY()
    FGuid ItemInstanceId;

    UPROPERTY()
    int32 ExpectedEquipmentRevision = INDEX_NONE;

    UPROPERTY()
    int32 ExpectedShopRevision = INDEX_NONE;

    UPROPERTY()
    FName EncounterId;
};
