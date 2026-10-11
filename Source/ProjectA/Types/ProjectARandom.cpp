#include "Types/ProjectARandom.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
#if WITH_EDITOR
    bool bBalanceSimulation = false;
    FRandomStream SimulationSeeds;
    FRandomStream SimulationIdentity;
#endif
}

bool ProjectARandom::IsBalanceSimulation()
{
#if WITH_EDITOR
    return bBalanceSimulation;
#else
    return false;
#endif
}

int32 ProjectARandom::NewSeed()
{
#if WITH_EDITOR
    if (bBalanceSimulation) return static_cast<int32>(SimulationSeeds.GetUnsignedInt());
#endif
    const FGuid Guid = FGuid::NewGuid();
    return static_cast<int32>(HashCombineFast(HashCombineFast(Guid.A, Guid.B), HashCombineFast(Guid.C, Guid.D)));
}

FGuid ProjectARandom::NewRunGuid()
{
#if WITH_EDITOR
    if (bBalanceSimulation)
    {
        const uint32 A = SimulationIdentity.GetUnsignedInt();
        const uint32 B = SimulationIdentity.GetUnsignedInt();
        const uint32 C = SimulationIdentity.GetUnsignedInt();
        const uint32 D = SimulationIdentity.GetUnsignedInt();
        return FGuid(A, B, C, D | 1u);
    }
#endif
    return FGuid::NewGuid();
}

#if WITH_EDITOR
ProjectARandom::FScopedBalanceSimulation::FScopedBalanceSimulation(int32 Seed)
{
    check(IsInGameThread() && IsRunningCommandlet() && FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && !bBalanceSimulation);
    SimulationSeeds.Initialize(Seed);
    SimulationIdentity.Initialize(static_cast<int32>(static_cast<uint32>(Seed) ^ 0x4944454eu));
    bBalanceSimulation = true;
}

ProjectARandom::FScopedBalanceSimulation::~FScopedBalanceSimulation()
{
    bBalanceSimulation = false;
}
#endif
