#pragma once

#include "CoreMinimal.h"
#include "Controller/PartyPlayerController.h"
#include "CombatDebugPlayerController.generated.h"

class UCombatDebugLoadout;
class UCombatDebugWidget;

// Isolated local tooling never routes inventory edits through persistent Run commands.
// 독립 로컬 도구는 영속 Run 명령을 통해 장착을 편집하지 않습니다.
UCLASS()
class PROJECTA_API ACombatDebugPlayerController : public APartyPlayerController
{
    GENERATED_BODY()

public:
    UCombatDebugLoadout* GetDebugLoadout();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCombatDebugLoadout> DebugLoadout;

    UPROPERTY(Transient)
    TObjectPtr<UCombatDebugWidget> DebugWidget;
};
