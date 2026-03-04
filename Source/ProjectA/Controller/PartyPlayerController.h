#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Turn/TurnManager.h"
#include "PartyPlayerController.generated.h"

class AUnitBase;

UCLASS()
class PROJECTA_API APartyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APartyPlayerController();

protected:
    virtual void BeginPlay() override;

public:
    // 파티 유닛 클래스 목록 (테스트용, 현재는 블루프린트 스폰 방식 사용 예정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
    TArray<TSubclassOf<AUnitBase>> UnitClasses;

    // 실제 게임에 존재하는 유닛들의 참조 목록
    // 블루프린트에서 스폰 완료 후 이 배열에 Add하면 된다
    UPROPERTY(BlueprintReadOnly, Category = "Party")
    TArray<AUnitBase*> PartyUnits;

    // 현재 선택된 유닛의 인덱스
    // PartyUnits 배열에서 어느 유닛을 조종할지 결정
    UPROPERTY(BlueprintReadOnly, Category = "Party")
    int32 ActiveUnitIndex;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UTurnManager* TurnManager;

private:
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    UUserWidget* HUDWidget;

public:
    // 현재 조종할 유닛을 반환
    UFUNCTION(BlueprintCallable, Category = "Party")
    AUnitBase* GetActiveUnit() const;

    // 다음 유닛으로 선택 전환
    // PartyUnits 배열을 순환하며 전환한다
    UFUNCTION(BlueprintCallable, Category = "Party")
    void SelectNextUnit();

    UFUNCTION(BlueprintCallable)
    void RegisterPartyUnits(const TArray<AUnitBase*>& Units, const TArray<ACombatGridTile*>& StartTiles);


};
