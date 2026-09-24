#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SingleAnimationPlayData.h"
#include "Unit/CharacterAppearanceTypes.h"
#include "CharacterAppearanceComponent.generated.h"

class UCharacterAppearanceCatalog;
class USkeletalMeshComponent;
class USkeletalMesh;
class UPhysicsAsset;
class UMaterialInterface;
struct FCharacterAppearanceBodyVariant;

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

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> OriginalBodyLeader;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMesh> OriginalBodyMesh;

    UPROPERTY(Transient)
    TObjectPtr<UPhysicsAsset> OriginalBodyPhysics;

    UPROPERTY(Transient)
    TObjectPtr<UClass> OriginalAnimationClass;

    UPROPERTY(Transient)
    FSingleAnimationPlayData OriginalAnimationData;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> OriginalMaterialOverrides;

    UPROPERTY(Transient)
    TObjectPtr<UCharacterAppearanceCatalog> AppliedCatalog;

    UPROPERTY(Transient)
    FCharacterAppearanceSelection AppliedSelection;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMesh> AppliedBodyMesh;

    FTransform OriginalMeshTransform = FTransform::Identity;
    FTransform AppliedMeshTransform = FTransform::Identity;
    uint8 OriginalAnimationMode = 0;
    bool bBodyVariantApplied = false;
    bool bAppearanceApplied = false;
    bool bLeaderVisibilitySaved = false;
    bool bLeaderWasVisible = true;
    uint8 LeaderPreviousTickOption = 0;

    USkeletalMeshComponent* FindPoseLeader() const;
    void RemoveModularMeshes();
    void RestorePoseLeader();
    void SaveOriginalBody(USkeletalMeshComponent* Leader);
    void RestoreOriginalBody();
    void ApplyHiddenMeshBones();
};
