#include "Game/Encounter/EncounterPrototypeStage.h"

#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DataAsset/EncounterStageVisualCatalog.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

bool EncounterPresentation::MatchesOffer(const FRunEncounterOffer& Offer, const FGameplayTagContainer& RequiredTags, const FGameplayTagContainer& ExcludedTags)
{
    const FGameplayTag ResolvedTag = Offer.GetResolvedTag();
    if (!ResolvedTag.IsValid()) return false;
    FGameplayTagContainer Tags(ResolvedTag);
    if (Offer.SelectionGroupTag.IsValid()) Tags.AddTag(Offer.SelectionGroupTag);
    return Tags.HasAll(RequiredTags) && !Tags.HasAny(ExcludedTags);
}

AEncounterPrototypeStage::AEncounterPrototypeStage()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = false;
    StageTitle = NSLOCTEXT("EncounterPrototype", "DefaultTitle", "GENERAL STORE");
    StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
    SetRootComponent(StageRoot);
    LibraryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LibraryRoot"));
    LibraryRoot->SetupAttachment(StageRoot);
    LibraryCharacter = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LibraryCharacter"));
    LibraryCharacter->SetupAttachment(LibraryRoot);
    LibraryCharacter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LibraryCharacter->SetGenerateOverlapEvents(false);
    LibraryCharacter->SetCanEverAffectNavigation(false);
    LibraryCharacter->PrimaryComponentTick.bStartWithTickEnabled = false;
    LibraryCharacter->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    LibraryLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LibraryLight"));
    LibraryLight->SetupAttachment(LibraryRoot);
    LibraryLight->SetMobility(EComponentMobility::Movable);
    LibraryLight->SetCastShadows(false);
    LibraryLight->SetAttenuationRadius(600.f);
    LibraryLight->SetVisibility(false);
    NPCBody = CreateDefaultSubobject<USceneComponent>(TEXT("NPCBody"));
    NPCBody->SetupAttachment(StageRoot);
    RightArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RightArmPivot"));
    RightArmPivot->SetupAttachment(NPCBody);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("StageCamera"));
    Camera->SetupAttachment(StageRoot);

    Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
    Counter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Counter"));
    Canopy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Canopy"));
    LeftPost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPost"));
    RightPost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPost"));
    SignBoard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignBoard"));
    Torso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Torso"));
    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    LeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArm"));
    RightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArm"));
    LeftEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftEye"));
    RightEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightEye"));
    Hat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hat"));
    DisplayBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayBase"));
    DisplayAccent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayAccent"));
    UStaticMeshComponent* StageShapes[] = {Platform, Counter, Canopy, LeftPost, RightPost, SignBoard, DisplayBase, DisplayAccent};
    for (UStaticMeshComponent* Shape : StageShapes) Shape->SetupAttachment(StageRoot);
    UStaticMeshComponent* NPCShapes[] = {Torso, Head, LeftArm, LeftEye, RightEye, Hat};
    for (UStaticMeshComponent* Shape : NPCShapes) Shape->SetupAttachment(NPCBody);
    RightArm->SetupAttachment(RightArmPivot);
    SignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignText"));
    SignText->SetupAttachment(StageRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    CubeMesh = Cube.Object;
    SphereMesh = Sphere.Object;
    CylinderMesh = Cylinder.Object;
    ConeMesh = Cone.Object;
    RefreshPrototype();
}

void AEncounterPrototypeStage::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshPrototype();
}

void AEncounterPrototypeStage::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    // Rebuild editor preview colors after map reload, once authored components have registered.
    // 작성된 컴포넌트 등록 후 맵 재로드 시 에디터 프리뷰 색상을 다시 만듭니다.
    if (GetWorld() && !GetWorld()->IsGameWorld() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) RefreshPrototype();
}

void AEncounterPrototypeStage::BeginPlay()
{
    Super::BeginPlay();
    // Recreate transient tint instances after loading an authored stage from a saved map.
    // 저장된 맵에서 작성된 무대를 로드한 뒤 일시적인 색상 인스턴스를 다시 만듭니다.
    RefreshPrototype();
    StopPresentation();
}

bool AEncounterPrototypeStage::MatchesOffer(const FRunEncounterOffer& Offer) const
{
    return EncounterPresentation::MatchesOffer(Offer, RequiredTags, ExcludedTags);
}

void AEncounterPrototypeStage::SetEncounterOffer(const FRunEncounterOffer& Offer)
{
    EncounterOffer = Offer;
    bHasEncounterOffer = true;
    StageTitle = Offer.GetDisplayName();
    RefreshPrototype();
}

void AEncounterPrototypeStage::ConfigureShape(UStaticMeshComponent* Component, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
    Component->SetStaticMesh(Mesh);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(FRotator::ZeroRotator);
    Component->SetRelativeScale3D(Scale);
    Component->SetVisibility(true);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
    if (PrototypeMaterial)
    {
        Component->SetMaterial(0, PrototypeMaterial);
        if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0)) Material->SetVectorParameterValue(TEXT("Tint"), Color);
    }
    else Component->SetMaterial(0, nullptr);
}

void AEncounterPrototypeStage::RefreshPrototype()
{
    const FLinearColor Wood(0.19f, 0.105f, 0.055f);
    const FLinearColor Skin(0.78f, 0.54f, 0.34f);
    const FLinearColor Dark(0.025f, 0.035f, 0.045f);
    const FLinearColor Gold(0.9f, 0.55f, 0.11f);
    ConfigureShape(Platform, CubeMesh, FVector(0.f, 0.f, 0.f), FVector(5.4f, 6.6f, 0.2f), FLinearColor(0.08f, 0.11f, 0.12f));
    ConfigureShape(Counter, CubeMesh, FVector(-70.f, -130.f, 65.f), FVector(1.7f, 2.6f, 1.1f), Wood);
    ConfigureShape(Canopy, CubeMesh, FVector(10.f, -130.f, 257.f), FVector(2.7f, 3.35f, 0.18f), Tint);
    ConfigureShape(LeftPost, CylinderMesh, FVector(70.f, -280.f, 135.f), FVector(0.12f, 0.12f, 2.5f), Wood);
    ConfigureShape(RightPost, CylinderMesh, FVector(70.f, 20.f, 135.f), FVector(0.12f, 0.12f, 2.5f), Wood);
    ConfigureShape(SignBoard, CubeMesh, FVector(-163.f, -130.f, 79.f), FVector(0.1f, 2.f, 0.5f), Tint * 0.42f);
    ConfigureShape(Torso, CylinderMesh, FVector(0.f, 0.f, 78.f), FVector(0.6f, 0.54f, 1.f), Tint);
    ConfigureShape(Head, SphereMesh, FVector(-4.f, 0.f, 147.f), FVector(0.58f), Skin);
    ConfigureShape(LeftArm, CylinderMesh, FVector(0.f, 40.f, 83.f), FVector(0.18f, 0.18f, 0.63f), Tint * 0.75f);
    ConfigureShape(RightArm, CylinderMesh, FVector(0.f, 0.f, -29.f), FVector(0.18f, 0.18f, 0.63f), Tint * 0.75f);
    ConfigureShape(LeftEye, SphereMesh, FVector(-31.f, -12.f, 152.f), FVector(0.072f), Dark);
    ConfigureShape(RightEye, SphereMesh, FVector(-31.f, 12.f, 152.f), FVector(0.072f), Dark);
    ConfigureShape(Hat, ConeMesh, FVector(-4.f, 0.f, 181.f), FVector(0.79f, 0.79f, 0.48f), Tint * 0.68f);
    ConfigureShape(DisplayBase, CylinderMesh, FVector(-82.f, -35.f, 136.f), FVector(0.48f, 0.48f, 0.3f), Gold);
    ConfigureShape(DisplayAccent, SphereMesh, FVector(-82.f, -35.f, 163.f), FVector(0.29f), Gold * 1.15f);

    // These five styles are scenery choices and never select stock, rewards or encounter eligibility.
    // 다섯 스타일은 무대 표현이며 재고·보상·인카운터 적격 여부를 선택하지 않습니다.
    switch (VisualStyle)
    {
    case 1:
        ConfigureShape(Counter, CylinderMesh, FVector(-65.f, -130.f, 64.f), FVector(2.1f, 2.8f, 1.08f), Dark);
        ConfigureShape(DisplayBase, CubeMesh, FVector(-85.f, -35.f, 142.f), FVector(0.85f, 0.72f, 0.28f), FLinearColor(0.27f, 0.3f, 0.32f));
        ConfigureShape(DisplayAccent, ConeMesh, FVector(-85.f, 12.f, 142.f), FVector(0.44f, 0.44f, 0.63f), Gold);
        DisplayAccent->SetRelativeRotation(FRotator(0.f, 0.f, -90.f));
        ConfigureShape(Hat, CylinderMesh, FVector(-4.f, 0.f, 177.f), FVector(0.73f, 0.73f, 0.22f), Dark);
        break;
    case 2:
        ConfigureShape(Counter, CylinderMesh, FVector(-65.f, -130.f, 49.f), FVector(2.1f, 2.8f, 0.78f), Tint * 0.55f);
        // Keep the shelter above the camera-to-face sightline while preserving its native primitive geometry.
        // 기본 도형 형태를 유지하며 지붕이 카메라에서 NPC 얼굴을 향한 시선을 가리지 않도록 높입니다.
        ConfigureShape(Canopy, ConeMesh, FVector(70.f, -130.f, 310.f), FVector(3.4f, 3.4f, 0.9f), Tint);
        ConfigureShape(LeftPost, CylinderMesh, FVector(70.f, -280.f, 145.f), FVector(0.12f, 0.12f, 2.7f), Wood);
        ConfigureShape(RightPost, CylinderMesh, FVector(70.f, 20.f, 145.f), FVector(0.12f, 0.12f, 2.7f), Wood);
        ConfigureShape(DisplayBase, CubeMesh, FVector(-174.f, -130.f, 80.f), FVector(0.08f, 0.17f, 0.65f), FLinearColor::White);
        ConfigureShape(DisplayAccent, CubeMesh, FVector(-176.f, -130.f, 80.f), FVector(0.08f, 0.65f, 0.17f), FLinearColor::White);
        SignBoard->SetRelativeLocation(FVector(-163.f, -130.f, 28.f));
        ConfigureShape(Hat, CylinderMesh, FVector(-4.f, 0.f, 178.f), FVector(0.72f, 0.72f, 0.25f), FLinearColor(0.85f, 0.88f, 0.81f));
        break;
    case 3:
        ConfigureShape(DisplayBase, SphereMesh, FVector(-90.f, -35.f, 146.f), FVector(0.5f), FLinearColor(0.12f, 0.7f, 0.48f));
        ConfigureShape(DisplayAccent, CylinderMesh, FVector(-90.f, -35.f, 179.f), FVector(0.18f, 0.18f, 0.37f), Gold);
        ConfigureShape(Hat, ConeMesh, FVector(-4.f, 0.f, 193.f), FVector(0.92f, 0.92f, 0.7f), Tint * 0.65f);
        break;
    case 4:
        ConfigureShape(Platform, CylinderMesh, FVector(0.f, 0.f, 0.f), FVector(6.3f, 6.3f, 0.2f), FLinearColor(0.1f, 0.13f, 0.19f));
        ConfigureShape(Counter, CylinderMesh, FVector(-65.f, -130.f, 49.f), FVector(2.1f, 2.8f, 0.78f), Tint * 0.4f);
        ConfigureShape(DisplayBase, CylinderMesh, FVector(-65.f, -12.f, 123.f), FVector(0.45f, 0.45f, 0.95f), Gold);
        ConfigureShape(DisplayAccent, SphereMesh, FVector(-65.f, -12.f, 193.f), FVector(0.67f), FLinearColor(0.23f, 0.72f, 1.f));
        Canopy->SetVisibility(false);
        LeftPost->SetVisibility(false);
        RightPost->SetVisibility(false);
        break;
    default:
        break;
    }

    SignText->SetRelativeLocation(FVector(-171.f, -130.f, VisualStyle == 2 ? 28.f : 79.f));
    SignText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
    SignText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    SignText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    SignText->SetWorldSize(18.f);
    SignText->SetTextRenderColor(FColor(244, 237, 210));
    SignText->SetText(StageTitle);
    SignText->SetVisibility(true);
    SignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SignText->SetCanEverAffectNavigation(false);
    Camera->SetRelativeLocation(FVector(-850.f, 160.f, 380.f));
    // Keep the merchant centered in the space between the always-visible inventory and shop panels.
    // 항상 표시되는 가방과 상품 패널 사이 공간의 중앙에 상인이 오도록 맞춥니다.
    Camera->SetRelativeRotation((FVector(35.f, -170.f, 145.f) - Camera->GetRelativeLocation()).Rotation());
    Camera->SetFieldOfView(80.f);
    Camera->SetConstraintAspectRatio(false);
    Camera->bOverrideAspectRatioAxisConstraint = true;
    Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);
    ResetPose();
    RebuildLibraryVisuals();
    if (bUsingLibraryVisuals && bEnvironmentOnly) Camera->SetRelativeRotation((LibraryPresentationFocus + FVector(0.f, -40.f, 0.f) - Camera->GetRelativeLocation()).Rotation());
    SetActorTickEnabled(bPresenting && bPresentationVisible && !IsHidden() && !bUsingLibraryVisuals);
}

void AEncounterPrototypeStage::ClearLibraryVisuals()
{
    for (UStaticMeshComponent* Prop : LibraryProps) if (IsValid(Prop)) Prop->DestroyComponent();
    for (USkeletalMeshComponent* Part : LibraryParts) if (IsValid(Part)) Part->DestroyComponent();
    LibraryProps.Reset();
    LibraryParts.Reset();
    LibraryCharacter->Stop();
    LibraryCharacter->SetAnimation(nullptr);
    LibraryCharacter->bPauseAnims = true;
    LibraryCharacter->SetComponentTickEnabled(false);
    LibraryCharacter->SetSkeletalMesh(nullptr);
    LibraryCharacter->SetRelativeTransform(FTransform::Identity);
    LibraryCharacter->OverrideMaterials.Reset();
    LibraryRoot->SetVisibility(false, true);
    LibraryLight->SetVisibility(false);
    LibraryLight->SetIntensity(0.f);
    LibraryProfileId = NAME_None;
    LibraryFaceBone = NAME_None;
    bUsingLibraryVisuals = false;
    bEnvironmentOnly = false;
    LibraryPresentationFocus = FVector::ZeroVector;
    LibraryPresentationFocusRadius = 0.f;
}

void AEncounterPrototypeStage::RebuildLibraryVisuals()
{
    if (bRefreshingLibrary || IsTemplate() || !GetWorld()) return;
    TGuardValue<bool> RefreshGuard(bRefreshingLibrary, true);
    ClearLibraryVisuals();
    if (!bUseLibraryVisuals || GetNetMode() == NM_DedicatedServer) return;
    const UEncounterStageVisualCatalog* Catalog = VisualCatalog ? VisualCatalog.Get() : GetDefault<UEncounterStageVisualCatalog>();
    const FEncounterStageVisualProfile* Profile = bHasEncounterOffer ? Catalog->Resolve(EncounterOffer) : Catalog->Resolve(RequiredTags);
    if (!Profile || Profile->PresentationFocus.ContainsNaN() || !FMath::IsFinite(Profile->PresentationFocusRadius) || Profile->PresentationFocusRadius <= 0.f || Profile->LightLocation.ContainsNaN() || !FMath::IsFinite(Profile->LightIntensity) || Profile->LightIntensity < 0.f) return;
    if (!FMath::IsFinite(Profile->LightColor.R) || !FMath::IsFinite(Profile->LightColor.G) || !FMath::IsFinite(Profile->LightColor.B) || !FMath::IsFinite(Profile->LightColor.A)) return;
    USkeletalMesh* Character = nullptr;
    UAnimSequence* Idle = nullptr;
    FBox CharacterBounds(ForceInit);
    float CharacterScale = 1.f;
    TArray<USkeletalMesh*> Parts;
    if (!Profile->bEnvironmentOnly)
    {
        Character = Profile->CharacterMesh.LoadSynchronous();
        Idle = Profile->IdleAnimation.LoadSynchronous();
        if (!Character || !Character->GetSkeleton() || !Idle || !Idle->GetSkeleton() || !Idle->GetSkeleton()->IsCompatibleMesh(Character))
        {
            UE_LOG(LogTemp, Warning, TEXT("Encounter stage %s retained prototype scenery: missing or incompatible character/idle for %s."), *GetName(), *Profile->ProfileId.ToString());
            return;
        }
        CharacterBounds = Character->GetBounds().GetBox();
        if (!CharacterBounds.IsValid || CharacterBounds.Min.ContainsNaN() || CharacterBounds.Max.ContainsNaN() || CharacterBounds.GetSize().Z <= UE_SMALL_NUMBER || !FMath::IsFinite(Profile->CharacterHeight) || Profile->CharacterHeight <= 0.f || Profile->CharacterLocation.ContainsNaN() || Profile->CharacterRotation.ContainsNaN()) return;
        CharacterScale = Profile->CharacterHeight / CharacterBounds.GetSize().Z;
        if (!FMath::IsFinite(CharacterScale) || CharacterScale <= UE_SMALL_NUMBER) return;
        for (const TSoftObjectPtr<USkeletalMesh>& PartPath : Profile->CharacterParts)
        {
            USkeletalMesh* Part = PartPath.LoadSynchronous();
            if (!Part || Part->GetSkeleton() != Character->GetSkeleton()) return;
            Parts.Add(Part);
        }
    }
    TArray<UStaticMesh*> Props;
    TArray<UMaterialInterface*> Materials;
    TArray<FTransform> PropTransforms;
    for (const FEncounterStageProp& Prop : Profile->Props)
    {
        UStaticMesh* Mesh = Prop.Mesh.LoadSynchronous();
        UMaterialInterface* Material = Prop.MaterialOverride.IsNull() ? nullptr : Prop.MaterialOverride.LoadSynchronous();
        if (!Mesh || (!Prop.MaterialOverride.IsNull() && !Material) || Prop.MaxSize.ContainsNaN() || Prop.MaxSize.GetMin() <= 0.f || Prop.Location.ContainsNaN() || Prop.Rotation.ContainsNaN()) return;
        if (!Prop.AttachBone.IsNone() && (!Character || Character->GetRefSkeleton().FindBoneIndex(Prop.AttachBone) == INDEX_NONE)) return;
        const FBox Bounds = Mesh->GetBoundingBox();
        if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN()) return;
        const FBox RotatedBounds = Bounds.TransformBy(FTransform(Prop.Rotation));
        const FVector Size = Bounds.GetSize();
        if (Size.ContainsNaN() || Size.GetMax() <= UE_SMALL_NUMBER) return;
        // Preserve authored fitting in the original mesh axes; flat water meshes contribute no zero-axis divisor.
        // 원본 메시 축 기준의 작성된 맞춤 배율을 유지하며 평면 수면 메시의 영 크기 축은 분모에서 제외합니다.
        double Scale = TNumericLimits<double>::Max();
        for (int32 Axis = 0; Axis < 3; ++Axis) if (Size[Axis] > UE_SMALL_NUMBER) Scale = FMath::Min(Scale, Prop.MaxSize[Axis] / Size[Axis]);
        if (!FMath::IsFinite(Scale) || Scale <= UE_SMALL_NUMBER) return;
        const bool bCentered = Prop.bCenterAnchor || !Prop.AttachBone.IsNone();
        const FVector Anchor = bCentered ? RotatedBounds.GetCenter() : FVector(RotatedBounds.GetCenter().X, RotatedBounds.GetCenter().Y, RotatedBounds.Min.Z);
        const FTransform Transform(Prop.Rotation, Prop.Location - Anchor * Scale, FVector(Scale));
        if (Transform.ContainsNaN()) return;
        Props.Add(Mesh);
        Materials.Add(Material);
        PropTransforms.Add(Transform);
    }

    // Validate all assets first, then fit original characters at their feet without changing source materials.
    // 모든 에셋을 먼저 검증한 뒤 원본 재질을 바꾸지 않고 원본 캐릭터의 발을 바닥에 맞춥니다.
    if (Character)
    {
        const FVector CharacterBottom(CharacterBounds.GetCenter().X, CharacterBounds.GetCenter().Y, CharacterBounds.Min.Z);
        LibraryCharacter->SetSkeletalMesh(Character);
        LibraryCharacter->SetRelativeTransform(FTransform(Profile->CharacterRotation, Profile->CharacterLocation - Profile->CharacterRotation.RotateVector(CharacterBottom * CharacterScale), FVector(CharacterScale)));
        LibraryCharacter->SetSimulatePhysics(false);
        LibraryCharacter->PlayAnimation(Idle, true);
        LibraryCharacter->SetPosition(0.f, false);
        LibraryCharacter->TickAnimation(0.f, false);
        LibraryCharacter->RefreshBoneTransforms();
        for (USkeletalMesh* Mesh : Parts)
        {
            USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
            Part->SetupAttachment(LibraryCharacter);
            Part->SetSkeletalMesh(Mesh);
            Part->SetMobility(EComponentMobility::Movable);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Part->SetGenerateOverlapEvents(false);
            Part->SetCanEverAffectNavigation(false);
            Part->SetLeaderPoseComponent(LibraryCharacter);
            Part->PrimaryComponentTick.bStartWithTickEnabled = false;
            AddInstanceComponent(Part);
            Part->RegisterComponent();
            Part->SetComponentTickEnabled(false);
            LibraryParts.Add(Part);
        }
    }
    for (int32 Index = 0; Index < Props.Num(); ++Index)
    {
        const FEncounterStageProp& Spec = Profile->Props[Index];
        FTransform Transform = PropTransforms[Index];
        if (!Spec.AttachBone.IsNone())
        {
            const FVector ParentScale = LibraryCharacter->GetSocketTransform(Spec.AttachBone, RTS_World).GetScale3D();
            if (ParentScale.ContainsNaN() || ParentScale.GetAbsMin() <= UE_SMALL_NUMBER)
            {
                ClearLibraryVisuals();
                return;
            }
            // Bone attachments keep authored grip offsets and dimensions in world units despite character fitting.
            // 본 부착물은 캐릭터 맞춤 배율과 관계없이 작성된 손잡이 오프셋과 크기를 월드 단위로 유지합니다.
            Transform.SetTranslation(Transform.GetTranslation() / ParentScale);
        }
        UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
        Prop->SetupAttachment(Spec.AttachBone.IsNone() ? LibraryRoot.Get() : LibraryCharacter.Get(), Spec.AttachBone);
        Prop->SetStaticMesh(Props[Index]);
        Prop->SetMobility(EComponentMobility::Movable);
        if (!Spec.AttachBone.IsNone()) Prop->SetAbsolute(false, false, true);
        Prop->SetRelativeTransform(Transform);
        if (Materials[Index]) for (int32 MaterialIndex = 0; MaterialIndex < FMath::Max(1, Props[Index]->GetStaticMaterials().Num()); ++MaterialIndex) Prop->SetMaterial(MaterialIndex, Materials[Index]);
        Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Prop->SetGenerateOverlapEvents(false);
        Prop->SetCanEverAffectNavigation(false);
        AddInstanceComponent(Prop);
        Prop->RegisterComponent();
        LibraryProps.Add(Prop);
    }

    UStaticMeshComponent* ReplacedShapes[] = {Counter, Canopy, LeftPost, RightPost, Torso, Head, LeftArm, RightArm, LeftEye, RightEye, Hat, DisplayBase, DisplayAccent};
    for (UStaticMeshComponent* Shape : ReplacedShapes) Shape->SetVisibility(false);
    ConfigureShape(Platform, CubeMesh, FVector(0.f, 0.f, 2.f), FVector(5.4f, 7.6f, 0.2f), FLinearColor(0.08f, 0.11f, 0.12f));
    SignBoard->SetRelativeLocation(FVector(-235.f, -130.f, 35.f));
    SignBoard->SetRelativeScale3D(FVector(0.1f, 2.f, 0.4f));
    SignText->SetRelativeLocation(FVector(-243.f, -130.f, 35.f));
    SignText->SetWorldSize(15.f);
    LibraryProfileId = Profile->ProfileId;
    LibraryFaceBone = Profile->FaceBone;
    bUsingLibraryVisuals = true;
    bEnvironmentOnly = Profile->bEnvironmentOnly;
    LibraryPresentationFocus = Profile->PresentationFocus;
    LibraryPresentationFocusRadius = Profile->PresentationFocusRadius;
    LibraryRoot->SetVisibility(true, true);
    LibraryCharacter->SetVisibility(!bEnvironmentOnly);
    if (bEnvironmentOnly)
    {
        // Keep the stone base at the authored prop floor height for both dungeon and direct presentations.
        // 던전과 직접 연출 모두에서 소품의 작성 바닥 높이에 맞춘 석재 받침을 유지합니다.
        SignBoard->SetVisibility(false);
        SignText->SetVisibility(false);
    }
    LibraryLight->SetRelativeLocation(Profile->LightLocation);
    LibraryLight->SetLightColor(Profile->LightColor);
    LibraryLight->SetIntensity(Profile->LightIntensity);
    UpdateLibraryPlayback();
}

void AEncounterPrototypeStage::UpdateLibraryPlayback()
{
    const bool bActive = bUsingLibraryVisuals && !bEnvironmentOnly && LibraryCharacter->GetSkeletalMeshAsset() && bPresenting && bPresentationVisible && !IsHidden();
    LibraryCharacter->bPauseAnims = !bActive;
    if (UAnimSingleNodeInstance* Animation = LibraryCharacter->GetSingleNodeInstance()) Animation->SetPlaying(bActive);
    LibraryCharacter->SetComponentTickEnabled(bActive);
    LibraryLight->SetVisibility(bUsingLibraryVisuals && bPresentationVisible && !IsHidden());
}

void AEncounterPrototypeStage::SetPresentationVisible(bool bVisible)
{
    bPresentationVisible = bVisible;
    SetActorHiddenInGame(!bVisible);
    UpdateLibraryPlayback();
    SetActorTickEnabled(bPresenting && bVisible && !bUsingLibraryVisuals);
}

bool AEncounterPrototypeStage::GetPresentationFocus(FVector& OutCenter, float& OutRadius) const
{
    if (IsHidden()) return false;
    if (bUsingLibraryVisuals && bEnvironmentOnly)
    {
        OutCenter = LibraryRoot->GetComponentTransform().TransformPosition(LibraryPresentationFocus);
        OutRadius = LibraryPresentationFocusRadius * LibraryRoot->GetComponentScale().GetAbsMax();
        return !OutCenter.ContainsNaN() && FMath::IsFinite(OutRadius) && OutRadius > 0.f;
    }
    if (bUsingLibraryVisuals && LibraryCharacter->GetSkeletalMeshAsset())
    {
        if (LibraryCharacter->GetBoneIndex(LibraryFaceBone) != INDEX_NONE) OutCenter = LibraryCharacter->GetBoneLocation(LibraryFaceBone);
        else OutCenter = LibraryCharacter->Bounds.Origin + FVector(0.f, 0.f, LibraryCharacter->Bounds.BoxExtent.Z * 0.7f);
        OutRadius = 16.f * GetActorScale3D().GetAbsMax();
        return true;
    }
    if (!Head->IsVisible()) return false;
    OutCenter = Head->Bounds.Origin;
    OutRadius = Head->Bounds.SphereRadius;
    return true;
}

void AEncounterPrototypeStage::ResetPose()
{
    NPCBody->SetRelativeLocation(FVector(35.f, -130.f, 22.f));
    RightArmPivot->SetRelativeLocation(FVector(0.f, -40.f, 112.f));
    RightArmPivot->SetRelativeRotation(FRotator::ZeroRotator);
}

void AEncounterPrototypeStage::StartPresentation()
{
    if (bPresenting || GetNetMode() == NM_DedicatedServer) return;
    bPresenting = true;
    PresentationSeconds = 0.0f;
    ResetPose();
    UpdateLibraryPlayback();
    SetActorTickEnabled(bPresentationVisible && !bUsingLibraryVisuals);
}

void AEncounterPrototypeStage::StopPresentation()
{
    bPresenting = false;
    PresentationSeconds = 0.0f;
    SetActorTickEnabled(false);
    UpdateLibraryPlayback();
    ResetPose();
}

void AEncounterPrototypeStage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPresenting || bUsingLibraryVisuals || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) return;
    PresentationSeconds += DeltaSeconds;
    NPCBody->SetRelativeLocation(FVector(35.f, -130.f, 22.f + FMath::Sin(PresentationSeconds * 2.1f) * 1.2f));
    float Wave = 0.0f;
    if (PresentationSeconds < 1.0f)
    {
        const float Envelope = FMath::Min(FMath::Clamp(PresentationSeconds / 0.18f, 0.0f, 1.0f), FMath::Clamp((1.0f - PresentationSeconds) / 0.2f, 0.0f, 1.0f));
        Wave = Envelope * (145.0f + 22.0f * FMath::Sin(PresentationSeconds * 19.0f));
    }
    RightArmPivot->SetRelativeRotation(FRotator(0.f, 0.f, Wave));
}
