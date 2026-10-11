#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EncounterPresentationCaptureLibrary.generated.h"

class UWorld;

UCLASS()
class PROJECTAEDITOR_API UEncounterPresentationCaptureLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Capture transient production scenery in the loaded editor world without starting a Run or saving assets.
    // Run 시작이나 에셋 저장 없이 로드한 에디터 월드의 임시 실제 연출을 촬영합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Presentation Capture")
    static bool CaptureDungeonInteriors(UWorld* World, const FString& OutputDirectory);
};
