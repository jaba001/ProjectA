#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EncounterMazeBakeLibrary.generated.h"

UCLASS()
class PROJECTAEDITOR_API UEncounterMazeBakeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Bake each maze variant into BP_EncounterMaze_V0~V7; existing Blueprints keep manual edits unless overwrite is requested.
    // 미로 변형마다 BP_EncounterMaze_V0~V7로 굽고, 덮어쓰기를 요청하지 않으면 기존 Blueprint의 수동 수정은 유지합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Encounter Maze")
    static int32 BakeEncounterMazeBlueprints(bool bOverwriteExisting, FString& OutReport);
};
