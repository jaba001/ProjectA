#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentTypes.h"
#include "Materials/MaterialInterface.h"
#include "Unit/CharacterEquipmentComponent.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentBodyVisualReviewTest, "ProjectA.Review.EquipmentBodyMaterialsAndRestoration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEquipmentBodyVisualReviewTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
    if (!TestNotNull(TEXT("A disposable world supports actual component attachment"), World.Get())) return false;
    USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_02/Mesh_UE5/Separate/SKM_Primitive_02_Body.SKM_Primitive_02_Body"));
    AUnitBase* Unit = World->SpawnActor<AUnitBase>();
    if (!TestTrue(TEXT("The existing female body and native unit are available"), Body && Unit && Unit->CharacterEquipment))
    {
        World->DestroyWorld(false);
        return false;
    }
    Unit->GetMesh()->SetSkeletalMeshAsset(Body);
    UMaterialInterface* Original = Unit->GetMesh()->GetMaterial(0);
    const URunEquipmentCatalog& Catalog = URunEquipmentCatalog::Get();
    int32 TestedRobes = 0;
    for (const FRunEquipmentBodyMesh& Variant : Catalog.BodyMeshes)
    {
        if (Variant.BodyMaterials.IsEmpty()) continue;
        ++TestedRobes;
        TestTrue(TEXT("Body-specific garment selection retains the item's stable source identity"), Catalog.ResolveBodyMesh(Variant.ItemAsset, Variant.BodyId) == Variant.MeshAsset);
        TestEqual(TEXT("Other body identifiers retain the original item mesh"), Catalog.ResolveBodyMesh(Variant.ItemAsset, TEXT("UnmappedBody")), Variant.ItemAsset);
        FRunEquipmentVisual Visual;
        Visual.Asset = Variant.MeshAsset;
        Visual.bFollowBodyPose = true;
        Visual.BodyMaterials = Catalog.ResolveBodyMaterials(Variant.ItemAsset, Variant.BodyId);
        if (!TestTrue(TEXT("The real component accepts the authored robe and skin material"), Unit->CharacterEquipment->SetEquipment(true, {Visual}))) continue;
        UMaterialInterface* Mask = Cast<UMaterialInterface>(Variant.BodyMaterials[0].TryLoad());
        TestTrue(TEXT("The masked skin is installed on the instance without replacing the source body mesh"),
            Mask && Mask->GetBlendMode() == BLEND_Masked && Unit->GetMesh()->GetMaterial(0) == Mask && Unit->GetMesh()->GetSkeletalMeshAsset() == Body);
        FRunEquipmentVisual Missing = Visual;
        Missing.BodyMaterials = {FSoftObjectPath(TEXT("/Game/User_JeHoon/Validation/MissingBodyMask.MissingBodyMask"))};
        AddExpectedError(TEXT("Failed to find object"), EAutomationExpectedErrorFlags::Contains, 0);
        TestFalse(TEXT("A missing skin material rejects the complete equipment edit"), Unit->CharacterEquipment->SetEquipment(true, {Missing}));
        TestEqual(TEXT("Rejected equipment retains the previously applied skin material"), Unit->GetMesh()->GetMaterial(0), Mask);
        TestTrue(TEXT("Unequipping restores the original skin material"), Unit->CharacterEquipment->SetEquipment(true, {}) && Unit->GetMesh()->GetMaterial(0) == Original);
    }
    TestEqual(TEXT("Both current robe mappings exercise material restoration"), TestedRobes, 2);
    World->DestroyWorld(false);
    return true;
}

#endif
