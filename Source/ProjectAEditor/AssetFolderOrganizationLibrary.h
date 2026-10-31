#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetFolderOrganizationLibrary.generated.h"

// Provide the official redirector fixup API to the audited project folder migration.
// 감사한 프로젝트 폴더 이동에 엔진의 공식 Redirector 정리 API를 제공합니다.
UCLASS()
class PROJECTAEDITOR_API UAssetFolderOrganizationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool RenameAuditedFolderAssets(const TArray<UObject*>& Assets);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool ConsolidateRetainedFolderAsset(UObject* OldAsset, UObject* NewAsset);

    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static bool FixupFolderRedirectors(const TArray<FName>& PackageNames);
};
