#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainMenuPreviewStage.generated.h"

class UCameraComponent;
class USceneComponent;

// World-space preview stage that owns the main menu camera and four character preview slots.
// 메인메뉴 카메라와 네 개의 캐릭터 프리뷰 슬롯을 소유하는 월드 공간 프리뷰 스테이지입니다.
UCLASS()
class PROJECTA_API AMainMenuPreviewStage : public AActor
{
    GENERATED_BODY()

public:
    AMainMenuPreviewStage();

    // Replaces the preview actor assigned to one slot using the configured class map.
    // 설정된 클래스 맵을 사용해 한 슬롯의 프리뷰 액터를 교체합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu|Preview")
    void SetPreviewActorForSlot(int32 SlotIndex, FName ClassId);

    // Removes the preview actor currently assigned to one slot.
    // 한 슬롯에 현재 배치된 프리뷰 액터를 제거합니다.
    UFUNCTION(BlueprintCallable, Category = "MainMenu|Preview")
    void ClearPreviewActorForSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "MainMenu|Preview")
    void ClearAllPreviewActors();

    UFUNCTION(BlueprintPure, Category = "MainMenu|Preview")
    AActor* GetPreviewActorForSlot(int32 SlotIndex) const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // Root component used to move the entire preview stage in L_MainMenu.
    // L_MainMenu에서 전체 프리뷰 스테이지를 이동하기 위한 루트 컴포넌트입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TObjectPtr<USceneComponent> SceneRoot;

    // Camera used as the player view while the main menu is active.
    // 메인메뉴 활성 중 플레이어 시점으로 사용하는 카메라입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TObjectPtr<UCameraComponent> PreviewCamera;

    // Preview actor spawn anchors for party slots zero through three.
    // 파티 슬롯 0부터 3까지의 프리뷰 액터 스폰 앵커입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TObjectPtr<USceneComponent> Slot0Anchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TObjectPtr<USceneComponent> Slot1Anchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TObjectPtr<USceneComponent> Slot2Anchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TObjectPtr<USceneComponent> Slot3Anchor;

    // Maps class ids from the UI to actor classes configured in the level instance.
    // UI 클래스 ID를 레벨 인스턴스에서 설정한 액터 클래스에 연결합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Preview")
    TMap<FName, TSubclassOf<AActor>> PreviewActorClasses;

private:
    USceneComponent* GetSlotAnchor(int32 SlotIndex) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedPreviewActors;
};
