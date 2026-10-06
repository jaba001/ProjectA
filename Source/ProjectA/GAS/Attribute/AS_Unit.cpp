#include "GAS/Attribute/AS_Unit.h"
#include "GameplayEffectExtension.h"
#include "Unit/UnitBase.h"
#include "Net/UnrealNetwork.h"

UAS_Unit::UAS_Unit()
{
    InitShield(0.0f);
    InitSpeed(10.0f);
}

void UAS_Unit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, HP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, MaxHP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, Shield, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_Unit, Speed, COND_None, REPNOTIFY_Always);
}

void UAS_Unit::OnRep_HP(const FGameplayAttributeData& PreviousHP)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, HP, PreviousHP);
}

void UAS_Unit::OnRep_MaxHP(const FGameplayAttributeData& PreviousMaxHP)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, MaxHP, PreviousMaxHP);
}

void UAS_Unit::OnRep_Shield(const FGameplayAttributeData& PreviousShield)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, Shield, PreviousShield);
}

void UAS_Unit::OnRep_Speed(const FGameplayAttributeData& PreviousSpeed)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Unit, Speed, PreviousSpeed);
}

void UAS_Unit::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (Attribute == GetShieldAttribute()) NewValue = FMath::IsFinite(NewValue) ? FMath::Max(0.0f, NewValue) : 0.0f;
    if (Attribute == GetHPAttribute()) NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxHP())) : 0.0f;
}

bool UAS_Unit::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
    if (!Super::PreGameplayEffectExecute(Data) || !GetOwningActor() || !GetOwningActor()->HasAuthority() || !FMath::IsFinite(Data.EvaluatedData.Magnitude)) return false;
    if (Data.EvaluatedData.Attribute != GetHPAttribute() && Data.EvaluatedData.Attribute != GetShieldAttribute()) return true;
    const AUnitBase* Unit = Data.Target.AbilityActorInfo.IsValid() ? Cast<AUnitBase>(Data.Target.AbilityActorInfo->AvatarActor.Get()) : nullptr;
    if (Unit && !Unit->IsUnitAlive()) return false;
    if (Data.EvaluatedData.Attribute == GetHPAttribute() && Data.EvaluatedData.ModifierOp == EGameplayModOp::Additive)
    {
        if (Data.EvaluatedData.Magnitude > 0.0f && GetHP() <= 0.0f) return false;
        if (Data.EvaluatedData.Magnitude < 0.0f)
        {
            // Keep legacy negative Data.Damage values and consume the shield before reducing HP.
            // 기존 Data.Damage 음수 값을 유지하며 HP 차감 전에 보호막을 소비합니다.
            const float Absorbed = FMath::Min(FMath::Max(0.0f, GetShield()), -Data.EvaluatedData.Magnitude);
            if (Absorbed > 0.0f) SetShield(GetShield() - Absorbed);
            Data.EvaluatedData.Magnitude += Absorbed;
        }
    }
    return true;
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
        SetHP(FMath::Clamp(GetHP(), 0.0f, FMath::Max(0.0f, GetMaxHP())));
        // Resolve death only after shield absorption and final HP clamping.
        // 보호막 흡수와 최종 HP 보정이 끝난 뒤 사망을 판정합니다.
        if (GetHP() <= 0.0f)
        {
            SetShield(0.0f);
            AActor* OwnerActor = nullptr;

            if (Data.Target.AbilityActorInfo.IsValid())
            {
                OwnerActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
            }

            AUnitBase* OwnerUnit = Cast<AUnitBase>(OwnerActor);

            if (OwnerUnit)
            {
                OwnerUnit->Die();
            }
        }
    }
    else if (Data.EvaluatedData.Attribute == GetMaxHPAttribute())
    {
        SetHP(FMath::Clamp(GetHP(), 0.0f, FMath::Max(0.0f, GetMaxHP())));
    }
    else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
    {
        SetShield(FMath::Max(0.0f, GetShield()));
    }
}
