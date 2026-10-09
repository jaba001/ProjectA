#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "DataAsset/CombatMeleeVfxCatalog.h"
#include "GAS/CombatGameplayTags.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sound/SoundBase.h"
#include "UObject/StrongObjectPtr.h"

namespace CombatMeleeVfxTests
{
    struct FFixture
    {
        TStrongObjectPtr<UCombatMeleeVfxCatalog> Catalog{NewObject<UCombatMeleeVfxCatalog>()};
        TStrongObjectPtr<UAnimMontage> AuthoredMontage{NewObject<UAnimMontage>()};
        TStrongObjectPtr<UAnimMontage> ResolvedMontage{NewObject<UAnimMontage>()};
        FCombatRoundSkill Skill;

        FFixture()
        {
            Catalog->Rules.SetNum(1);
            FCombatMeleeVfxRule& Rule = Catalog->Rules[0];
            Rule.CastMontages = {TSoftObjectPtr<UAnimMontage>(ResolvedMontage.Get())};
            Rule.FloatOverrides = {{TEXT("User.RotateSpeed"), -1.0f}};
            Skill.SkillId = TEXT("SyntheticMeleeVisual");
            Skill.Kind = ECombatRoundSkillKind::Melee;
            Skill.bUseEffectCollision = true;
            Skill.CastMontage = AuthoredMontage.Get();
            Skill.EffectTags.AddTag(ProjectACombatTags::Skill_Shape_Slash);
            Skill.EffectTags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
            Skill.EffectTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Attack.Close")));
            Skill.Vfx.Niagara = Rule.Niagara;
            Skill.Vfx.RelativeTransform = FTransform(FQuat::Identity, FVector(-120.0, 15.0, -8.0), FVector(1.25, 0.75, 1.5));
            Skill.Vfx.BoolParameters.Add(TEXT("User.AudioOn"), true);
            Skill.Vfx.FloatParameters.Add(TEXT("User.OtherValue"), 2.5f);
            Skill.Vfx.Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Test/MeleeSound.MeleeSound")));
            Skill.Vfx.SoundVolume = 0.75f;
            Skill.Vfx.SoundPitch = 1.25f;
            Skill.Vfx.SoundMaxDuration = 3.5f;
            Skill.Vfx.StartPositionParameter = TEXT("User.Start");
            Skill.Vfx.StartPositionSpace = ECombatVfxEndpointSpace::ComponentLocal;
            Skill.Vfx.StartPositionOffset = FVector(4.0, 5.0, 6.0);
            Skill.Vfx.EndPositionParameter = TEXT("User.End");
            Skill.Vfx.EndPositionSpace = ECombatVfxEndpointSpace::World;
            Skill.ImpactVfx = Skill.Vfx;
            Skill.EffectOffset = FVector(120.0, 0.0, 0.0);
            Skill.EffectTravel = FVector(250.0, 0.0, 0.0);
            Skill.EffectHitDelaySeconds = 0.1f;
            Skill.EffectDuration = 0.35f;
        }
    };

    bool SameVisual(const FCombatSkillVfx& Left, const FCombatSkillVfx& Right)
    {
        return FCombatSkillVfx::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMeleeVfxNativeScopeTest, "ProjectA.Combat.MeleeVfx.NativeEvidenceScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMeleeVfxNativeScopeTest::RunTest(const FString& Parameters)
{
    const UCombatMeleeVfxCatalog* Catalog = GetDefault<UCombatMeleeVfxCatalog>();
    TestEqual(TEXT("Only the three source effects with verified float controls are enabled"), Catalog->Rules.Num(), 3);
    const TMap<FString, float> Expected =
    {
        {TEXT("/Game/SlashHitVFX/NS/NS_Slash_Axe.NS_Slash_Axe"), -1.0f},
        {TEXT("/Game/SlashHitVFX/NS/NS_Slash_CurvedSword.NS_Slash_CurvedSword"), -1.5f},
        {TEXT("/Game/SlashHitVFX/NS/NS_Slash_Reaper.NS_Slash_Reaper"), -2.0f}
    };
    TSet<FSoftObjectPath> UniqueSources;
    FGameplayTagContainer Tags;
    Tags.AddTag(ProjectACombatTags::Skill_Shape_Slash);
    Tags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
    Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Attack.Close")));
    for (const FCombatMeleeVfxRule& Rule : Catalog->Rules)
    {
        const float* Speed = Expected.Find(Rule.Niagara.ToString());
        const float* Override = Rule.FloatOverrides.Find(TEXT("User.RotateSpeed"));
        TestTrue(TEXT("Each original source occurs once with exactly its verified rotation speed"), Rule.bEnabled && Speed && Override && Rule.FloatOverrides.Num() == 1 && *Speed == *Override && !UniqueSources.Contains(Rule.Niagara.ToSoftObjectPath()));
        UniqueSources.Add(Rule.Niagara.ToSoftObjectPath());
        TestTrue(TEXT("The native rule requires the existing slash, close-attack and damage tags"), !Rule.SkillQuery.IsEmpty() && Rule.SkillQuery.Matches(Tags));
        TestEqual(TEXT("Only the reviewed Manny and Skeleton Guard montages are eligible"), Rule.CastMontages.Num(), 2);
        FGameplayTagContainer MissingTag = Tags;
        MissingTag.RemoveTag(ProjectACombatTags::Skill_Effect_Damage);
        TestFalse(TEXT("Removing the effect classification disables the same native rule"), Rule.SkillQuery.Matches(MissingTag));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMeleeVfxCopyIsolationTest, "ProjectA.Combat.MeleeVfx.ResolvedMontageAndCopyIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMeleeVfxCopyIsolationTest::RunTest(const FString& Parameters)
{
    CombatMeleeVfxTests::FFixture Fixture;
    const FCombatRoundSkill Before = Fixture.Skill;
    const FCombatSkillVfx Visual = Fixture.Catalog->Resolve(Fixture.Skill, Fixture.ResolvedMontage.Get());
    const float* RotationSpeed = Visual.FloatParameters.Find(TEXT("User.RotateSpeed"));
    TestTrue(TEXT("The actual per-unit montage admits the cosmetic rule despite a different authored montage"), RotationSpeed && *RotationSpeed == -1.0f);
    FCombatSkillVfx Expected = Before.Vfx;
    Expected.FloatParameters.Add(TEXT("User.RotateSpeed"), -1.0f);
    TestTrue(TEXT("Only the specified float parameter changes; transform, endpoints, sound and asset references remain exact"), CombatMeleeVfxTests::SameVisual(Visual, Expected));
    TestTrue(TEXT("The entire skill, impact profile, collision geometry, GAS tags and timing stay unchanged"), FCombatRoundSkill::StaticStruct()->CompareScriptStruct(&Fixture.Skill, &Before, 0));
    TestTrue(TEXT("Matching the authored rather than the resolved montage would correctly leave this unit unchanged"), CombatMeleeVfxTests::SameVisual(Fixture.Catalog->Resolve(Fixture.Skill, Fixture.AuthoredMontage.Get()), Before.Vfx));
    Fixture.Skill.Vfx = Visual;
    TestTrue(TEXT("A previously corrected visual copy cannot receive another correction"), CombatMeleeVfxTests::SameVisual(Fixture.Catalog->Resolve(Fixture.Skill, Fixture.ResolvedMontage.Get()), Visual));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMeleeVfxGuardTest, "ProjectA.Combat.MeleeVfx.UnmatchedAndAuthoredOverridesPreserved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMeleeVfxGuardTest::RunTest(const FString& Parameters)
{
    CombatMeleeVfxTests::FFixture Fixture;
    const FCombatRoundSkill Original = Fixture.Skill;
    const FCombatMeleeVfxRule OriginalRule = Fixture.Catalog->Rules[0];
    const auto Unchanged = [this, &Fixture](const TCHAR* Label)
    {
        TestTrue(Label, CombatMeleeVfxTests::SameVisual(Fixture.Catalog->Resolve(Fixture.Skill, Fixture.ResolvedMontage.Get()), Fixture.Skill.Vfx));
    };
    Fixture.Catalog->Rules[0].bEnabled = false;
    Unchanged(TEXT("Disabled evidence does not change presentation"));
    Fixture.Catalog->Rules[0] = OriginalRule;
    Fixture.Catalog->Rules[0].SkillQuery = FGameplayTagQuery();
    Unchanged(TEXT("An empty tag query cannot silently match all content"));
    Fixture.Catalog->Rules[0] = OriginalRule;
    for (FGameplayTag Tag : {ProjectACombatTags::Skill_Shape_Slash.GetTag(), ProjectACombatTags::Skill_Effect_Damage.GetTag(), FGameplayTag::RequestGameplayTag(TEXT("Attack.Close"))})
    {
        Fixture.Skill = Original;
        Fixture.Skill.EffectTags.RemoveTag(Tag);
        Unchanged(TEXT("Every required content tag participates in selection"));
    }
    Fixture.Skill = Original;
    Fixture.Skill.Kind = ECombatRoundSkillKind::Projectile;
    Unchanged(TEXT("Projectile execution does not inherit melee presentation rules"));
    Fixture.Skill = Original;
    Fixture.Skill.bUseEffectCollision = false;
    Unchanged(TEXT("Weapon-trace or legacy direct-hit execution remains unchanged"));
    Fixture.Skill = Original;
    Fixture.Skill.Vfx.Niagara = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/SlashHitVFX/NS/NS_Slash_Katana.NS_Slash_Katana")));
    Unchanged(TEXT("An unverified source effect is not inferred from its slash tag or name"));
    Fixture.Skill = Original;
    Fixture.Skill.Vfx.RelativeTransform.SetRotation(FRotator(0.0, 5.0, 0.0).Quaternion());
    Unchanged(TEXT("Custom authored visual rotation is never overridden"));
    Fixture.Skill = Original;
    Fixture.Skill.Vfx.FloatParameters.Add(TEXT("User.RotateSpeed"), 0.35f);
    Unchanged(TEXT("An existing positive custom float override takes precedence"));
    Fixture.Skill = Original;
    Fixture.Skill.Vfx.BoolParameters.Add(TEXT("User.RotateSpeed"), true);
    Unchanged(TEXT("Conflicting parameter types cannot be overwritten"));
    Fixture.Skill = Original;
    Fixture.Catalog->Rules.Add(OriginalRule);
    Unchanged(TEXT("Ambiguous matching rules preserve the original without order-dependent accumulation"));
    Fixture.Catalog->Rules.SetNum(1);
    Fixture.Catalog->Rules[0].FloatOverrides.Add(NAME_None, 1.0f);
    Unchanged(TEXT("Invalid additional metadata rejects the whole cosmetic correction atomically"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMeleeVfxReplicationTest, "ProjectA.Combat.MeleeVfx.ExistingVisualNetSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatMeleeVfxReplicationTest::RunTest(const FString& Parameters)
{
    CombatMeleeVfxTests::FFixture Fixture;
    FCombatSkillVfx Visual = Fixture.Catalog->Resolve(Fixture.Skill, Fixture.ResolvedMontage.Get());
    TArray<uint8> Bytes;
    bool bWritten = false;
    {
        FMemoryWriter Writer(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        TestTrue(TEXT("The existing visual NetSerialize handles the corrected copy"), Visual.NetSerialize(Archive, nullptr, bWritten));
        TestTrue(TEXT("Writing existing cosmetic fields succeeds without a new network schema"), bWritten && !Archive.IsError());
    }
    FCombatSkillVfx Restored;
    bool bRead = false;
    {
        FMemoryReader Reader(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Reader, false);
        TestTrue(TEXT("The existing visual NetSerialize reads the corrected copy"), Restored.NetSerialize(Archive, nullptr, bRead));
        TestTrue(TEXT("Reading consumes the full existing cosmetic payload"), bRead && !Archive.IsError() && Reader.AtEnd());
    }
    TestTrue(TEXT("Network serialization preserves the override and all original presentation fields"), CombatMeleeVfxTests::SameVisual(Visual, Restored));
    return true;
}

#endif
