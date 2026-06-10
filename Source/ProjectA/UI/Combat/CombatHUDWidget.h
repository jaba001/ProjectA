#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class ACombatManager;

// HUD widget that exposes combat turn text for UI binding.
// 전투 턴 텍스트를 UI 바인딩에 제공하는 HUD 위젯입니다.
UCLASS()
class PROJECTA_API UCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Returns current turn information for display.
    // 표시용 현재 턴 정보를 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "BattleHUD")
    FText GetTurnInfoText() const;

protected:
    // Caches combat references after widget construction.
    // 위젯 생성 후 전투 참조를 캐시합니다.
    virtual void NativeConstruct() override;

private:
    // Cached combat manager used by HUD text getters.
    // HUD 텍스트 Getter에서 사용하는 캐시된 전투 매니저입니다.
    UPROPERTY()
    ACombatManager* CachedCombatManager = nullptr;
};
