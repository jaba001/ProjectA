#include "GAS/Attribute/AS_Unit.h"
#include "GameplayEffectExtension.h"
#include "Unit/UnitBase.h"

void UAS_Unit::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHPAttribute())
    {
        // HP 하한 보정
        if (GetHP() < 0.0f)
        {
            SetHP(0.0f);
        }

        // HP 상한 보정
        if (GetHP() > GetMaxHP())
        {
            SetHP(GetMaxHP());
        }

        //UE_LOG(LogTemp, Log, TEXT("[AS_Unit] HP Changed | NewHP=%.1f / MaxHP=%.1f"), GetHP(), GetMaxHP());

        // HP가 0 이하면 소유 유닛 사망 처리
        if (GetHP() <= 0.0f)
        {
            AActor* OwnerActor = nullptr;

            if (Data.Target.AbilityActorInfo.IsValid())
            {
                OwnerActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
            }

            AUnitBase* OwnerUnit = Cast<AUnitBase>(OwnerActor);

            if (OwnerUnit)
            {
                //UE_LOG(LogTemp, Log, TEXT("[AS_Unit] Die Triggered | Unit=%s"), *OwnerUnit->GetName());
                OwnerUnit->Die();
            }
        }
    }
}