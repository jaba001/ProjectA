#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Dom/JsonObject.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

namespace ExistingSaveCopyReview
{
    FString AbsolutePath(const FString& Path)
    {
        FString Result = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeDirectoryName(Result);
        return Result;
    }

    bool IsSHA256Literal(const FString& Value)
    {
        if (Value.Len() != 64) return false;
        for (const TCHAR Character : Value)
        {
            if (!FChar::IsHexDigit(Character)) return false;
        }
        return true;
    }

    TSharedPtr<FJsonObject> SkillOffer(const FRunSkillShopOffer& Offer)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("offer_id"), Offer.OfferId.ToString());
        Result->SetStringField(TEXT("asset"), Offer.Skill.ToString());
        Result->SetStringField(TEXT("stored_display_name"), Offer.DisplayName.ToString());
        Result->SetNumberField(TEXT("price"), Offer.Price);
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExistingSaveCopyReviewTest, "ProjectA.TodoReview.ExistingSaveCopy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExistingSaveCopyReviewTest::RunTest(const FString& Parameters)
{
    using namespace ExistingSaveCopyReview;
    const FString Root = AbsolutePath(FPaths::ProjectDir());
    if (!TestTrue(TEXT("The externally authorized ProjectA Editor uses its own project-local Saved directory."), GIsEditor && FPaths::GetCleanFilename(FPaths::GetProjectFilePath()) == TEXT("ProjectA.uproject") && AbsolutePath(FPaths::ProjectSavedDir()).Equals(Root / TEXT("Saved"), ESearchCase::IgnoreCase))) return false;
    FString Slot = Parameters.TrimStartAndEnd();
    if (Slot.IsEmpty()) FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveCopySlot="), Slot);
    const FString Prefix = TEXT("ProjectA_Automation_ExistingSave_");
    FGuid Guid;
    if (!TestTrue(TEXT("Only an explicitly supplied disposable UUID slot may be loaded."), Slot.StartsWith(Prefix) && Slot.Len() == Prefix.Len() + 32 && FGuid::ParseExact(Slot.Right(32), EGuidFormats::Digits, Guid) && Guid.IsValid() && Slot == Prefix + Guid.ToString(EGuidFormats::Digits).ToLower())) return false;
    int32 ExpectedBytes = 0;
    FString ExpectedSHA256;
    if (!TestTrue(TEXT("The external driver explicitly supplies the verified source size and SHA256."), FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveCopyBytes="), ExpectedBytes) && ExpectedBytes > 0 && FParse::Value(FCommandLine::Get(), TEXT("ProjectAExistingSaveCopySHA256="), ExpectedSHA256) && IsSHA256Literal(ExpectedSHA256))) return false;
    const FString SourcePath = Root / TEXT("Saved/SaveGames/ProjectA_Run.sav");
    const FString ClonePath = Root / TEXT("Saved/SaveGames") / (Slot + TEXT(".sav"));
    TArray<uint8> SourceBefore;
    TArray<uint8> CloneBefore;
    if (!TestTrue(TEXT("The explicitly verified original size and byte-identical owned clone match."), FFileHelper::LoadFileToArray(SourceBefore, *SourcePath) && SourceBefore.Num() == ExpectedBytes && FFileHelper::LoadFileToArray(CloneBefore, *ClonePath) && CloneBefore == SourceBefore)) return false;
    FString BaselineText;
    TSharedPtr<FJsonObject> Baseline;
    FString BaselineSHA256;
    if (!TestTrue(TEXT("The external SHA256 baseline identifies the explicitly verified original."), FFileHelper::LoadFileToString(BaselineText, *(Root / TEXT("Saved/Automation/TodoReview/BeforeUserState.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BaselineText), Baseline) && Baseline.IsValid() && Baseline->TryGetStringField(TEXT("Saved/SaveGames/ProjectA_Run.sav"), BaselineSHA256) && IsSHA256Literal(BaselineSHA256) && BaselineSHA256.Equals(ExpectedSHA256, ESearchCase::IgnoreCase))) return false;
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("scope"), TEXT("Editor raw deserialization of a byte-identical UUID copy, public-field observation and read-only authored skill profile resolution. Run checkpoint migration and Continue are not invoked. External driver verifies original baseline SHA256; C++ verifies exact bytes and SHA1."));
    Report->SetStringField(TEXT("clone_slot"), Slot);
    Report->SetStringField(TEXT("external_baseline_sha256"), BaselineSHA256);
    Report->SetStringField(TEXT("cpp_original_sha1_before"), FSHA1::HashBuffer(SourceBefore.GetData(), SourceBefore.Num()).ToString());
    Report->SetNumberField(TEXT("original_bytes"), SourceBefore.Num());
    Report->SetBoolField(TEXT("production_migration_tested"), false);
    Report->SetBoolField(TEXT("actual_continue_tested"), false);
    Report->SetBoolField(TEXT("cooked_compatibility_tested"), false);
    Report->SetBoolField(TEXT("original_slot_loaded_by_unreal"), false);
    Report->SetBoolField(TEXT("original_slot_saved_by_unreal"), false);
    auto Finish = [&](bool bObserved)
    {
        TArray<uint8> SourceAfter;
        TArray<uint8> CloneAfter;
        const bool bOriginalUnchanged = FFileHelper::LoadFileToArray(SourceAfter, *SourcePath) && SourceAfter == SourceBefore;
        const bool bCloneUnchanged = FFileHelper::LoadFileToArray(CloneAfter, *ClonePath) && CloneAfter == SourceBefore;
        Report->SetBoolField(TEXT("original_bytes_unchanged"), bOriginalUnchanged);
        Report->SetBoolField(TEXT("clone_bytes_unchanged"), bCloneUnchanged);
        if (bOriginalUnchanged) Report->SetStringField(TEXT("cpp_original_sha1_after"), FSHA1::HashBuffer(SourceAfter.GetData(), SourceAfter.Num()).ToString());
        if (bCloneUnchanged) Report->SetStringField(TEXT("cpp_clone_sha1_after"), FSHA1::HashBuffer(CloneAfter.GetData(), CloneAfter.Num()).ToString());
        const bool bOriginalPassed = TestTrue(TEXT("The original save bytes remain untouched after raw inspection."), bOriginalUnchanged);
        const bool bClonePassed = TestTrue(TEXT("The clone save bytes remain untouched after raw inspection."), bCloneUnchanged);
        const bool bPassed = bObserved && bOriginalPassed && bClonePassed;
        Report->SetBoolField(TEXT("passed"), bPassed);
        FString Json;
        const bool bWritten = FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Root / TEXT("Saved/Automation/TodoReview/SavedOriginalCopyNativeReview.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        return TestTrue(TEXT("The raw observation report is written to Saved only."), bWritten) && bPassed;
    };
    // Deserialize only the externally guarded disposable slot; never invoke production migration or checkpoint mutation.
    // 외부에서 보호한 일회성 슬롯만 역직렬화하며 실제 이행이나 체크포인트 변경을 호출하지 않습니다.
    TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
    if (!TestTrue(TEXT("The original byte copy deserializes as the exact RunSaveGame class."), Saved.IsValid() && Saved->GetClass() == URunSaveGame::StaticClass())) return Finish(false);
    if (!TestTrue(TEXT("The existing save contains a version and one to four actual party members."), Saved->Version > 0 && Saved->Party.Num() >= 1 && Saved->Party.Num() <= 4)) return Finish(false);
    Report->SetStringField(TEXT("loaded_class"), Saved->GetClass()->GetPathName());
    Report->SetNumberField(TEXT("version"), Saved->Version);
    Report->SetStringField(TEXT("phase"), UEnum::GetValueAsString(Saved->Phase));
    Report->SetStringField(TEXT("result"), UEnum::GetValueAsString(Saved->Result));
    Report->SetStringField(TEXT("current_node"), Saved->CurrentNode.ToString());
    Report->SetStringField(TEXT("current_encounter"), Saved->CurrentEncounter.ToString());
    TArray<TSharedPtr<FJsonValue>> Completed;
    for (const FName Node : Saved->CompletedNodes) Completed.Add(MakeShared<FJsonValueString>(Node.ToString()));
    Report->SetArrayField(TEXT("completed_nodes"), Completed);
    const FSoftObjectPath MeleePath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.BPDA_swoard_attack"));
    TArray<TSharedPtr<FJsonValue>> Party;
    for (const FRunPartyMember& Member : Saved->Party)
    {
        TSharedPtr<FJsonObject> MemberJson = MakeShared<FJsonObject>();
        MemberJson->SetNumberField(TEXT("slot_index"), Member.SlotIndex);
        MemberJson->SetStringField(TEXT("class_id"), Member.ClassId.ToString());
        MemberJson->SetNumberField(TEXT("current_hp"), Member.CurrentHP);
        MemberJson->SetNumberField(TEXT("gold"), Member.Gold);
        MemberJson->SetBoolField(TEXT("player_controlled"), Member.bPlayerControlled);
        MemberJson->SetBoolField(TEXT("has_skill_loadout"), Member.bHasSkillLoadout);
        MemberJson->SetNumberField(TEXT("item_count"), Member.Items.Num());
        MemberJson->SetBoolField(TEXT("saved_melee_path_present"), Member.Skills.Contains(MeleePath));
        TArray<TSharedPtr<FJsonValue>> Skills;
        for (const FSoftObjectPath& Skill : Member.Skills) Skills.Add(MakeShared<FJsonValueString>(Skill.ToString()));
        MemberJson->SetArrayField(TEXT("skills"), Skills);
        Party.Add(MakeShared<FJsonValueObject>(MemberJson));
    }
    Report->SetArrayField(TEXT("party"), Party);
    Report->SetNumberField(TEXT("party_count"), Party.Num());
    TSharedPtr<FJsonObject> ItemShop = MakeShared<FJsonObject>();
    ItemShop->SetNumberField(TEXT("schema_version"), Saved->ItemShopState.SchemaVersion);
    ItemShop->SetNumberField(TEXT("catalog_count"), Saved->ItemShopState.Catalog.Num());
    ItemShop->SetNumberField(TEXT("offer_count"), Saved->ItemShopState.Offers.Num());
    ItemShop->SetNumberField(TEXT("revision"), Saved->ItemShopState.Revision);
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FRunItemDefinition& Item : Saved->ItemShopState.Catalog)
    {
        TSharedPtr<FJsonObject> ItemJson = MakeShared<FJsonObject>();
        ItemJson->SetStringField(TEXT("asset"), Item.Asset.ToString());
        ItemJson->SetStringField(TEXT("stored_display_name"), Item.DisplayName.ToString());
        ItemJson->SetNumberField(TEXT("price"), Item.Price);
        Items.Add(MakeShared<FJsonValueObject>(ItemJson));
    }
    ItemShop->SetArrayField(TEXT("catalog"), Items);
    Report->SetObjectField(TEXT("frozen_item_shop"), ItemShop);
    TSharedPtr<FJsonObject> SkillShop = MakeShared<FJsonObject>();
    SkillShop->SetNumberField(TEXT("schema_version"), Saved->SkillShopState.SchemaVersion);
    SkillShop->SetNumberField(TEXT("revision"), Saved->SkillShopState.Revision);
    TArray<TSharedPtr<FJsonValue>> SkillCatalog;
    TArray<TSharedPtr<FJsonValue>> SkillOffers;
    for (const FRunSkillShopOffer& Offer : Saved->SkillShopState.Catalog) SkillCatalog.Add(MakeShared<FJsonValueObject>(SkillOffer(Offer)));
    for (const FRunSkillShopOffer& Offer : Saved->SkillShopState.Offers) SkillOffers.Add(MakeShared<FJsonValueObject>(SkillOffer(Offer)));
    SkillShop->SetArrayField(TEXT("catalog"), SkillCatalog);
    SkillShop->SetArrayField(TEXT("offers"), SkillOffers);
    SkillShop->SetNumberField(TEXT("catalog_count"), SkillCatalog.Num());
    SkillShop->SetNumberField(TEXT("offer_count"), SkillOffers.Num());
    Report->SetObjectField(TEXT("frozen_skill_shop"), SkillShop);
    // Observe the current melee label independently of frozen text without saving either the asset or the clone.
    // 에셋과 사본을 저장하지 않고 확정 저장 문구와 별도로 현재 근접 표시명을 관측합니다.
    const FString MeleeFile = Root / TEXT("Content/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack.uasset");
    TArray<uint8> MeleeBefore;
    TArray<uint8> MeleeAfter;
    if (!TestTrue(TEXT("The retained original melee DataAsset file exists."), FFileHelper::LoadFileToArray(MeleeBefore, *MeleeFile))) return Finish(false);
    TStrongObjectPtr<USkillDefinitionDataAsset> Melee(Cast<USkillDefinitionDataAsset>(MeleePath.TryLoad()));
    if (!TestTrue(TEXT("The retained melee DataAsset loads with its current display label."), Melee.IsValid() && Melee->SkillName.ToString() == TEXT("근접 공격"))) return Finish(false);
    // Resolve an output copy of the authored skill profile without migrating or applying the saved Run.
    // 저장된 Run을 이행하거나 적용하지 않고 작성 스킬 프로필의 출력 사본만 해석합니다.
    FCombatRoundSkill RuntimeMelee;
    FText RuntimeMeleeError;
    const bool bRuntimeResolved = Melee->ResolveRoundSkill(RuntimeMelee, RuntimeMeleeError);
    if (!TestTrue(TEXT("The official read-only skill resolver uses the current melee display label: ") + RuntimeMeleeError.ToString(), bRuntimeResolved && RuntimeMelee.Name.ToString() == TEXT("근접 공격"))) return Finish(false);
    TSharedPtr<FJsonObject> MeleeJson = MakeShared<FJsonObject>();
    MeleeJson->SetStringField(TEXT("asset"), MeleePath.ToString());
    MeleeJson->SetStringField(TEXT("skill_name"), Melee->SkillName.ToString());
    MeleeJson->SetStringField(TEXT("raw_authored_round_name"), Melee->RoundDefinition.Name.ToString());
    MeleeJson->SetStringField(TEXT("resolved_runtime_round_name"), RuntimeMelee.Name.ToString());
    MeleeJson->SetStringField(TEXT("cpp_asset_sha1_before"), FSHA1::HashBuffer(MeleeBefore.GetData(), MeleeBefore.Num()).ToString());
    const bool bMeleeUnchanged = FFileHelper::LoadFileToArray(MeleeAfter, *MeleeFile) && MeleeAfter == MeleeBefore;
    MeleeJson->SetBoolField(TEXT("asset_bytes_unchanged"), bMeleeUnchanged);
    Report->SetObjectField(TEXT("current_authored_melee"), MeleeJson);
    Report->SetBoolField(TEXT("raw_observation_completed"), bMeleeUnchanged);
    return Finish(TestTrue(TEXT("Reading the original melee labels does not alter its source asset bytes."), bMeleeUnchanged));
}

#endif
