#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameModeBase.generated.h"

class ACombatManager;

UCLASS()
class PROJECTA_API ACombatGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACombatGameModeBase();

protected:
    virtual void BeginPlay() override;

private:

    // CombatManager 클래스 (에디터에서 지정 가능)
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TSubclassOf<ACombatManager> CombatManagerClass;

    // 현재 전투 매니저 인스턴스 (서버 전용)
    UPROPERTY()
    ACombatManager* CombatManager;

    // 전투 시작
    void StartCombat();
};