#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"

// Worn armor and accessories are a trial catalog exposed only to debug combat until shops and rewards adopt them.
// 착용 방어구·액세서리는 상점·보상에 도입하기 전까지 디버그 전투에만 노출하는 시범 카탈로그입니다.
namespace RunArmorCatalog
{
    PROJECTA_API bool Load(TArray<FRunItemDefinition>& OutCatalog, FText& OutError);
    PROJECTA_API bool LoadFromString(FString CsvText, TArray<FRunItemDefinition>& OutCatalog, FText& OutError);
}
