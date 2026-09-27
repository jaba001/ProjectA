#include "Game/GameModes/CombatDebugGameMode.h"
#include "Combat/CombatManager.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
#include "Unit/CharacterAppearanceComponent.h"
#include "Unit/CharacterEquipmentComponent.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"

ACombatDebugGameMode::ACombatDebugGameMode()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = ACombatDebugPlayerController::StaticClass();
}

bool ACombatDebugGameMode::IsDebugWorld(const UWorld* World)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return World && World->GetNetMode() == NM_Standalone && Cast<ACombatDebugGameMode>(World->GetAuthGameMode()) != nullptr;
#endif
}

void ACombatDebugGameMode::BeginPlay()
{
    Super::BeginPlay();
    // Wait for the placed grid and local controller before creating the disposable session.
    // 배치된 Grid와 로컬 컨트롤러가 준비된 다음 일회성 세션을 생성합니다.
    if (HasAuthority()) InitializeTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ACombatDebugGameMode::InitializeDebugCombat);
}

void ACombatDebugGameMode::InitializeDebugCombat()
{
    RestartCombat();
}

bool ACombatDebugGameMode::RestartCombat()
{
    StatusMessage = FText::GetEmpty();
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (!HasAuthority() || !IsDebugWorld(GetWorld()) || (Run && (Run->IsManagedRun() || Run->HasManagedLease())))
    {
        StatusMessage = NSLOCTEXT("CombatDebug", "Unavailable", "디버그 전투는 관리 Run이 없는 로컬 개발 실행에서만 사용할 수 있습니다.");
        return false;
    }
    ACombatDebugPlayerController* Controller = Cast<ACombatDebugPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!IsValid(Controller) || !Controller->IsLocalController() || !IsValid(PartyDefinition) || !IsValid(EnemyDefinition))
    {
        StatusMessage = NSLOCTEXT("CombatDebug", "MissingSetup", "디버그 컨트롤러, 파티 또는 적 정의가 없습니다.");
        return false;
    }
    if (!IsValid(Arena))
    {
        for (TActorIterator<ACombatArena> It(GetWorld()); It; ++It)
        {
            if (IsValid(Arena))
            {
                StatusMessage = NSLOCTEXT("CombatDebug", "MultipleArenas", "디버그 레벨에는 전투장이 하나만 있어야 합니다.");
                Arena = nullptr;
                return false;
            }
            Arena = *It;
        }
    }
    if (!IsValid(Arena))
    {
        StatusMessage = NSLOCTEXT("CombatDebug", "MissingArena", "디버그 전투장이 없습니다.");
        return false;
    }
    CleanupDebugCombat();
    if (!Arena->PrepareArena(StatusMessage)) return false;
    if (!IsValid(CombatManager))
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        CombatManager = GetWorld()->SpawnActor<ACombatManager>(ACombatManager::StaticClass(), FTransform::Identity, Params);
    }
    if (!IsValid(CombatManager) || !SpawnDebugUnits())
    {
        if (StatusMessage.IsEmpty()) StatusMessage = NSLOCTEXT("CombatDebug", "SpawnFailed", "디버그 전투 유닛을 생성하지 못했습니다.");
        CleanupDebugCombat();
        return false;
    }
    TArray<AUnitBase*> Units;
    for (AUnitBase* Unit : SpawnedUnits) Units.Add(Unit);
    CombatManager->SetCombatGrid(Arena->Grid);
    CombatManager->RegisterUnits(Units);
    Controller->SetCombatContext(CombatManager, false);
    Arena->ActivateArena(Controller);
    CombatManager->StartCombat_Internal();
    if (!CombatManager->IsCombatActive())
    {
        StatusMessage = NSLOCTEXT("CombatDebug", "StartFailed", "디버그 전투를 시작하지 못했습니다. 전투 설정 로그를 확인하세요.");
        CleanupDebugCombat();
        return false;
    }
    Controller->SetCombatContext(CombatManager, true);
    StatusMessage = NSLOCTEXT("CombatDebug", "Ready", "독립 디버그 전투 · Run 저장에 반영되지 않습니다.");
    return true;
}

bool ACombatDebugGameMode::SpawnDebugUnits()
{
    FProfessionDefinition Profession;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> StartingSkills;
    if (!PartyDefinition->ResolveProfession(TEXT("Warrior"), Profession, StatusMessage) || !PartyDefinition->ResolveStartingSkills(TEXT("Warrior"), StartingSkills, StatusMessage)) return false;
    if (Arena->PlayerCoords.IsEmpty() || !EnemyDefinition->OpponentSnapshotSlot.IsNone() || EnemyDefinition->EnemyUnitClasses.IsEmpty() || EnemyDefinition->EnemyUnitClasses.Num() > Arena->EnemyCoords.Num())
    {
        StatusMessage = NSLOCTEXT("CombatDebug", "InvalidFormation", "디버그 전투에는 플레이어 배치와 유효한 기본 적 목록이 필요합니다.");
        return false;
    }
    ACombatGridTile* PlayerTile = Arena->Grid->GetTileAtCoord(Arena->PlayerCoords[0]);
    if (!IsValid(PlayerTile) || PlayerTile->GetOccupyingUnit() || PlayerTile->GetTerritory() != ETileTerritory::Player) return false;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APlayerUnit* Player = GetWorld()->SpawnActor<APlayerUnit>(Profession.CombatClass, PlayerTile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, 90.f, 0.f), Params);
    if (!IsValid(Player)) return false;
    SpawnedUnits.Add(Player);
    if (!Player->CharacterAppearance || !Player->CharacterAppearance->SetAppearance(Profession.AppearanceCatalog, FCharacterAppearanceSelection())) return false;
    if (!Player->ConfigureProfession(Profession.MaxHP, Profession.ActionPoints, Profession.SubActionPoints, StartingSkills, Profession.Strength, Profession.Dexterity, Profession.Intelligence)) return false;
    if (!Player->CharacterEquipment || !Player->CharacterEquipment->SetEquipment(true, TArray<FRunEquipmentVisual>())) return false;
    Player->RuntimeCharacterName = NSLOCTEXT("CombatDebug", "WarriorName", "디버그 전사");
    Player->SetTeam(ETeam::Player);
    Player->SetCurrentTile(PlayerTile);
    for (int32 Index = 0; Index < EnemyDefinition->EnemyUnitClasses.Num(); ++Index)
    {
        const TSubclassOf<AEnemyUnit> EnemyClass = EnemyDefinition->EnemyUnitClasses[Index];
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Arena->EnemyCoords[Index]);
        if (!EnemyClass || !IsValid(Tile) || Tile->GetOccupyingUnit() || Tile->GetTerritory() != ETileTerritory::Enemy) return false;
        AEnemyUnit* Enemy = GetWorld()->SpawnActor<AEnemyUnit>(EnemyClass, Tile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, -90.f, 0.f), Params);
        if (!IsValid(Enemy)) return false;
        SpawnedUnits.Add(Enemy);
        Enemy->SetTeam(ETeam::Enemy);
        Enemy->SetCurrentTile(Tile);
    }
    return true;
}

void ACombatDebugGameMode::CleanupDebugCombat()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ACombatDebugPlayerController* Controller = Cast<ACombatDebugPlayerController>(It->Get())) Controller->SetCombatContext(nullptr, false);
    }
    // Stop round callbacks, projectiles and effects before destroying their units.
    // 유닛을 제거하기 전에 라운드 콜백과 투사체 및 효과를 중단합니다.
    if (IsValid(CombatManager)) CombatManager->ResetCombat();
    for (AUnitBase* Unit : SpawnedUnits)
    {
        if (IsValid(Unit)) Unit->Destroy();
    }
    SpawnedUnits.Reset();
    if (IsValid(Arena)) Arena->CleanupArena();
}

void ACombatDebugGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(InitializeTimer);
    CleanupDebugCombat();
    if (IsValid(CombatManager)) CombatManager->Destroy();
    CombatManager = nullptr;
    Super::EndPlay(EndPlayReason);
}
