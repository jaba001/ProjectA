#include "BalanceSimCommandlet.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Encounter/EncounterManager.h"
#include "Game/GameModes/GameplayGameModeBase.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunStateSubsystem.h"
#include "GAS/Attribute/AS_Unit.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Profession/ProfessionBase.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Types/ProjectARandom.h"
#include "UI/MainMenu/CharacterPartyDraft.h"
#include "Unit/UnitBase.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogBalanceSim, Log, All);

namespace BalanceSim
{
    constexpr float TickSeconds = 1.f / 60.f;

    struct FOptions
    {
        int32 Players = 1;
        int32 Repeats = 1;
        int32 Seed = 1;
        int32 MaxSteps = 80;
        float TimeScale = 20.f;
        float MaxSeconds = 7200.f;
        FString Difficulty = TEXT("Medium");
    };

    struct FCombatRecord
    {
        int32 Number = 0;
        int32 Rounds = 0;
        int32 Survivors = 0;
        int64 GoldBefore = 0;
        float HP = 0.f;
        float MaxHP = 0.f;
        FString Kind;
        FString Difficulty;
        FString Enemies;
        bool bFinished = false;
    };

    FString Cell(FString Value)
    {
        Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return TEXT("\"") + Value + TEXT("\"");
    }

    bool Append(const FString& Path, const TArray<FString>& Cells)
    {
        TArray<FString> Escaped;
        for (const FString& Value : Cells) Escaped.Add(Cell(Value));
        return FFileHelper::SaveStringToFile(FString::Join(Escaped, TEXT(",")) + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    }

    int64 Gold(const URunStateSubsystem& Run)
    {
        int64 Total = 0;
        for (const FRunPartyMember& Member : Run.GetPartyMembers()) if (Member.bCreated) Total += Member.Gold;
        return Total;
    }

    int32 Steps(const URunStateSubsystem& Run)
    {
        return Run.GetCompletedNodes().Num() + Run.GetTargetRunState().CompletedEncounterChoices.Num();
    }

    void EquipNewItems(URunStateSubsystem& Run, AEncounterManager& Encounter, FGuid CharacterId, int32 FirstItem)
    {
        const FRunPartyMember* Member = Run.GetPartyMembers().FindByPredicate([CharacterId](const FRunPartyMember& Value) { return Value.CharacterId == CharacterId; });
        if (!Member) return;
        const int32 ItemCount = Member->Items.Num();
        for (int32 Index = FirstItem; Index < ItemCount; ++Index)
        {
            Member = Run.GetPartyMembers().FindByPredicate([CharacterId](const FRunPartyMember& Value) { return Value.CharacterId == CharacterId; });
            if (!Member || !Member->Items.IsValidIndex(Index)) return;
            const FRunEquipmentProfile* Profile = URunEquipmentCatalog::Get().ResolveProfile(Member->Items[Index]);
            if (!Profile) continue;
            FText Error;
            FRunEquipmentCommand Command;
            Command.CharacterId = CharacterId;
            Command.ItemIndex = Index;
            Command.TargetSlot = Profile->PreferredSlot;
            Command.ExpectedRevision = Member->Equipment.Revision;
            // Prefer an empty legal slot, then the profile's normal preferred slot.
            // 적법한 빈 슬롯을 우선하고 없으면 프로필의 기본 선호 슬롯을 사용합니다.
            for (FGameplayTag Slot : URunEquipmentCatalog::GetSlotTags())
            {
                if (!Command.TargetSlot.IsValid() && RunEquipmentRules::CanDrop(*Member, Index, Slot, Error)) Command.TargetSlot = Slot;
                if (RunEquipmentRules::FindItemIndexAtSlot(*Member, Slot) == INDEX_NONE && RunEquipmentRules::CanDrop(*Member, Index, Slot, Error))
                {
                    Command.TargetSlot = Slot;
                    break;
                }
            }
            Encounter.ChangeEquipment(Member->OwnerAccountId, Command, Error);
        }
    }

    void Shop(URunStateSubsystem& Run, AEncounterManager& Encounter, FRandomStream& Policy)
    {
        const FRunEncounterOffer* Selected = Run.GetEncounterProgress().FindSelectedOffer();
        if (!Selected) return;
        const FRunEncounterOffer Offer = *Selected;
        // Ordinary single-player gold and purchases remain limited to its selected character.
        // 일반 싱글플레이의 골드와 구매는 기존 선택 캐릭터 제한을 유지합니다.
        const TArray<FRunPartyMember> Members = Run.GetPartyMembers();
        for (const FRunPartyMember& Original : Members)
        {
            if (!Original.bCreated || !Original.bPlayerControlled) continue;
            FText Error;
            EquipNewItems(Run, Encounter, Original.CharacterId, 0);
            if (Offer.IsService())
            {
                Run.PurchaseRecoveryOffer(Original.OwnerAccountId, Original.CharacterId, Offer.GetResolvedTag().GetTagName(), Error, Run.GetRecoveryState().Revision);
                continue;
            }
            TArray<FRunItemShopOffer> Offers = Run.GetItemShopState().Offers;
            for (int32 Index = Offers.Num() - 1; Index > 0; --Index) Offers.Swap(Index, Policy.RandRange(0, Index));
            for (const FRunItemShopOffer& Stock : Offers)
            {
                const FRunPartyMember* Current = Run.GetPartyMembers().FindByPredicate([&Original](const FRunPartyMember& Value) { return Value.CharacterId == Original.CharacterId; });
                if (!Current || Stock.bSold || Stock.Item.Price > Current->Gold || !URunEquipmentCatalog::Get().ResolveProfile(Stock.Item)) continue;
                const int32 FirstItem = Current->Items.Num();
                if (Encounter.PurchaseShopOffer(Current->OwnerAccountId, Current->CharacterId, Stock.OfferId, Error, Run.GetItemShopState().Revision)) EquipNewItems(Run, Encounter, Original.CharacterId, FirstItem);
            }
        }
    }

    bool Rewards(URunStateSubsystem& Run, AEncounterManager& Encounter, FRandomStream& Policy, FText& Error)
    {
        const TArray<FGuid> Recipients = Run.GetGoldRewardRecipientIds();
        for (FGuid CharacterId : Recipients)
        {
            const FRunGoldRewardState& Reward = Run.GetGoldRewardState();
            if (Reward.Claims.ContainsByPredicate([CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; })) continue;
            const FRunPartyMember* Member = Run.GetPartyMembers().FindByPredicate([CharacterId](const FRunPartyMember& Value) { return Value.CharacterId == CharacterId; });
            const int32 Count = Reward.SchemaVersion == 2 ? Reward.ItemChoices.Num() : Reward.GoldChoices.Num();
            if (!Member || Count <= 0) return false;
            if (!Encounter.SelectGoldReward(Member->OwnerAccountId, CharacterId, Reward.NodeId, Policy.RandRange(0, Count - 1), Error)) return false;
        }
        return Run.CanContinueAfterRewards();
    }

    bool RunOnce(const FOptions& Options, int32 Seed, const FString& Output)
    {
        ProjectARandom::FScopedBalanceSimulation Scope(Seed);
        UWorld* PreviousWorld = GWorld;
        ON_SCOPE_EXIT { GWorld = PreviousWorld; };
        GWorld = nullptr;
        FRandomStream Policy(static_cast<int32>(static_cast<uint32>(Seed) ^ 0x504f4c49u));
        TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
        Instance->InitializeStandalone();
        FWorldContext* Context = Instance->GetWorldContext();
        ON_SCOPE_EXIT
        {
            if (UWorld* World = Context->World())
            {
                World->BeginTearingDown();
                if (AGameplayGameModeBase* Mode = World->GetAuthGameMode<AGameplayGameModeBase>()) if (AEncounterManager* Encounter = Mode->GetEncounterManager()) Encounter->ShutdownGameplay();
                World->DestroyWorld(false);
                Instance->Shutdown();
                GEngine->DestroyWorldContext(World);
            }
            else Instance->Shutdown();
            GWorld = nullptr;
            Instance.Reset();
            CollectGarbage(RF_NoFlags);
        };
        const double Started = FPlatformTime::Seconds();
        FText Error;
        FString LoadError;
        FString Outcome = TEXT("Error");
        FCombatRecord Combat;
        URunStateSubsystem* Run = Instance->GetSubsystem<URunStateSubsystem>();
        Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
        FCharacterPartyDraft Draft;
        Draft.Reset(UProfessionBase::GetPlayableIds(), false);
        for (int32 Index = 0; Index < Options.Players; ++Index)
        {
            Draft.Create(Index);
            Draft.SetName(Index, FText::FromString(FString::Printf(TEXT("BalanceSim_%d"), Index + 1)));
        }
        Draft.SelectControlled(0);
        // Validate normal checkpoint transactions, but the scoped storage sink never touches a save file.
        // 정상 체크포인트 트랜잭션을 검증하되 범위 내 저장 대상은 저장 파일에 접근하지 않습니다.
        Run->EnableCheckpointSaving(TEXT("ProjectA_BalanceSim_MemoryOnly"));
        FURL URL(nullptr, TEXT("/Game/User_JeHoon/LEVEL/Core/Gameplay"), TRAVEL_Absolute);
        if (Run->PartyDefinition && Run->InitializeTargetRun(Draft.ExportParty(), Error) && GEngine->LoadMap(*Context, URL, nullptr, LoadError))
        {
            UWorld* World = Context->World();
            GWorld = World;
            AEncounterManager* Encounter = nullptr;
            FDelegateHandle ResultHandle;
            ON_SCOPE_EXIT
            {
                if (Encounter && Encounter->GetCombatManager()) Encounter->GetCombatManager()->OnCombatResult.Remove(ResultHandle);
            };
            while (!IsEngineExitRequested())
            {
                const double FrameStarted = FPlatformTime::Seconds();
                if (FrameStarted - Started > Options.MaxSeconds)
                {
                    Outcome = TEXT("Timeout");
                    break;
                }
                if (!Encounter)
                {
                    if (AGameplayGameModeBase* Mode = World->GetAuthGameMode<AGameplayGameModeBase>()) Encounter = Mode->GetEncounterManager();
                    if (Encounter && Encounter->GetCombatManager())
                    {
                        ResultHandle = Encounter->GetCombatManager()->OnCombatResult.AddLambda([&Combat, Encounter](ECombatResult Result)
                        {
                            Combat.bFinished = Result != ECombatResult::None;
                            Combat.HP = 0.f;
                            Combat.Survivors = 0;
                            if (const ACombatRoundCoordinator* Round = Encounter->GetCombatManager()->GetRoundCoordinator()) Combat.Rounds = FMath::Max(Combat.Rounds, Round->GetView().RoundNumber);
                            for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
                            {
                                if (!IsValid(Unit) || Unit->GetTeam() != ETeam::Player || !Unit->GetAttributeSet()) continue;
                                if (Unit->IsUnitAlive())
                                {
                                    Combat.HP += Unit->GetAttributeSet()->GetHP();
                                    ++Combat.Survivors;
                                }
                            }
                        });
                    }
                }
                if (Encounter)
                {
                    if (const ACombatRoundCoordinator* Round = Encounter->GetCombatManager()->GetRoundCoordinator()) Combat.Rounds = FMath::Max(Combat.Rounds, Round->GetView().RoundNumber);
                    const ERunPhase Phase = Run->GetPhase();
                    if ((Phase == ERunPhase::Result || Phase == ERunPhase::Defeat) && Combat.bFinished)
                    {
                        if (Phase == ERunPhase::Result && !Rewards(*Run, *Encounter, Policy, Error)) break;
                        const int64 Earned = Gold(*Run) - Combat.GoldBefore;
                        if (!Append(Output / TEXT("combats.csv"), {LexToString(Seed), LexToString(Combat.Number), Combat.Kind, Combat.Difficulty, LexToString((Combat.Number + 1) / 2), Combat.Enemies, Phase == ERunPhase::Defeat ? TEXT("Defeat") : TEXT("Victory"), LexToString(Combat.Rounds), LexToString(Combat.HP), LexToString(Combat.MaxHP), LexToString(Combat.Survivors), LexToString(Earned)})) break;
                        Combat.bFinished = false;
                    }
                    if (Phase == ERunPhase::Defeat || Phase == ERunPhase::Complete)
                    {
                        Outcome = Phase == ERunPhase::Complete ? TEXT("Complete") : TEXT("Defeat");
                        break;
                    }
                    if (Steps(*Run) >= Options.MaxSteps)
                    {
                        // The final result still passes through normal Continue before declaring completion.
                        // 마지막 결과도 정상 Continue를 거친 뒤에만 완주로 기록합니다.
                        if (Phase == ERunPhase::Result && Run->GetCompletedNodes().Num() == Run->GetNodes().Num())
                        {
                            if (!Encounter->ContinueRun()) break;
                            continue;
                        }
                        Outcome = TEXT("StepLimit");
                        break;
                    }
                    if (Phase == ERunPhase::EncounterChoice)
                    {
                        const TArray<FRunEncounterOffer>& Offers = Run->GetEncounterProgress().Offers;
                        if (Offers.IsEmpty() || !Encounter->SelectRunEncounter(Offers[Policy.RandRange(0, Offers.Num() - 1)].EncounterId)) break;
                    }
                    else if (Phase == ERunPhase::Shop)
                    {
                        Shop(*Run, *Encounter, Policy);
                        if (!Encounter->LeaveRunEncounter()) break;
                    }
                    else if (Phase == ERunPhase::Map)
                    {
                        const int32 Index = Run->GetCompletedNodes().Num();
                        if (!Run->GetNodes().IsValidIndex(Index)) break;
                        const bool bPve = Index % 2 == 0;
                        const int32 Choice = Options.Difficulty == TEXT("Random") && bPve ? Policy.RandRange(0, 2) : Options.Difficulty == TEXT("Low") ? 0 : Options.Difficulty == TEXT("High") ? 2 : 1;
                        const TArray<FGameplayTag> Tags = {RunPveDifficulty::GetLowTag(), RunPveDifficulty::GetMediumTag(), RunPveDifficulty::GetHighTag()};
                        Combat = FCombatRecord();
                        Combat.Number = Index + 1;
                        Combat.Kind = bPve ? TEXT("PvE") : TEXT("Snapshot");
                        Combat.Difficulty = bPve ? Tags[Choice].ToString() : TEXT("N/A");
                        Combat.GoldBefore = Gold(*Run);
                        for (const FRunPartyMember& Member : Run->GetPartyMembers())
                        {
                            if (!Member.bCreated) continue;
                            FProfessionDefinition Profession;
                            if (Run->ResolveMemberProfession(Member, Profession, Error)) Combat.MaxHP += Profession.MaxHP;
                        }
                        if (!Encounter->RequestStartNode(Run->GetNodes()[Index].NodeId, bPve ? Tags[Choice] : FGameplayTag())) break;
                        TArray<FString> Enemies;
                        for (AUnitBase* Unit : Encounter->GetSpawnedUnits())
                        {
                            if (!IsValid(Unit)) continue;
                            // Refresh server collision bones even when NullRHI never renders the character.
                            // NullRHI에서 캐릭터가 렌더링되지 않아도 서버 충돌 뼈를 갱신합니다.
                            if (USkeletalMeshComponent* Mesh = Unit->GetMesh()) Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                            if (Unit->GetTeam() == ETeam::Enemy) Enemies.Add(Unit->RuntimeCharacterName.ToString() + TEXT("[") + Unit->GetClass()->GetPathName() + TEXT("]"));
                        }
                        Combat.Enemies = FString::Join(Enemies, TEXT(";"));
                    }
                    else if (Phase == ERunPhase::Result)
                    {
                        if (!Encounter->ContinueRun()) break;
                    }
                    if (Encounter->CanRetryCombatCheckpoint()) break;
                }
                // Keep animation, collision, timers and AP/SAP on the same fixed world step at every speed.
                // 모든 속도에서 애니메이션·충돌·타이머·AP/SAP의 고정 월드 간격을 유지합니다.
                ++GFrameCounter;
                FApp::SetDeltaTime(TickSeconds);
                FTSTicker::GetCoreTicker().Tick(TickSeconds);
                World->Tick(LEVELTICK_All, TickSeconds);
                const double Wait = TickSeconds / Options.TimeScale - (FPlatformTime::Seconds() - FrameStarted);
                if (Wait > 0.0) FPlatformProcess::SleepNoStats(static_cast<float>(Wait));
            }
        }
        const bool bTerminal = Outcome == TEXT("Complete") || Outcome == TEXT("Defeat");
        const bool bWritten = Append(Output / TEXT("runs.csv"), {LexToString(Seed), LexToString(Options.Players), Options.Difficulty, Outcome, Outcome == TEXT("Defeat") ? LexToString(Combat.Number) : TEXT("0"), LexToString(Steps(*Run)), LexToString(Gold(*Run)), LexToString(FPlatformTime::Seconds() - Started)});
        if (!bTerminal)
        {
            UE_LOG(LogBalanceSim, Error, TEXT("Seed=%d outcome=%s steps=%d error=%s load=%s save=%s"), Seed, *Outcome, Steps(*Run), *Error.ToString(), *LoadError, *Run->GetSaveError().ToString());
        }
        else
        {
            UE_LOG(LogBalanceSim, Display, TEXT("Seed=%d outcome=%s steps=%d combat=%d"), Seed, *Outcome, Steps(*Run), Combat.Number);
        }
        return bTerminal && bWritten;
    }
}

UBalanceSimCommandlet::UBalanceSimCommandlet()
{
    IsClient = true;
    IsServer = true;
    IsEditor = true;
    LogToConsole = true;
}

int32 UBalanceSimCommandlet::Main(const FString& Params)
{
    BalanceSim::FOptions Options;
    FParse::Value(*Params, TEXT("Players="), Options.Players);
    FParse::Value(*Params, TEXT("Repeats="), Options.Repeats);
    FParse::Value(*Params, TEXT("Seed="), Options.Seed);
    FParse::Value(*Params, TEXT("MaxSteps="), Options.MaxSteps);
    FParse::Value(*Params, TEXT("TimeScale="), Options.TimeScale);
    FParse::Value(*Params, TEXT("MaxSeconds="), Options.MaxSeconds);
    FParse::Value(*Params, TEXT("Difficulty="), Options.Difficulty);
    if (Options.Difficulty.Equals(TEXT("Low"), ESearchCase::IgnoreCase)) Options.Difficulty = TEXT("Low");
    else if (Options.Difficulty.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) Options.Difficulty = TEXT("Medium");
    else if (Options.Difficulty.Equals(TEXT("High"), ESearchCase::IgnoreCase)) Options.Difficulty = TEXT("High");
    else if (Options.Difficulty.Equals(TEXT("Random"), ESearchCase::IgnoreCase)) Options.Difficulty = TEXT("Random");
    else return 1;
    if (!GEngine || !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) || Options.Players < 1 || Options.Players > 4 || Options.Repeats < 1 || Options.Repeats > 100000 || Options.MaxSteps < 1 || Options.MaxSteps > 80 || !FMath::IsFinite(Options.TimeScale) || Options.TimeScale < 1.f || Options.TimeScale > 1000.f || !FMath::IsFinite(Options.MaxSeconds) || Options.MaxSeconds <= 0.f)
    {
        UE_LOG(LogBalanceSim, Error, TEXT("Require -nullrhi Players=1..4 Difficulty=Low|Medium|High|Random Repeats=1..100000 Seed=int32 TimeScale=1..1000 MaxSteps=1..80 MaxSeconds>0."));
        return 1;
    }
    const FString Output = FPaths::ProjectSavedDir() / TEXT("BalanceSim") / (FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    if (!IFileManager::Get().MakeDirectory(*Output, true)) return 1;
    if (!BalanceSim::Append(Output / TEXT("runs.csv"), {TEXT("seed"), TEXT("players"), TEXT("difficulty_policy"), TEXT("result"), TEXT("defeat_combat"), TEXT("progress_steps"), TEXT("final_gold"), TEXT("real_seconds")})) return 1;
    if (!BalanceSim::Append(Output / TEXT("combats.csv"), {TEXT("seed"), TEXT("combat_number"), TEXT("kind"), TEXT("difficulty"), TEXT("group"), TEXT("enemies"), TEXT("result"), TEXT("rounds"), TEXT("ally_hp"), TEXT("ally_max_hp"), TEXT("surviving_allies"), TEXT("gold_earned")})) return 1;
    UE_LOG(LogBalanceSim, Display, TEXT("Output=%s; policy=v1; fixed tick=1/60; TimeScale=%g; MaxSteps=%d; MaxSeconds=%g"), *FPaths::ConvertRelativePathToFull(Output), Options.TimeScale, Options.MaxSteps, Options.MaxSeconds);
    const double PreviousDelta = FApp::GetDeltaTime();
    ON_SCOPE_EXIT { FApp::SetDeltaTime(PreviousDelta); };
    bool bSucceeded = true;
    for (int32 Index = 0; Index < Options.Repeats && !IsEngineExitRequested(); ++Index)
    {
        const int32 Seed = static_cast<int32>(static_cast<uint32>(Options.Seed) + static_cast<uint32>(Index));
        bSucceeded = BalanceSim::RunOnce(Options, Seed, Output) && bSucceeded;
    }
    return bSucceeded ? 0 : 2;
}
