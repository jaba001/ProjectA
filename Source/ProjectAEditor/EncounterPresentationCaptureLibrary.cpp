#include "EncounterPresentationCaptureLibrary.h"

#include "Camera/CameraComponent.h"
#include "AssetCompilingManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Encounter/EncounterDungeonRoute.h"
#include "Game/Encounter/EncounterPrototypeStage.h"
#include "Game/Run/RunEncounterTypes.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "FXSystem.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "ShaderCompiler.h"
#include "UObject/StrongObjectPtr.h"

bool UEncounterPresentationCaptureLibrary::CaptureDungeonInteriors(UWorld* World, const FString& OutputDirectory)
{
    // Require an explicit screenshot-only invocation and keep every result beneath the project's Saved directory.
    // 명시적인 스크린샷 전용 실행만 허용하고 결과를 프로젝트 Saved 아래에 둡니다.
    FString Output = FPaths::ConvertRelativePathToFull(OutputDirectory);
    FString AllowedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / TEXT("Screenshots/Encounter");
    FPaths::NormalizeDirectoryName(Output);
    FPaths::NormalizeDirectoryName(AllowedRoot);
    if (!World || World->IsGameWorld() || !FApp::CanEverRender() || !FParse::Param(FCommandLine::Get(), TEXT("EncounterInteriorScreenshots")) || !FPaths::IsUnderDirectory(Output, AllowedRoot) || !IFileManager::Get().MakeDirectory(*Output, true))
    {
        UE_LOG(LogTemp, Error, TEXT("Interior capture guard rejected: world=%d game=%d render=%d requested=%d output=%s root=%s"), World != nullptr, World && World->IsGameWorld(), FApp::CanEverRender(), FParse::Param(FCommandLine::Get(), TEXT("EncounterInteriorScreenshots")), *Output, *AllowedRoot);
        return false;
    }
    FRunEncounterOffer Offer;
    Offer.EncounterId = TEXT("TorchScreenshotBasicShop");
    Offer.EncounterTag = FRunEncounterOffer::GetBasicItemShopTag();
    if (FParse::Param(FCommandLine::Get(), TEXT("EncounterCaptureShield")))
    {
        Offer.EncounterTag = FRunEncounterOffer::GetTagItemShopTag();
        Offer.ItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield")));
    }
    AEncounterPrototypeStage* Template = nullptr;
    for (TActorIterator<AEncounterPrototypeStage> It(World); It; ++It)
    {
        if (It->PrototypeMaterial && It->MatchesOffer(Offer) && (!Template || It->Priority > Template->Priority))
        {
            Template = *It;
        }
    }
    if (!Template)
    {
        UE_LOG(LogTemp, Error, TEXT("Interior capture requires a loaded production stage with its material."));
        return false;
    }
    // Initialize only the renderer's FX interface; commandlet worlds do not create it by default.
    // 명령렛 월드에 기본 생성되지 않는 렌더러 FX 인터페이스만 초기화합니다.
    if (!World->FXSystem)
    {
        World->FXSystem = FFXSystemInterface::Create(World->GetFeatureLevel(), World->Scene);
        World->Scene->SetFXSystem(World->FXSystem.Get());
    }
    FActorSpawnParameters Parameters;
    Parameters.ObjectFlags |= RF_Transient;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEncounterDungeonRoute* Route = World->SpawnActor<AEncounterDungeonRoute>(AEncounterDungeonRoute::StaticClass(), FVector(0.f, 0.f, -6000.f), FRotator::ZeroRotator, Parameters);
    ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
    if (!Route || !CaptureActor)
    {
        if (Route) Route->Destroy();
        if (CaptureActor) CaptureActor->Destroy();
        return false;
    }
    bool bSuccess = Route->ConfigureLayout(0);
    USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
    TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>(GetTransientPackage()));
    Target->RenderTargetFormat = RTF_RGBA8;
    Target->InitAutoFormat(1280, 720);
    Target->UpdateResourceImmediate(true);
    Capture->TextureTarget = Target.Get();
    Capture->CaptureSource = SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->bAlwaysPersistRenderingState = true;
    Capture->FOVAngle = Route->Camera->FieldOfView;
    Capture->PostProcessSettings = Route->Camera->PostProcessSettings;
    Capture->PostProcessBlendWeight = Route->Camera->PostProcessBlendWeight;
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    TArray<UNiagaraComponent*> Flames;
    Route->GetComponents(Flames);
    for (UNiagaraComponent* Flame : Flames)
    {
        Flame->SetForceSolo(true);
        Flame->Activate(true);
        Flame->AdvanceSimulation(15, 1.f / 30.f);
        Flame->MarkRenderDynamicDataDirty();
        UE_LOG(LogTemp, Display, TEXT("Interior flame active=%d"), Flame->IsActive());
    }
    World->SendAllEndOfFrameUpdates();
    const auto Save = [&](const FString& Name, const FVector& Location, const FRotator& Rotation, float FieldOfView = 80.f)
    {
        Capture->FOVAngle = FieldOfView;
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        World->SendAllEndOfFrameUpdates();
        CaptureActor->SetActorLocationAndRotation(Location, Rotation);
        for (int32 Frame = 0; Frame < 8; ++Frame)
        {
            Capture->CaptureScene();
            FlushRenderingCommands();
        }
        UKismetRenderingLibrary::ExportRenderTarget(World, Target.Get(), Output, Name);
        return IFileManager::Get().FileSize(*(Output / Name)) > 0;
    };
    const FVector Origin = Route->GetActorLocation();
    bSuccess = bSuccess && Save(TEXT("01_Junction.png"), Origin + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight), FRotator::ZeroRotator, EncounterDungeonLayout::ChoiceFieldOfView);
    bSuccess = bSuccess && Save(TEXT("02_TorchAndStone.png"), Origin + FVector(-150.f, 0.f, EncounterDungeonLayout::EyeHeight), FRotator(16.f, 35.f, 0.f));
    const TArray<FVector> Path = EncounterDungeonLayout::GetPath(0, 0);
    const FRotator CorridorRotation = Path.Num() > 4 ? (Path[4] - Path[3]).Rotation() : FRotator::ZeroRotator;
    bSuccess = bSuccess && Path.Num() > 4 && Save(TEXT("03_Corridor.png"), Origin + Path[3] + CorridorRotation.Vector() * 200.f, CorridorRotation);
    AEncounterPrototypeStage* Stage = Route->ConfigureStage(Template, 0, Offer);
    bSuccess = bSuccess && Stage && Save(TEXT("04_Shop.png"), Stage->Camera->GetComponentLocation(), Stage->Camera->GetComponentRotation());
    if (Stage && FParse::Param(FCommandLine::Get(), TEXT("EncounterCaptureShield")))
    {
        FVector Focus;
        float Radius = 0.f;
        if (Stage->GetPresentationFocus(Focus, Radius))
        {
            const FVector Center = Focus - FVector(0.f, 0.f, 45.f);
            const FVector Front = Center - Stage->Camera->GetForwardVector() * 240.f;
            const FVector Side = Center + Stage->Camera->GetRightVector() * 240.f;
            bSuccess = bSuccess && Save(TEXT("05_ShieldFront.png"), Front, (Center - Front).Rotation(), 55.f);
            bSuccess = bSuccess && Save(TEXT("06_ShieldSide.png"), Side, (Center - Side).Rotation(), 55.f);
        }
    }
    bSuccess = bSuccess && Save(TEXT("01_Junction.png"), Origin + FVector(0.f, 0.f, EncounterDungeonLayout::EyeHeight), FRotator::ZeroRotator, EncounterDungeonLayout::ChoiceFieldOfView);
    CaptureActor->Destroy();
    Route->Destroy();
    return bSuccess;
}
