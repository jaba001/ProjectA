#include "Game/GameModes/CombatDebugGameMode.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/CapsuleComponent.h"
#include "Controller/CombatDebugPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GameFramework/HUD.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
#include "Unit/CharacterAppearanceComponent.h"
#include "Unit/CharacterEquipmentComponent.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"

namespace
{
    constexpr float DebugMaxHP = 10000.f;

    bool IsUsableEnemyClass(const UClass* Class)
    {
        return IsValid(Class) && Class->IsChildOf(AEnemyUnit::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
    }
}

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

void ACombatDebugGameMode::InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer)
{
    // CommonUI owns the debug screen; only spawn an AHUD when an explicit class is configured.
    // CommonUI가 디버그 화면을 소유하며 명시적인 클래스가 있을 때만 AHUD를 생성합니다.
    if (NewPlayer && HUDClass) Super::InitializeHUDForPlayer_Implementation(NewPlayer);
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
    if (Arena->PlayerCoords.IsEmpty() || Arena->EnemyCoords.IsEmpty() || !EnemyDefinition->OpponentSnapshotSlot.IsNone() || EnemyDefinition->EnemyUnitClasses.IsEmpty())
    {
        StatusMessage = NSLOCTEXT("CombatDebug", "InvalidFormation", "디버그 전투에는 플레이어 배치와 유효한 기본 적 목록이 필요합니다.");
        return false;
    }
    AUnitBase* Player = SpawnConfiguredDebugUnit(false, TEXT("Warrior"), Arena->Grid->GetTileAtCoord(Arena->PlayerCoords[0]), StatusMessage);
    if (!IsValid(Player)) return false;
    SpawnedUnits.Add(Player);
    const TSubclassOf<AEnemyUnit> EnemyClass = EnemyDefinition->EnemyUnitClasses[0];
    if (!EnemyClass) return false;
    AUnitBase* Enemy = SpawnConfiguredDebugUnit(true, FName(*EnemyClass->GetPathName()), Arena->Grid->GetTileAtCoord(Arena->EnemyCoords[0]), StatusMessage);
    if (!IsValid(Enemy)) return false;
    SpawnedUnits.Add(Enemy);
    return true;
}

TArray<TSubclassOf<AEnemyUnit>> ACombatDebugGameMode::GetAvailableEnemyClasses() const
{
    TArray<TSubclassOf<AEnemyUnit>> Classes;
    if (!IsValid(EnemyDefinition) || !EnemyDefinition->OpponentSnapshotSlot.IsNone()) return Classes;
    // Load soft catalog entries only for debug selection and retain valid initial classes in the union.
    // 디버그 선택에만 소프트 카탈로그를 로드하고 유효한 초기 편성 클래스도 합집합에 보존합니다.
    for (const TSoftClassPtr<AEnemyUnit>& Candidate : EnemyDefinition->EnemyCatalogClasses)
    {
        if (Candidate.IsNull()) continue;
        UClass* Resolved = Candidate.LoadSynchronous();
        if (IsUsableEnemyClass(Resolved)) Classes.AddUnique(Resolved);
    }
    for (TSubclassOf<AEnemyUnit> Candidate : EnemyDefinition->EnemyUnitClasses)
    {
        if (IsUsableEnemyClass(Candidate.Get())) Classes.AddUnique(Candidate);
    }
    return Classes;
}

void ACombatDebugGameMode::GetDebugSpawnOptions(bool bEnemy, TArray<FName>& OutIds, TArray<FText>& OutNames) const
{
    OutIds.Reset();
    OutNames.Reset();
    if (bEnemy)
    {
        for (TSubclassOf<AEnemyUnit> EnemyClass : GetAvailableEnemyClasses())
        {
            const FName Id(*EnemyClass->GetPathName());
            const AEnemyUnit* Defaults = EnemyClass->GetDefaultObject<AEnemyUnit>();
            FString Name = EnemyClass->GetName();
            Name.RemoveFromEnd(TEXT("_C"));
            OutIds.Add(Id);
            OutNames.Add(Defaults->RuntimeCharacterName.IsEmpty() ? FText::FromString(Name) : Defaults->RuntimeCharacterName);
        }
        return;
    }
    if (!IsValid(PartyDefinition)) return;
    TArray<FName> ClassIds;
    PartyDefinition->Professions.GetKeys(ClassIds);
    ClassIds.Sort(FNameLexicalLess());
    for (FName ClassId : ClassIds)
    {
        FProfessionDefinition Profession;
        if (!PartyDefinition->ResolveProfession(ClassId, Profession)) continue;
        OutIds.Add(ClassId);
        OutNames.Add(Profession.DisplayName.IsEmpty() ? FText::FromName(ClassId) : Profession.DisplayName);
    }
}

bool ACombatDebugGameMode::CanSpawnDebugUnit(APlayerController* Controller, bool bEnemy, FText& OutError) const
{
    OutError = FText::GetEmpty();
    const URunStateSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (!HasAuthority() || !IsDebugWorld(GetWorld()) || (Run && (Run->IsManagedRun() || Run->HasManagedLease())) || !IsValid(CombatManager) || !IsValid(CombatManager->GetRoundCoordinator()))
    {
        OutError = FText::FromString(TEXT("준비된 독립 디버그 전투에서만 캐릭터를 추가할 수 있습니다."));
        return false;
    }
    if ((bEnemy && !IsValid(EnemyDefinition)) || (!bEnemy && !IsValid(PartyDefinition)))
    {
        OutError = FText::FromString(TEXT("추가할 캐릭터의 데이터 정의가 없습니다."));
        return false;
    }
    return CombatManager->GetRoundCoordinator()->CanAddDebugUnit(Controller, bEnemy, OutError);
}

AUnitBase* ACombatDebugGameMode::SpawnConfiguredDebugUnit(bool bEnemy, FName OptionId, ACombatGridTile* Tile, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (!IsValid(Tile) || !IsValid(Arena) || !IsValid(Arena->Grid) || Tile->GetWorld() != GetWorld() || Arena->Grid->GetTileAtCoord(Tile->GridCoord) != Tile || Tile->GetOccupyingUnit() || Tile->GetTerritory() != (bEnemy ? ETileTerritory::Enemy : ETileTerritory::Player))
    {
        OutError = FText::FromString(TEXT("배치할 진영에 유효한 빈 칸이 없습니다."));
        return nullptr;
    }
    TSubclassOf<AUnitBase> UnitClass;
    FProfessionDefinition Profession;
    TArray<TObjectPtr<USkillDefinitionDataAsset>> StartingSkills;
    if (bEnemy)
    {
        // Initial debug spawning reuses hard references without loading unselected catalog monsters.
        // 초기 디버그 생성은 선택되지 않은 카탈로그 몬스터를 로드하지 않고 기존 직접 참조를 사용합니다.
        if (IsValid(EnemyDefinition) && EnemyDefinition->OpponentSnapshotSlot.IsNone())
        {
            for (TSubclassOf<AEnemyUnit> Candidate : EnemyDefinition->EnemyUnitClasses)
            {
                if (IsUsableEnemyClass(Candidate.Get()) && FName(*Candidate->GetPathName()) == OptionId)
                {
                    UnitClass = Candidate;
                    break;
                }
            }
        }
        if (!UnitClass)
        {
            for (TSubclassOf<AEnemyUnit> Candidate : GetAvailableEnemyClasses())
            {
                if (FName(*Candidate->GetPathName()) == OptionId)
                {
                    UnitClass = Candidate;
                    break;
                }
            }
        }
    }
    else
    {
        if (!IsValid(PartyDefinition) || !PartyDefinition->ResolveProfession(OptionId, Profession, OutError) || !PartyDefinition->ResolveStartingSkills(OptionId, StartingSkills, OutError)) return nullptr;
        UnitClass = Profession.CombatClass;
    }
    if (!UnitClass || UnitClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
    {
        OutError = FText::FromString(TEXT("등록된 목록에서 생성 가능한 캐릭터를 선택하세요."));
        return nullptr;
    }
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AUnitBase* Unit = GetWorld()->SpawnActor<AUnitBase>(UnitClass, Tile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, bEnemy ? -90.f : 90.f, 0.f), Params);
    if (!IsValid(Unit))
    {
        OutError = FText::FromString(TEXT("캐릭터 액터 생성에 실패했습니다."));
        return nullptr;
    }
    // Configure disposable actors before claiming a tile or joining the existing combat roster.
    // 기존 전투 명단이나 타일을 변경하기 전에 일회성 액터의 구성을 완료합니다.
    if (!bEnemy)
    {
        APlayerUnit* Player = Cast<APlayerUnit>(Unit);
        if (!Player || !Player->CharacterAppearance || !Player->CharacterAppearance->SetAppearance(Profession.AppearanceCatalog, FCharacterAppearanceSelection()) || !Player->ConfigureProfession(Profession.MaxHP, Profession.ActionPoints, Profession.SubActionPoints, StartingSkills, Profession.Speed) || !Player->CharacterEquipment || !Player->CharacterEquipment->SetEquipment(true, TArray<FRunEquipmentVisual>()))
        {
            OutError = FText::FromString(TEXT("캐릭터 외형·능력치·시작 스킬 구성에 실패했습니다."));
            Unit->Destroy();
            return nullptr;
        }
        Player->RuntimeCharacterName = FText::Format(NSLOCTEXT("CombatDebug", "ProfessionName", "디버그 {0}"), Profession.DisplayName);
    }
    // Apply debug health after class and profession initialization without changing shared definitions.
    // 공용 정의를 변경하지 않고 클래스·직업 초기화가 끝난 뒤 디버그 체력을 적용합니다.
    UAbilitySystemComponent* AbilitySystem = Unit->GetAbilitySystemComponent();
    if (!AbilitySystem || !Unit->GetAttributeSet())
    {
        OutError = FText::FromString(TEXT("캐릭터의 디버그 체력 초기화에 필요한 능력치가 없습니다."));
        Unit->Destroy();
        return nullptr;
    }
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetMaxHPAttribute(), DebugMaxHP);
    AbilitySystem->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), DebugMaxHP);
    Unit->SetTeam(bEnemy ? ETeam::Enemy : ETeam::Player);
    Unit->SetActorLocation(Tile->GetActorLocation() + FVector(0.f, 0.f, Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
    Unit->SetCurrentTile(Tile);
    return Unit;
}

bool ACombatDebugGameMode::SpawnDebugUnit(APlayerController* Controller, bool bEnemy, FName OptionId, int32& OutUnitId, FText& OutError)
{
    OutUnitId = INDEX_NONE;
    if (!CanSpawnDebugUnit(Controller, bEnemy, OutError)) return false;
    ACombatRoundCoordinator* Round = CombatManager->GetRoundCoordinator();
    ACombatGridTile* Tile = Round->FindDebugSpawnTile(bEnemy);
    AUnitBase* Unit = SpawnConfiguredDebugUnit(bEnemy, OptionId, Tile, OutError);
    if (!Unit) return false;
    if (!Round->AddDebugUnit(Controller, Unit, OutError))
    {
        if (IsValid(Tile) && Tile->GetOccupyingUnit() == Unit) Tile->SetOccupyingUnit(nullptr);
        Unit->Destroy();
        return false;
    }
    SpawnedUnits.Add(Unit);
    OutUnitId = Unit->UnitIndex;
    StatusMessage = Round->GetView().Message;
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
