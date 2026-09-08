#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameplayActionButton.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameplayActionRequested, FName);

// Carries a data identifier for dynamically generated node and skill buttons.
// 동적으로 생성하는 노드 및 스킬 버튼에 데이터 식별자를 연결합니다.
UCLASS()
class PROJECTA_API UGameplayActionButton : public UButton
{
    GENERATED_BODY()

public:
    void Configure(FName InActionId, const FText& Label);
    FOnGameplayActionRequested OnActionRequested;

private:
    UFUNCTION()
    void HandleClicked();

    FName ActionId;
};
