#include "Game/Encounter/EncounterManager.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Combat/CombatManager.h"
#include "Combat/SkillActor/SkillActorBase.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/EncounterDefinitionDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GAS/Attribute/AS_Unit.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "TimerManager.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"

AEncounterManager::AEncounterManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEncounterManager::InitializeEncounter(ACombatArena* InArena, ACombatManager* InCombatManager, UPartyDefinitionDataAsset* InPartyDefinition, const TMap<FName, TObjectPtr<UEncounterDefinitionDataAsset>>& InDefinitions)
{
    Arena = InArena;
    CombatManager = InCombatManager;
    PartyDefinition = InPartyDefinition;
    Definitions = InDefinitions;
    RunState = GetGameInstance()->GetSubsystem<URunStateSubsystem>();
    if (RunState->PartyDefinition)
    {
        PartyDefinition = RunState->PartyDefinition;
    }
    if (CombatManager)
    {
        CombatManager->OnCombatResult.AddUObject(this, &AEncounterManager::HandleCombatResult);
    }
    if (Arena)
    {
        Arena->CleanupArena();
    }
    SetPlayerCombatInput(false);
}

bool AEncounterManager::RequestStartNode(FName NodeId)
{
    if (!HasAuthority() || bPreparing || PendingResult != ECombatResult::None || !RunState || !RunState->CanStartNode(NodeId))
    {
        return false;
    }
    bPreparing = true;
    FlowMessage = FText::GetEmpty();
    SetPlayerCombatInput(false);
    if (!RunState->BeginEncounter(NodeId))
    {
        bPreparing = false;
        return false;
    }
    OnFlowChanged.Broadcast();
    if (!Arena || !CombatManager || !PartyDefinition)
    {
        return FailPreparation(FText::FromString(TEXT("Gameplay setup is incomplete. Check Arena and PartyDefinition. / Gameplay 설정을 확인하세요.")));
    }
    FText ArenaError;
    if (!Arena->PrepareArena(ArenaError))
    {
        return FailPreparation(ArenaError);
    }
    const TObjectPtr<UEncounterDefinitionDataAsset>* Definition = Definitions.Find(RunState->GetCurrentEncounterId());
    if (!Definition || !IsValid(*Definition) || !SpawnEncounter(*Definition))
    {
        return FailPreparation(FText::FromString(TEXT("Encounter spawn failed. Check unit classes and spawn coordinates. / 유닛 클래스와 스폰 좌표를 확인하세요.")));
    }
    CombatManager->SetCombatGrid(Arena->Grid);
    TArray<AUnitBase*> Units;
    for (AUnitBase* Unit : SpawnedUnits)
    {
        Units.Add(Unit);
    }
    CombatManager->RegisterUnits(Units);
    Arena->ActivateArena(GetWorld()->GetFirstPlayerController());
    RunState->MarkCombatStarted();
    bPreparing = false;
    CombatManager->StartCombat_Internal();
    if (!CombatManager->IsCombatActive())
    {
        if (PendingResult != ECombatResult::None)
        {
            return true;
        }
        return FailPreparation(FText::FromString(TEXT("Combat could not start. / 전투를 시작할 수 없습니다.")));
    }
    SetPlayerCombatInput(true);
    UE_LOG(LogTemp, Log, TEXT("[Encounter] Started Node=%s Units=%d"), *NodeId.ToString(), SpawnedUnits.Num());
    OnFlowChanged.Broadcast();
    return true;
}

bool AEncounterManager::SpawnEncounter(UEncounterDefinitionDataAsset* Definition)
{
    if (Definition->EnemyUnitClasses.IsEmpty() || Definition->EnemyUnitClasses.Num() > Arena->EnemyCoords.Num())
    {
        return false;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = this;
    for (const FRunPartyMember& Member : RunState->GetPartyMembers())
    {
        if (!Member.bCreated || Member.CurrentHP == 0.f)
        {
            continue;
        }
        if (!Arena->PlayerCoords.IsValidIndex(Member.SlotIndex))
        {
            return false;
        }
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Arena->PlayerCoords[Member.SlotIndex]);
        FProfessionDefinition Profession;
        if (!PartyDefinition->ResolveProfession(Member.ClassId, Profession))
        {
            return false;
        }
        TSubclassOf<APlayerUnit> UnitClass = Profession.CombatClass;
        if (!Tile || Tile->GetOccupyingUnit() || Tile->GetTerritory() != ETileTerritory::Player || !UnitClass)
        {
            return false;
        }
        APlayerUnit* Unit = GetWorld()->SpawnActor<APlayerUnit>(UnitClass, Tile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, 90.f, 0.f), Params);
        if (!Unit)
        {
            return false;
        }
        SpawnedUnits.Add(Unit);
        PartyActors.Add(Member.SlotIndex, Unit);
        if (!Unit->ConfigureProfession(Profession.MaxHP, Profession.ActionPoints, Profession.SubActionPoints, Profession.StartingSkills))
        {
            return false;
        }
        Unit->RuntimeCharacterName = Member.CharacterName;
        Unit->SetTeam(ETeam::Player);
        Unit->SetCurrentTile(Tile);
        if (Member.CurrentHP >= 0.f && Unit->GetAttributeSet())
        {
            Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(UAS_Unit::GetHPAttribute(), FMath::Clamp(Member.CurrentHP, 0.f, Unit->GetAttributeSet()->GetMaxHP()));
        }
    }
    if (PartyActors.IsEmpty())
    {
        return false;
    }
    for (int32 Index = 0; Index < Definition->EnemyUnitClasses.Num(); ++Index)
    {
        ACombatGridTile* Tile = Arena->Grid->GetTileAtCoord(Arena->EnemyCoords[Index]);
        if (!Tile || Tile->GetOccupyingUnit() || Tile->GetTerritory() != ETileTerritory::Enemy || !Definition->EnemyUnitClasses[Index])
        {
            return false;
        }
        AEnemyUnit* Unit = GetWorld()->SpawnActor<AEnemyUnit>(Definition->EnemyUnitClasses[Index], Tile->GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator(0.f, -90.f, 0.f), Params);
        if (!Unit)
        {
            return false;
        }
        SpawnedUnits.Add(Unit);
        Unit->SetTeam(ETeam::Enemy);
        Unit->SetCurrentTile(Tile);
    }
    return true;
}

void AEncounterManager::HandleCombatResult(ECombatResult Result)
{
    if (PendingResult != ECombatResult::None || !RunState || RunState->GetPhase() != ERunPhase::Combat || Result == ECombatResult::None)
    {
        return;
    }
    PendingResult = Result;
    SetPlayerCombatInput(false);
    // Defer destruction until damage, death and ability callbacks have unwound.
    // 피해, 사망, 어빌리티 콜백이 반환된 다음에 액터를 정리합니다.
    FinishTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AEncounterManager::FinishEncounter);
}

void AEncounterManager::FinishEncounter()
{
    for (const TPair<int32, TObjectPtr<AUnitBase>>& Entry : PartyActors)
    {
        if (IsValid(Entry.Value) && Entry.Value->GetAttributeSet())
        {
            float HP = Entry.Value->GetAttributeSet()->GetHP();
            if (!Entry.Value->IsUnitAlive())
            {
                HP = 0.f;
            }
            RunState->UpdatePartyMemberHP(Entry.Key, HP);
        }
    }
    CleanupEncounter();
    const ECombatResult Result = PendingResult;
    PendingResult = ECombatResult::None;
    RunState->CompleteEncounter(Result);
    UE_LOG(LogTemp, Log, TEXT("[Encounter] Finished Result=%d RemainingUnits=%d"), static_cast<int32>(Result), SpawnedUnits.Num());
    OnFlowChanged.Broadcast();
}

bool AEncounterManager::ContinueRun()
{
    if (!RunState || PendingResult != ECombatResult::None)
    {
        return false;
    }
    return RunState->ContinueRun();
}

bool AEncounterManager::FailPreparation(const FText& Message)
{
    FlowMessage = Message;
    CleanupEncounter();
    bPreparing = false;
    RunState->AbortEncounter();
    UE_LOG(LogTemp, Error, TEXT("[Encounter] %s"), *Message.ToString());
    OnFlowChanged.Broadcast();
    return false;
}

void AEncounterManager::SetPlayerCombatInput(bool bEnabled)
{
    APartyPlayerController* Controller = Cast<APartyPlayerController>(GetWorld()->GetFirstPlayerController());
    if (Controller)
    {
        Controller->SetCombatContext(CombatManager, bEnabled);
    }
}

void AEncounterManager::CleanupEncounter()
{
    SetPlayerCombatInput(false);
    if (CombatManager)
    {
        CombatManager->ResetCombat();
    }
    for (TActorIterator<ASkillActorBase> It(GetWorld()); It; ++It)
    {
        if (SpawnedUnits.Contains(It->GetSourceUnit()))
        {
            It->Destroy();
        }
    }
    for (AUnitBase* Unit : SpawnedUnits)
    {
        if (IsValid(Unit))
        {
            Unit->OnTurnEnd();
            Unit->CancelCurrentAction();
            Unit->SetCurrentTile(nullptr);
            AController* Controller = Unit->GetController();
            if (Controller)
            {
                Controller->UnPossess();
                Controller->Destroy();
            }
            Unit->Destroy();
        }
    }
    SpawnedUnits.Reset();
    PartyActors.Reset();
    if (Arena)
    {
        Arena->CleanupArena();
    }
}

void AEncounterManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(FinishTimer);
    if (CombatManager)
    {
        CombatManager->OnCombatResult.RemoveAll(this);
    }
    CleanupEncounter();
    OnFlowChanged.Clear();
    Super::EndPlay(EndPlayReason);
}
