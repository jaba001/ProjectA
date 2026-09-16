#include "GAS/Attribute/AS_Unit.h"
#include "GameplayEffectExtension.h"
#include "Unit/UnitBase.h"
#include "Net/UnrealNetwork.h"

UAS_Unit::UAS_Unit()
{
    InitStrength(10.0f);
    InitDexterity(10.0f);
    InitIntelligence(10.0f);
}

void UAS_Unit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, HP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, MaxHP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, Strength, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, Dexterity, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, Intelligence, COND_None, REPNOTIFY_Always);
}

void UAS_Unit::OnRep_HP(const FGameplayAttributeData& PreviousHP)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, HP, PreviousHP);
}

void UAS_Unit::OnRep_MaxHP(const FGameplayAttributeData& PreviousMaxHP)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, MaxHP, PreviousMaxHP);
}

void UAS_Unit::OnRep_Strength(const FGameplayAttributeData& PreviousStrength)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, Strength, PreviousStrength);
}

void UAS_Unit::OnRep_Dexterity(const FGameplayAttributeData& PreviousDexterity)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, Dexterity, PreviousDexterity);
}

void UAS_Unit::OnRep_Intelligence(const FGameplayAttributeData& PreviousIntelligence)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, Intelligence, PreviousIntelligence);
}

void UAS_Unit::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // Only the server clamps gameplay state and resolves lethal effects.
    // 서버만 게임 상태를 보정하고 치명적 이펙트를 판정합니다.
    if (!GetOwningActor() || !GetOwningActor()->HasAuthority())
    {
        return;
    }

    if (Data.EvaluatedData.Attribute == GetHPAttribute())
    {
        // Clamp HP to the lower bound
        if (GetHP() < 0.0f)
        {
            SetHP(0.0f);
        }

        // Clamp HP to the upper bound
        if (GetHP() > GetMaxHP())
        {
            SetHP(GetMaxHP());
        }

        //UE_LOG(LogTemp, Log, TEXT("[AS_Unit] HP Changed | NewHP=%.1f / MaxHP=%.1f"), GetHP(), GetMaxHP());

        // Trigger unit death when HP reaches zero or below
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
