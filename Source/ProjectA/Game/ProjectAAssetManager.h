#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "ProjectAAssetManager.generated.h"

UCLASS()
class PROJECTA_API UProjectAAssetManager : public UAssetManager
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook) override;
#endif
};
