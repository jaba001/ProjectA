#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Unit/CharacterAppearanceTypes.h"
#include "CharacterAppearanceComponent.generated.h"

class UCharacterAppearanceCatalog;
class USkeletalMeshComponent;

// Apply instance-only appearance settings without modifying source assets or combat state.
// 원본 에셋이나 전투 상태를 변경하지 않고 인스턴스의 외형 설정만 적용합니다.
UCLASS(ClassGroup = (Appearance), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UCharacterAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterAppearanceComponent();

    // Hide embedded mesh parts while keeping separately attached equipment and physics unchanged.
    // 메시 내장 부위를 숨기며 별도로 부착한 장비와 물리 설정은 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FName> HiddenMeshBones;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Appearance, Category = "Appearance")
    TObjectPtr<UCharacterAppearanceCatalog> AppearanceCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Appearance, Category = "Appearance")
    FCharacterAppearanceSelection Selection;

    UFUNCTION(BlueprintCallable, Category = "Appearance")
    bool SetAppearance(UCharacterAppearanceCatalog* InCatalog, const FCharacterAppearanceSelection& InSelection);

    UFUNCTION(BlueprintCallable, Category = "Appearance")
    bool RefreshAppearance();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    virtual void OnUnregister() override;

private:
    UFUNCTION()
    void OnRep_Appearance();

    UPROPERTY(Transient)
    TArray<TObjectPtr<USkeletalMeshComponent>> ModularMeshes;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> PoseLeader;

    bool bLeaderVisibilitySaved = false;
    bool bLeaderWasVisible = true;
    uint8 LeaderPreviousTickOption = 0;

    USkeletalMeshComponent* FindPoseLeader() const;
    void RemoveModularMeshes();
    void RestorePoseLeader();
    void ApplyHiddenMeshBones();
};
