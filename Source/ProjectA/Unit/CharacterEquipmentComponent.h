#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Run/RunEquipmentTypes.h"
#include "CharacterEquipmentComponent.generated.h"

class UMeshComponent;

// Replicate the loadout marker and its visual entries as one coherent property.
// 장착 여부와 표시 항목을 하나의 일관된 속성으로 복제합니다.
USTRUCT()
struct FCharacterEquipmentPresentation
{
    GENERATED_BODY()

    UPROPERTY()
    bool bHasLoadout = false;

    UPROPERTY()
    TArray<FRunEquipmentVisual> Visuals;
};

// Equipment meshes are instance-only visuals; combat traces retain their authored components.
// 장비 메시는 인스턴스 표시 전용이며 전투 판정은 기존 작성된 컴포넌트를 유지합니다.
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UCharacterEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterEquipmentComponent();

    bool SetEquipment(bool bHasLoadout, const TArray<FRunEquipmentVisual>& Visuals);
    bool RefreshEquipment();
    bool HasEquipmentLoadout() const { return Presentation.bHasLoadout; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    virtual void OnUnregister() override;

private:
    UPROPERTY(ReplicatedUsing = OnRep_Presentation)
    FCharacterEquipmentPresentation Presentation;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMeshComponent>> EquipmentMeshes;

    UFUNCTION()
    void OnRep_Presentation();

    void RemoveEquipmentMeshes();
};
