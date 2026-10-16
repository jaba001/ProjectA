#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EncounterMazeBakeLibrary.generated.h"

UCLASS()
class PROJECTAEDITOR_API UEncounterMazeBakeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Bake the shared BP_EncounterMaze_V7; keep manual edits unless overwrite is requested.
    // 공통 BP_EncounterMaze_V7을 굽고 덮어쓰기를 요청하지 않으면 수동 수정을 유지합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Encounter Maze")
    static int32 BakeEncounterMazeBlueprints(bool bOverwriteExisting, FString& OutReport);

    // Remove unreferenced retired maze Blueprint and generated-class redirectors through editor APIs.
    // 참조가 없는 폐기 미로 Blueprint와 생성 클래스의 Redirector만 에디터 API로 정리합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Encounter Maze")
    static bool FixupRetiredMazeRedirectors();
};
