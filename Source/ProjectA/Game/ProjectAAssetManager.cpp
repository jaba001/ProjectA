#include "Game/ProjectAAssetManager.h"

#if WITH_EDITOR
#include "Game/Run/RunItemShopCatalog.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectAAssetManager, Log, All);

void UProjectAAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
    Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
    TArray<FRunItemDefinition> Catalog;
    FText Error;
    if (!RunItemShopCatalog::Load(Catalog, Error))
    {
        UE_LOG(LogProjectAAssetManager, Error, TEXT("Cannot collect item catalog packages for cooking: %s"), *Error.ToString());
        return;
    }

    // CSV paths are runtime references; include every listed item without cooking its entire source pack.
    // CSV 경로는 런타임 참조이므로 원본 팩 전체 대신 목록에 기록된 아이템을 모두 포함합니다.
    for (const FRunItemDefinition& Item : Catalog)
    {
        const FName Package = Item.Asset.GetLongPackageFName();
        if (PackagesToNeverCook.Contains(Package) || !FPackageName::DoesPackageExist(Package.ToString()))
        {
            UE_LOG(LogProjectAAssetManager, Error, TEXT("Item catalog package is missing or excluded from cooking: %s"), *Package.ToString());
            continue;
        }
        PackagesToCook.AddUnique(Package);
    }
}
#endif
