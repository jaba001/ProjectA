#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Game/Run/RunTypes.h"
#include "UObject/Object.h"
#include "CombatDebugLoadout.generated.h"

class ACombatManager;
class ACombatRoundCoordinator;
class APlayerController;
class AUnitBase;

// Keep debug equipment copies in the current combat only, independently of persistent Run state.
// 디버그 장비 사본은 영속 Run 상태와 분리하여 현재 전투에서만 보관합니다.
UCLASS()
class PROJECTA_API UCombatDebugLoadout : public UObject
{
    GENERATED_BODY()

public:
    virtual UWorld* GetWorld() const override;
    void Initialize(ACombatManager* InManager);
    const TArray<FSoftObjectPath>& GetSkillAssets() const { return SkillAssets; }
    FText GetSkillLabel(const FSoftObjectPath& Asset) const;
    const FGameplayTagContainer& GetSkillTags(const FSoftObjectPath& Asset) const;
    const TArray<FRunItemDefinition>& GetEquipmentItems() const { return EquipmentItems; }
    const FRunPartyMember* GetEquipmentMember(int32 UnitId) const;
    bool GrantEquipment(APlayerController* Controller, int32 UnitId, const FSoftObjectPath& Asset, FGameplayTag Slot, FText& OutError);
    bool RemoveEquipment(APlayerController* Controller, int32 UnitId, int32 ItemIndex, FText& OutError);

private:
    bool ResolveEditableUnit(APlayerController* Controller, int32 UnitId, ACombatRoundCoordinator*& OutCoordinator, AUnitBase*& OutUnit, FText& OutError) const;
    bool ApplyEquipment(APlayerController* Controller, int32 UnitId, FRunPartyMember Candidate, FText& OutError);

    UPROPERTY(Transient)
    TWeakObjectPtr<ACombatManager> Manager;

    UPROPERTY(Transient)
    TArray<FSoftObjectPath> SkillAssets;

    UPROPERTY(Transient)
    TMap<FSoftObjectPath, FText> SkillLabels;

    UPROPERTY(Transient)
    TMap<FSoftObjectPath, FGameplayTagContainer> SkillTags;

    UPROPERTY(Transient)
    TArray<FRunItemDefinition> EquipmentItems;

    UPROPERTY(Transient)
    TMap<int32, FRunPartyMember> EquipmentMembers;

    FGuid InitializedCombatId;
};
