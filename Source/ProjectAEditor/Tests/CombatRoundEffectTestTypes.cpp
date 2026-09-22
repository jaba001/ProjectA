#include "CombatRoundEffectTestTypes.h"

UCombatRoundDurationTestEffect::UCombatRoundDurationTestEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FScalableFloat(5.f);
}

UCombatRoundInfiniteTestEffect::UCombatRoundInfiniteTestEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
}
