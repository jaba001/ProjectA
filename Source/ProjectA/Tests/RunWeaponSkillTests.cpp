#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "GAS/CombatGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ValidationRarityLow, "Validation.WeaponSkills.Rarity.Low");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ValidationRarityHigh, "Validation.WeaponSkills.Rarity.High");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ValidationHighPool, "Validation.WeaponSkills.Pool.High");

    struct FWeaponSkillFixture
    {
        TStrongObjectPtr<UPackage> Package;
        TArray<TStrongObjectPtr<USkillDefinitionDataAsset>> Skills;
        FRunWeaponSkillRulesState Rules;

        FWeaponSkillFixture()
        {
            const FString PackageName = TEXT("/Game/User_JeHoon/Validation/T12/WeaponSkills_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
            Package.Reset(CreatePackage(*PackageName));
            Package->SetFlags(RF_Transient);
            Rules.SchemaVersion = 1;
            Rules.SkillCount = 1;
            Rules.WeaponQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag());
            AddSkill(TEXT("Melee"), TEXT("Item.Weapon.Dagger"), ProjectACombatTags::Skill_Shape_Slash);
            Rules.Candidates[0].SelectionTags.AddTag(TAG_ValidationHighPool);
            AddSkill(TEXT("Arrow"), TEXT("Item.Weapon.Bow"), ProjectACombatTags::Skill_Shape_Projectile);
            FRunWeaponRarityRule& Low = Rules.Rarities.AddDefaulted_GetRef();
            Low.RarityTag = TAG_ValidationRarityLow;
            Low.DisplayName = FText::FromString(TEXT("Validation Low"));
            Low.BaseWeight = 1.0f;
            FRunWeaponRarityRule& High = Rules.Rarities.AddDefaulted_GetRef();
            High.RarityTag = TAG_ValidationRarityHigh;
            High.DisplayName = FText::FromString(TEXT("Validation High"));
            High.Color = FLinearColor::Green;
            High.BaseWeight = 1.0f;
            High.SkillQuery = FGameplayTagQuery::MakeQuery_MatchTag(TAG_ValidationHighPool);
        }

        void AddSkill(FName Name, FName ItemTag, FGameplayTag Shape)
        {
            USkillDefinitionDataAsset* Skill = NewObject<USkillDefinitionDataAsset>(Package.Get(), Name, RF_Transient);
            Skill->bUseRoundDefinition = true;
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Effect_Damage);
            Skill->RoundDefinition.EffectTags.AddTag(ProjectACombatTags::Skill_Element_Physical);
            Skill->RoundDefinition.EffectTags.AddTag(Shape);
            Skill->RoundDefinition.Kind = Shape == ProjectACombatTags::Skill_Shape_Projectile ? ECombatRoundSkillKind::Projectile : ECombatRoundSkillKind::Melee;
            Skill->RoundDefinition.Approach = ECombatRoundApproach::None;
            Skills.Emplace(Skill);
            FRunWeaponSkillCandidate& Candidate = Rules.Candidates.AddDefaulted_GetRef();
            Candidate.Skill = FSoftObjectPath(Skill);
            Candidate.Tags = Skill->RoundDefinition.EffectTags;
            Candidate.AllowedItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(ItemTag));
            Candidate.BaseWeight = 1.0f;
        }

        FRunItemDefinition Item(FName ItemTag, int32 Index = 0) const
        {
            FRunItemDefinition Result;
            Result.Asset = FSoftObjectPath(FString::Printf(TEXT("/Game/User_JeHoon/Validation/T12/Weapon_%d.Weapon_%d"), Index, Index));
            Result.DisplayName = FText::FromString(FString::Printf(TEXT("Validation Weapon %d"), Index));
            Result.Tags.AddTag(FGameplayTag::RequestGameplayTag(ItemTag));
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunWeaponSkillCompatibilityTest, "ProjectA.Run.WeaponSkills.TagCompatibilityAndFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunWeaponSkillCompatibilityTest::RunTest(const FString& Parameters)
{
    FWeaponSkillFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Injected tag-based generation rules validate"), RunWeaponSkillRules::Validate(Fixture.Rules, Error))) return false;
    const FRunItemDefinition Dagger = Fixture.Item(TEXT("Item.Weapon.Dagger"));
    const FRunItemDefinition Bow = Fixture.Item(TEXT("Item.Weapon.Bow"));
    FRandomStream Random(731);
    FRunItemDefinition DaggerCopy;
    FRunItemDefinition BowCopy;
    if (!TestTrue(TEXT("A dagger receives its compatible melee skill"), RunWeaponSkillRules::Generate(Dagger, Fixture.Rules, Random, DaggerCopy, Error) && DaggerCopy.GrantedSkills == TArray<FSoftObjectPath>{Fixture.Rules.Candidates[0].Skill})) return false;
    if (!TestTrue(TEXT("A bow receives its arrow skill and excludes a grade with no compatible candidates"), RunWeaponSkillRules::Generate(Bow, Fixture.Rules, Random, BowCopy, Error) && BowCopy.GrantedSkills == TArray<FSoftObjectPath>{Fixture.Rules.Candidates[1].Skill} && BowCopy.RarityTag == TAG_ValidationRarityLow)) return false;
    TestFalse(TEXT("Selection metadata never rewrites the original GAS execution tags"), Fixture.Rules.Candidates[0].Tags.HasTag(TAG_ValidationHighPool));
    FRunWeaponSkillRulesState HighOnly = Fixture.Rules;
    HighOnly.Rarities[0].BaseWeight = 0.0f;
    FRunItemDefinition HighCopy;
    TestTrue(TEXT("Separate selection tags participate in the actual grade candidate query"), RunWeaponSkillRules::Generate(Dagger, HighOnly, Random, HighCopy, Error) && HighCopy.RarityTag == TAG_ValidationRarityHigh);
    FRunItemDefinition Tampered = BowCopy;
    Tampered.GrantedSkills = DaggerCopy.GrantedSkills;
    TestFalse(TEXT("Stored bow copies reject a melee skill even when it belongs to the frozen catalog"), RunWeaponSkillRules::ValidateGeneratedCopy(Tampered, Fixture.Rules, Error));
    Tampered = DaggerCopy;
    const FSoftObjectPath Duplicate = Tampered.GrantedSkills[0];
    Tampered.GrantedSkills.Add(Duplicate);
    TestFalse(TEXT("A stored copy never gains duplicate skills"), RunWeaponSkillRules::ValidateGeneratedCopy(Tampered, Fixture.Rules, Error));
    const FRunItemDefinition Unsupported = Fixture.Item(TEXT("Item.Weapon.Axe"));
    TestFalse(TEXT("A weapon with no matching tag eligibility is excluded before item selection"), RunWeaponSkillRules::CanGenerate(Unsupported, Fixture.Rules));
    FRunItemDefinition Preserved = BowCopy;
    TestFalse(TEXT("Missing compatible candidates do not fill a weapon with unrelated skills"), RunWeaponSkillRules::Generate(Unsupported, Fixture.Rules, Random, Preserved, Error));
    TestTrue(TEXT("Failed generation preserves the complete prior output copy"), RunItemShopCatalog::IsSameDefinition(Preserved, BowCopy));
    TestFalse(TEXT("An existing generated copy cannot be rerolled through the creation API"), RunWeaponSkillRules::Generate(DaggerCopy, Fixture.Rules, Random, Preserved, Error));
    const FRunItemDefinition NonWeapon = Fixture.Item(TEXT("Item.Consumable.Healing"));
    FRunItemDefinition NonWeaponCopy;
    TestTrue(TEXT("An item outside WeaponQuery may receive a grade but never weapon skills"), RunWeaponSkillRules::Generate(NonWeapon, Fixture.Rules, Random, NonWeaponCopy, Error) && NonWeaponCopy.ItemInstanceId.IsValid() && NonWeaponCopy.RarityTag.IsValid() && NonWeaponCopy.GrantedSkills.IsEmpty());
    FRunWeaponSkillRulesState Invalid = Fixture.Rules;
    Invalid.Candidates[0].Tags.AddTag(TAG_ValidationHighPool);
    TestFalse(TEXT("Frozen GAS tags must still exactly match actual skill execution tags"), RunWeaponSkillRules::Validate(Invalid, Error));
    Invalid = Fixture.Rules;
    Invalid.Candidates[0].AllowedItemQuery = FGameplayTagQuery();
    TestFalse(TEXT("An unspecified item compatibility query cannot silently allow every weapon"), RunWeaponSkillRules::Validate(Invalid, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunWeaponSkillFrozenCopyTest, "ProjectA.Run.WeaponSkills.FrozenShopCopiesAndSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunWeaponSkillFrozenCopyTest::RunTest(const FString& Parameters)
{
    FWeaponSkillFixture Fixture;
    FRunItemShopState Shop;
    Shop.SchemaVersion = 1;
    for (int32 Index = 0; Index < 5; ++Index) Shop.Catalog.Add(Fixture.Item(TEXT("Item.Weapon.Dagger"), Index));
    FText Error;
    if (!TestTrue(TEXT("Shop display creates fixed individual copies before any purchase"), RunItemShopCatalog::Roll(Shop, false, Fixture.Rules.WeaponQuery, Error, &Fixture.Rules))) return false;
    TSet<FGuid> CopyIds;
    for (const FRunItemShopOffer& Offer : Shop.Offers)
    {
        const FRunItemDefinition* Base = Shop.Catalog.FindByPredicate([&Offer](const FRunItemDefinition& Item) { return Item.Asset == Offer.Item.Asset; });
        if (!TestTrue(TEXT("Each displayed copy retains its frozen base definition and generated identity"), Base && Offer.Item.GenerationVersion == 1 && Offer.Item.ItemInstanceId.IsValid() && !CopyIds.Contains(Offer.Item.ItemInstanceId) && RunItemShopCatalog::IsSameBaseDefinition(*Base, Offer.Item))) return false;
        CopyIds.Add(Offer.Item.ItemInstanceId);
    }
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->WeaponSkillRules = Fixture.Rules;
    Save->ItemShopState = Shop;
    FRunPartyMember& Buyer = Save->Party.AddDefaulted_GetRef();
    Buyer.Items.Add(Shop.Offers[0].Item);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Fixed generated offers and the purchased copy serialize through Unreal SaveGame"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("The generated copy save deserializes"), Restored.Get())) return false;
    TestTrue(TEXT("Loaded generation queries and candidate metadata remain valid"), RunWeaponSkillRules::Validate(Restored->WeaponSkillRules, Error));
    if (!TestTrue(TEXT("Continue validates the saved displayed results without regenerating them"), RunItemShopCatalog::Validate(Restored->ItemShopState, Error, &Restored->WeaponSkillRules))) return false;
    TestTrue(TEXT("Purchase preserves the displayed copy identity grade and ordered skills"), Restored->Party.Num() == 1 && Restored->Party[0].Items.Num() == 1 && RunItemShopCatalog::IsSameDefinition(Restored->Party[0].Items[0], Shop.Offers[0].Item));
    for (int32 Index = 0; Index < Shop.Offers.Num(); ++Index)
    {
        TestTrue(TEXT("Every saved offer preserves its original per-copy result"), RunItemShopCatalog::IsSameDefinition(Restored->ItemShopState.Offers[Index].Item, Shop.Offers[Index].Item));
        TestFalse(TEXT("Restoring legacy random grades does not inject current authored grade metadata"), Restored->ItemShopState.Offers[Index].Item.CatalogRarityTag.IsValid());
    }
    FRunWeaponSkillRulesState Insufficient = Fixture.Rules;
    Insufficient.SkillCount = 2;
    FRunItemShopState Fresh;
    Fresh.SchemaVersion = 1;
    Fresh.Catalog = Shop.Catalog;
    const FRunItemShopState Before = Fresh;
    TestFalse(TEXT("A reroll cannot silently duplicate or replace insufficient skill candidates"), RunItemShopCatalog::Roll(Fresh, false, Insufficient.WeaponQuery, Error, &Insufficient));
    TestTrue(TEXT("Failed item generation leaves stock revision and catalog unchanged"), FRunItemShopState::StaticStruct()->CompareScriptStruct(&Before, &Fresh, 0));
    FRunItemDefinition DifferentCopy = Shop.Offers[0].Item;
    DifferentCopy.ItemInstanceId = FGuid::NewGuid();
    TestTrue(TEXT("Catalog identity is independent of per-copy generation identity"), RunItemShopCatalog::IsSameBaseDefinition(DifferentCopy, Shop.Offers[0].Item));
    TestFalse(TEXT("Full copy comparison still rejects a changed generation identity"), RunItemShopCatalog::IsSameDefinition(DifferentCopy, Shop.Offers[0].Item));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunWeaponSkillAuthoredRarityTest, "ProjectA.Run.WeaponSkills.AuthoredRarityAndFrozenSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunWeaponSkillAuthoredRarityTest::RunTest(const FString& Parameters)
{
    FWeaponSkillFixture Fixture;
    FRunItemDefinition Dagger = Fixture.Item(TEXT("Item.Weapon.Dagger"));
    Dagger.CatalogRarityTag = TAG_ValidationRarityHigh;
    FRandomStream Random(371);
    FText Error;
    FRunItemDefinition Copy;
    if (!TestTrue(TEXT("A fixed high grade generates only from its compatible skill pool"), RunWeaponSkillRules::CanGenerate(Dagger, Fixture.Rules) && RunWeaponSkillRules::Generate(Dagger, Fixture.Rules, Random, Copy, Error))) return false;
    TestTrue(TEXT("Generated copies retain their authored base grade and compatible skill"), Copy.CatalogRarityTag == TAG_ValidationRarityHigh && Copy.RarityTag == TAG_ValidationRarityHigh && Copy.GrantedSkills == TArray<FSoftObjectPath>{Fixture.Rules.Candidates[0].Skill} && Copy.Price == Dagger.Price);
    FRunItemDefinition Altered = Copy;
    Altered.RarityTag = TAG_ValidationRarityLow;
    TestFalse(TEXT("A generated copy cannot substitute a different grade"), RunWeaponSkillRules::ValidateGeneratedCopy(Altered, Fixture.Rules, Error));
    TestFalse(TEXT("Basic item validation also rejects a changed fixed grade"), RunItemShopCatalog::ValidateItem(Altered));
    Altered = Copy;
    Altered.CatalogRarityTag = FGameplayTag();
    TestFalse(TEXT("Removing authored metadata cannot match the saved base catalog"), RunItemShopCatalog::IsSameBaseDefinition(Altered, Dagger));

    FRunItemDefinition Bow = Fixture.Item(TEXT("Item.Weapon.Bow"));
    Bow.CatalogRarityTag = TAG_ValidationRarityHigh;
    const FRunItemDefinition BeforeFailure = Copy;
    const int32 BeforeSeed = Random.GetCurrentSeed();
    TestFalse(TEXT("An incompatible fixed grade is excluded without downgrading the weapon"), RunWeaponSkillRules::CanGenerate(Bow, Fixture.Rules));
    TestFalse(TEXT("An incompatible fixed grade cannot use the otherwise valid low bow pool"), RunWeaponSkillRules::Generate(Bow, Fixture.Rules, Random, Copy, Error));
    TestTrue(TEXT("Rejected fixed grade generation preserves the complete old copy"), RunItemShopCatalog::IsSameDefinition(Copy, BeforeFailure));
    TestEqual(TEXT("Rejected fixed grade generation preserves the random stream"), Random.GetCurrentSeed(), BeforeSeed);
    Bow.CatalogRarityTag = FGameplayTag();
    TestTrue(TEXT("Unclassified legacy bows retain their original compatible random-grade policy"), RunWeaponSkillRules::Generate(Bow, Fixture.Rules, Random, Copy, Error) && Copy.RarityTag == TAG_ValidationRarityLow && !Copy.CatalogRarityTag.IsValid());
    FRunItemDefinition Unknown = Dagger;
    Unknown.CatalogRarityTag = TAG_ValidationHighPool;
    TestFalse(TEXT("A valid tag outside the frozen rarity rules cannot become a grade"), RunWeaponSkillRules::CanGenerate(Unknown, Fixture.Rules) || RunWeaponSkillRules::Generate(Unknown, Fixture.Rules, Random, Copy, Error));

    FRunItemDefinition NonWeapon = Fixture.Item(TEXT("Item.Consumable.Healing"));
    NonWeapon.CatalogRarityTag = TAG_ValidationRarityHigh;
    const int32 BeforeNonWeaponSeed = Random.GetCurrentSeed();
    TestTrue(TEXT("Fixed nonweapon grades receive no weapon skills"), RunWeaponSkillRules::Generate(NonWeapon, Fixture.Rules, Random, Copy, Error) && Copy.RarityTag == TAG_ValidationRarityHigh && Copy.GrantedSkills.IsEmpty());
    TestEqual(TEXT("Fixed grade assignment itself never consumes a random draw"), Random.GetCurrentSeed(), BeforeNonWeaponSeed);

    FRunItemShopState Shop;
    Shop.SchemaVersion = 1;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        FRunItemDefinition Base = Fixture.Item(TEXT("Item.Weapon.Dagger"), Index);
        Base.CatalogRarityTag = Index % 2 == 0 ? TAG_ValidationRarityLow : TAG_ValidationRarityHigh;
        Shop.Catalog.Add(Base);
    }
    if (!TestTrue(TEXT("Shop generation uses the grade frozen in each catalog entry"), RunItemShopCatalog::Roll(Shop, false, Fixture.Rules.WeaponQuery, Error, &Fixture.Rules))) return false;
    for (const FRunItemShopOffer& Offer : Shop.Offers) TestEqual(TEXT("Each offer retains the authored color independently of item sampling"), Offer.Item.RarityTag, Offer.Item.CatalogRarityTag);
    TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
    Save->WeaponSkillRules = Fixture.Rules;
    Save->ItemShopState = Shop;
    Save->ItemShopState.Offers[0].bSold = true;
    Save->Party.AddDefaulted_GetRef().Items.Add(Shop.Offers[0].Item);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Authored grades and purchased copies serialize through Unreal SaveGame"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<URunSaveGame> Restored(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Authored-grade item data deserializes"), Restored.Get())) return false;
    if (!TestTrue(TEXT("Continue validates saved grades and skill choices without consulting the current CSV"), RunItemShopCatalog::Validate(Restored->ItemShopState, Error, &Restored->WeaponSkillRules))) return false;
    TestTrue(TEXT("Purchased copy identity grade skills and sold stock remain frozen"), Restored->Party.Num() == 1 && Restored->Party[0].Items.Num() == 1 && Restored->ItemShopState.Offers[0].bSold && RunItemShopCatalog::IsSameDefinition(Restored->Party[0].Items[0], Shop.Offers[0].Item));
    for (int32 Index = 0; Index < Shop.Offers.Num(); ++Index) TestTrue(TEXT("Saved offers preserve their complete authored and generated metadata"), RunItemShopCatalog::IsSameDefinition(Restored->ItemShopState.Offers[Index].Item, Shop.Offers[Index].Item));
    return true;
}

#endif
