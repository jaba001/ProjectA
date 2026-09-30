#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatVfxAssetLibrary.generated.h"

class UNiagaraSystem;

UCLASS()
class PROJECTAEDITOR_API UCombatVfxAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Inspect authored emitter spaces, renderers and rapid iteration values without changing the asset.
    // 에셋 변경 없이 작성된 이미터 공간, 렌더러와 빠른 반복 파라미터 값을 검사합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static FString InspectNiagaraSpace(UNiagaraSystem* System);

    // Change selected emitter spaces only in project-owned derivatives; source packages are rejected.
    // 원본 패키지를 거절하고 프로젝트 전용 파생본에서 선택한 이미터의 공간만 변경합니다.
    // Return an empty string on success or a diagnostic string that remains visible to Python on failure.
    // 성공 시 빈 문자열, 실패 시 Python에서도 보존되는 진단 문자열을 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static FString ConfigureEmitterSpace(UNiagaraSystem* System, const TArray<FName>& EmitterNames, bool bLocalSpace, bool bClearVelocity);

    // Upgrade both engine location-event modules to version 1.1 on derivative-owned nodes without changing source modules.
    // 원본 모듈을 바꾸지 않고 파생본 소유 노드의 위치 이벤트 생성·수신 모듈을 모두 1.1로 올립니다.
    UFUNCTION(BlueprintCallable, Category = "ProjectA|Asset Authoring")
    static FString UpgradeLocationEventToWorldVersion(UNiagaraSystem* System);
};
