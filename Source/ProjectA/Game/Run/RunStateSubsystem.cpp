#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveFormat.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunCombatRewards.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"
#include "Unit/UnitDataRules.h"

void URunStateSubsystem::ResetDevelopmentRun()
{
    if (!UDevelopmentCoopSubsystem::IsAvailable() || bManagedRun || ManagedLease) return;
    bCheckpointSaving = false;
    SaveSlot = ResolveCheckpointSlot(FCommandLine::Get());
    RunIdentity = FRunIdentityData();
    Participation = FRunParticipationData();
    EncounterProgress = FRunEncounterProgress();
    TargetRun = FRunTargetState();
    SkillShopState = FRunSkillShopState();
    ItemShopState = FRunItemShopState();
    WeaponSkillAcquisitionVersion = 0;
    WeaponSkillRules = FRunWeaponSkillRulesState();
    GoldRewardState = FRunGoldRewardState();
    PendingGoldRewardState = FRunGoldRewardState();
    PartyMembers.Reset();
    Nodes.Reset();
    CompletedNodes.Reset();
    CurrentNodeId = CurrentEncounterId = NAME_None;
    Phase = ERunPhase::None;
    LastResult = ECombatResult::None;
    CombatCheckpoint = FCombatCheckpointData();
    SaveError = FText::GetEmpty();
    PartyDefinition = nullptr;
    OnRunStateChanged.Broadcast();
}

namespace
{
    TArray<FGuid> ResolveGoldRewardRecipients(const FRunIdentityData& Identity, const FRunParticipationData& Participation, const TArray<FRunPartyMember>& Members, bool bManaged)
    {
        TArray<FGuid> Recipients;
        const bool bSinglePlayer = !bManaged && Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Identity.OriginalParticipants.Num() == 1;
        for (const FRunPartyMember& Member : Members)
        {
            if (!Member.bCreated || !Member.bHasSkillLoadout || !Member.CharacterId.IsValid() || Member.OwnerAccountId.IsEmpty() || (bSinglePlayer && !Member.bPlayerControlled)) continue;
            if (bManaged)
            {
                EPartyControlMode Mode = EPartyControlMode::ServerAI;
                FText Error;
                if (!URunParticipationLibrary::ResolveControlMode(Participation, Identity, Members, Member.CharacterId, Mode, Error) || Mode != EPartyControlMode::Human) continue;
            }
            Recipients.Add(Member.CharacterId);
        }
        return Recipients;
    }

    bool IsValidLocalCaller(const FLocalDevelopmentCallerContext& Context)
    {
        if (!FLocalRunAuthorityStore(Context.StoreNamespace).IsValid() || Context.AccountId.Provider != TEXT("Development") || Context.AccountId.Subject.IsEmpty() || Context.AccountId.Subject.Len() > 256) return false;
        for (TCHAR Character : Context.AccountId.Subject)
        {
            if (Character < 33 || Character > 126) return false;
        }
        return true;
    }
}

URunStateSubsystem::URunStateSubsystem()
{
    SaveSlot = ResolveCheckpointSlot(FCommandLine::Get());
}

void URunStateSubsystem::Deinitialize()
{
    ClearManagedMenuTravel();
    OnRunStateChanged.Clear();
    CloseManagedRun();
    Super::Deinitialize();
}

FString URunStateSubsystem::ResolveCheckpointSlot(const TCHAR* CommandLine)
{
    FString ExplicitSlot;
    if (FParse::Value(CommandLine, TEXT("ProjectASaveSlot="), ExplicitSlot) && !ExplicitSlot.TrimStartAndEnd().IsEmpty())
    {
        return ExplicitSlot;
    }
    FString OpponentSlot;
    if (FParse::Value(CommandLine, TEXT("ProjectAOpponentSnapshot="), OpponentSlot) || FParse::Param(CommandLine, TEXT("ProjectAOpponentSnapshot")))
    {
        const FName SlotId = OpponentSlot.Len() <= 64 ? FName(*OpponentSlot) : NAME_None;
        if (!UPartySnapshotLibrary::GetSaveSlotName(SlotId).IsEmpty())
        {
            return TEXT("ProjectA_SnapshotRun_") + SlotId.ToString();
        }
        // Invalid launch arguments must remain isolated while gameplay reports the selection error.
        // 잘못된 실행 인수도 Gameplay에서 선택 오류를 알리는 동안 일반 진행과 분리합니다.
        return TEXT("ProjectA_RejectedSnapshotRun");
    }
    return TEXT("ProjectA_Run");
}

void URunStateSubsystem::EnableCheckpointSaving(const FString& Slot)
{
    if (!Slot.IsEmpty())
    {
        SaveSlot = Slot;
    }
    bCheckpointSaving = true;
}

void URunStateSubsystem::AutoSaveCheckpoint()
{
    if (bCheckpointSaving && Phase != ERunPhase::Preparing && Phase != ERunPhase::Combat)
    {
        SaveCheckpoint(SaveError);
    }
}

bool URunStateSubsystem::ValidateGoldRewardState(const URunSaveGame* Save) const
{
    const FRunGoldRewardState& Reward = Save->GoldRewardState;
    const bool bItemReward = Reward.SchemaVersion == 2;
    const bool bEmpty = Reward.NodeId.IsNone() && Reward.GoldChoices.IsEmpty() && Reward.Claims.IsEmpty() && Reward.ItemChoices.IsEmpty() && Reward.BonusGold == 0;
    if (!bItemReward && (!Reward.ItemChoices.IsEmpty() || Reward.BonusGold != 0)) return false;
    if (bItemReward && (Save->TargetRun.SchemaVersion != 1 || Save->WeaponSkillAcquisitionVersion != 1)) return false;
    if (Save->TargetRun.SchemaVersion == 1)
    {
        const bool bRewardPhase = Save->Phase == ERunPhase::Result || Save->Phase == ERunPhase::EncounterChoice || Save->Phase == ERunPhase::Shop || Save->Phase == ERunPhase::Map;
        const bool bPveReward = bRewardPhase && Save->Result == ECombatResult::Victory && Save->CompletedNodes.Num() % 2 != 0;
        if (!bPveReward) return Reward.SchemaVersion == 0 && bEmpty;
        const int32 GroupIndex = Save->CompletedNodes.Num() / 2;
        if (!Save->TargetRun.Groups.IsValidIndex(GroupIndex)) return false;
        if (bItemReward)
        {
            FText RewardError;
            if (!RunCombatRewards::Validate(Reward, Save->ItemShopState, Save->WeaponSkillRules, Save->TargetRun.Groups[GroupIndex].GoldChoices, RewardError)) return false;
        }
        else if (Reward.SchemaVersion != 1 || Reward.GoldChoices != Save->TargetRun.Groups[GroupIndex].GoldChoices) return false;
    }
    if (Reward.SchemaVersion == 0) return bEmpty;
    if (Reward.SchemaVersion != 1 && !bItemReward) return false;
    const bool bPostVictory = Save->Phase == ERunPhase::Result || Save->Phase == ERunPhase::EncounterChoice || Save->Phase == ERunPhase::Shop || Save->Phase == ERunPhase::Complete;
    if (bEmpty) return !bPostVictory;
    const int32 ChoiceCount = bItemReward ? Reward.ItemChoices.Num() : Reward.GoldChoices.Num();
    if (Save->Result != ECombatResult::Victory || Save->CompletedNodes.IsEmpty() || Reward.NodeId != Save->CompletedNodes.Last() || Reward.NodeId != Save->CurrentNode || ChoiceCount != 3 || Reward.Claims.Num() > Save->Party.Num() || (!bPostVictory && Save->Phase != ERunPhase::Map)) return false;
    for (int32 Amount : Reward.GoldChoices)
    {
        if (Amount <= 0) return false;
    }
    TSet<FGuid> Claimed;
    const bool bManaged = FRunSaveFormat::IsManaged(Save->Version);
    const bool bSinglePlayer = !bManaged && Save->Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Save->Identity.OriginalParticipants.Num() == 1;
    for (const FRunGoldRewardClaim& Claim : Reward.Claims)
    {
        const FRunPartyMember* Member = Save->Party.FindByPredicate([&Claim](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.CharacterId == Claim.CharacterId; });
        if (!Member || !Member->bHasSkillLoadout || !Claim.CharacterId.IsValid() || Member->OwnerAccountId.IsEmpty() || (bSinglePlayer && !Member->bPlayerControlled) || Claimed.Contains(Claim.CharacterId) || Claim.ChoiceIndex < 0 || Claim.ChoiceIndex >= ChoiceCount) return false;
        Claimed.Add(Claim.CharacterId);
    }
    if (bItemReward)
    {
        // A claimed generated copy belongs to its recipient exactly once; unselected copies never enter inventory.
        // 수령한 생성 사본은 해당 수령인의 인벤토리에 한 번만 존재하며 미선택 사본은 지급되지 않습니다.
        for (int32 Index = 0; Index < Reward.ItemChoices.Num(); ++Index)
        {
            const FRunItemDefinition& Choice = Reward.ItemChoices[Index];
            const FRunGoldRewardClaim* Claim = Reward.Claims.FindByPredicate([Index](const FRunGoldRewardClaim& Entry) { return Entry.ChoiceIndex == Index; });
            int32 OwnedCount = 0;
            for (const FRunPartyMember& Member : Save->Party)
            {
                for (const FRunItemDefinition& Item : Member.Items)
                {
                    if (Item.ItemInstanceId != Choice.ItemInstanceId) continue;
                    if (!Claim || Claim->CharacterId != Member.CharacterId || !RunItemShopCatalog::IsSameDefinition(Choice, Item)) return false;
                    ++OwnedCount;
                }
            }
            if (OwnedCount != (Claim ? 1 : 0)) return false;
        }
    }
    if (Save->Phase != ERunPhase::Result)
    {
        for (const FGuid& CharacterId : ResolveGoldRewardRecipients(Save->Identity, Save->Participation, Save->Party, bManaged))
        {
            if (!Claimed.Contains(CharacterId)) return false;
        }
    }
    return true;
}

bool URunStateSubsystem::ValidateSave(const URunSaveGame* Save, FText& OutError) const
{
    OutError = FText::FromString(TEXT("저장 파일이 손상되었거나 현재 버전·직업 설정과 호환되지 않습니다."));
    FRunSaveFormat Format;
    if (!Save || !FRunSaveFormat::Resolve(Save->Version, Format) || Save->Party.IsEmpty() || Save->Party.Num() > 4)
    {
        return false;
    }
    const FRunRouteDefinition* SavedRoute = RunProgressRules::GetRouteForNodes(Save->Nodes);
    if (!SavedRoute) return false;
    const FRunRouteDefinition& Route = *SavedRoute;
    const FRunProgressView Progress{Save->Nodes, Save->CompletedNodes, Save->CurrentNode, Save->CurrentEncounter, Save->Phase, Save->Result};
    if (!RunProgressRules::ValidateNodes(Route, Progress) || !RunProgressRules::ValidateEncounterProgress(Route, Progress, Save->EncounterProgress)) return false;
    if (!UTargetRunDefinitionDataAsset::Validate(Save->TargetRun, Progress, Save->EncounterProgress, OutError)) return false;
    if (Save->WeaponSkillAcquisitionVersion < 0 || Save->WeaponSkillAcquisitionVersion > 1 || (Save->WeaponSkillAcquisitionVersion == 1 && Save->TargetRun.SchemaVersion != 1)) return false;
    if (!RunWeaponSkillRules::Validate(Save->WeaponSkillRules, OutError) || Save->WeaponSkillRules.SchemaVersion != Save->WeaponSkillAcquisitionVersion) return false;
    if (Save->WeaponSkillAcquisitionVersion == 1 && (Save->SkillShopState.SchemaVersion != 0 || Save->TargetRun.EncounterPool.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag().MatchesTag(FRunEncounterOffer::GetSkillShopTag()); }))) return false;
    if (!ValidateRecoverySave(Save, OutError)) return false;
    if (Save->TargetRun.SchemaVersion == 1 && (Format.bManaged || Save->Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Save->Identity.OriginalParticipants.Num() != 1)) return false;
    if (Save->EncounterProgress.SchemaVersion != 0 && Save->Identity.Origin == ERunIdentityOrigin::LegacyOffline) return false;
    if (!ValidateGoldRewardState(Save)) return false;
    FText ShopError;
    if (!URunEncounterPoolDataAsset::ValidateSkillShop(Save->SkillShopState, ShopError))
    {
        OutError = ShopError;
        return false;
    }
    const FRunWeaponSkillRulesState* FrozenWeaponRules = Save->WeaponSkillRules.SchemaVersion == 1 ? &Save->WeaponSkillRules : nullptr;
    if (!RunItemShopCatalog::Validate(Save->ItemShopState, ShopError, FrozenWeaponRules))
    {
        OutError = ShopError;
        return false;
    }
    const bool bItemShopSelected = Save->EncounterProgress.IsItemShop();
    if (Save->ItemShopState.SelectionVersion != Save->TargetRun.EncounterSelectionVersion) return false;
    if (Save->ItemShopState.SelectionVersion == 1)
    {
        if (Save->WeaponSkillAcquisitionVersion != 1 || Save->ItemShopState.RarityProbabilities.SchemaVersion != 1) return false;
        // Validate selectable shops against their frozen catalog, then bind saved stock to its encounter profile.
        // 선택 가능한 상점의 저장된 카탈로그 적합성을 확인하고 진열 상태를 인카운터 조건에 연결합니다.
        for (const FRunEncounterOffer& Shop : Save->TargetRun.EncounterPool)
        {
            if (!Shop.IsItemShop() || Shop.GroupWeight <= 0.0f || Shop.VariantWeight <= 0.0f) continue;
            FGameplayTagContainer ShopTags(Shop.GetResolvedTag());
            ShopTags.AddTag(Shop.SelectionGroupTag);
            if (!Save->TargetRun.EncounterQuery.IsEmpty() && !Save->TargetRun.EncounterQuery.Matches(ShopTags)) continue;
            TArray<int32> EligibleItems;
            if (!RunItemRarityProbabilities::GetEligibleIndices(Save->ItemShopState.Catalog, Save->ItemShopState.RarityProbabilities, Shop.ItemQuery, EligibleItems, OutError, FrozenWeaponRules)) return false;
            if (EligibleItems.Num() < (Shop.ItemStockPolicyVersion == 1 ? 1 : 5))
            {
                OutError = FText::Format(NSLOCTEXT("RunItemShop", "EmptyProfile", "{0}의 조건에 맞는 상품이 부족합니다. 인카운터 태그와 등급 확률을 확인하세요."), Shop.DisplayName);
                return false;
            }
        }
        if (Save->ItemShopState.Revision > 0)
        {
            const FRunEncounterOffer* ActiveShop = Save->TargetRun.EncounterPool.FindByPredicate([Save](const FRunEncounterOffer& Shop) { return Shop.EncounterId == Save->ItemShopState.ActiveEncounterId; });
            if (!ActiveShop || !ActiveShop->IsItemShop() || ActiveShop->ItemStockPolicyVersion != Save->ItemShopState.ActiveStockPolicyVersion || !FGameplayTagQuery::StaticStruct()->CompareScriptStruct(&ActiveShop->ItemQuery, &Save->ItemShopState.ActiveItemQuery, 0)) return false;
            if (bItemShopSelected && Save->EncounterProgress.SelectedEncounterId != Save->ItemShopState.ActiveEncounterId) return false;
        }
    }
    if (Save->ItemShopState.SchemaVersion == 1 && ((Save->EncounterProgress.SchemaVersion != 1 && Save->EncounterProgress.SchemaVersion != 2) || (bItemShopSelected && Save->ItemShopState.Revision <= 0) || (!Route.bRepeatEncounters && (Save->ItemShopState.Revision > 0) != bItemShopSelected))) return false;
    TSet<FSoftObjectPath> DisplayedItemAssets;
    for (const FRunItemShopOffer& Offer : Save->ItemShopState.Offers)
    {
        if (DisplayedItemAssets.Contains(Offer.Item.Asset)) return false;
        DisplayedItemAssets.Add(Offer.Item.Asset);
    }
    // Legacy saves remain offline; missing or damaged ownership must never downgrade a new save.
    // 기존 저장은 오프라인으로 유지하며 새 저장의 누락·손상된 소유권을 구버전으로 우회하지 않습니다.
    if (Format.bLegacyOffline != (Save->Identity.Origin == ERunIdentityOrigin::LegacyOffline))
    {
        return false;
    }
    const bool bManaged = Format.bManaged;
    const FRunParticipationData EmptyParticipation;
    if (bManaged)
    {
        if (Save->Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Save->Identity.SchemaVersion != URunIdentityLibrary::CurrentSchemaVersion || Save->Identity.OriginalParticipants.Num() < 2) return false;
        FText ParticipationError;
        if (!URunParticipationLibrary::Validate(Save->Participation, Save->Identity, Save->Party, ParticipationError))
        {
            OutError = ParticipationError;
            return false;
        }
    }
    else if (!FRunParticipationData::StaticStruct()->CompareScriptStruct(&Save->Participation, &EmptyParticipation, 0))
    {
        return false;
    }
    const FCombatCheckpointData EmptyCheckpoint;
    if (!Format.bRequiresCombat && !(bManaged && Save->Phase == ERunPhase::Combat) && !FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Save->CombatCheckpoint, &EmptyCheckpoint, 0))
    {
        return false;
    }
    FText IdentityError;
    if (!URunIdentityLibrary::ValidateIdentity(Save->Identity, Save->Party, IdentityError))
    {
        OutError = IdentityError;
        return false;
    }
    if (!bManaged && Save->Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Save->Identity.OriginalParticipants.Num() == 1)
    {
        int32 PlayerSlot = INDEX_NONE;
        FText SelectionError;
        if (!URunParticipationLibrary::ResolveStandalonePlayerSlot(Save->Party, PlayerSlot, SelectionError))
        {
            OutError = SelectionError;
            return false;
        }
    }
    UPartyDefinitionDataAsset* Catalog = Cast<UPartyDefinitionDataAsset>(Save->Catalog.TryLoad());
    if (!Catalog)
    {
        return false;
    }
    TSet<int32> Slots;
    int32 Created = 0;
    int32 Living = 0;
    TSet<FGuid> OwnedItemIds;
    for (const FRunPartyMember& Member : Save->Party)
    {
        if (Member.SlotIndex < 0 || Member.SlotIndex >= 4 || Slots.Contains(Member.SlotIndex) || !FMath::IsFinite(Member.CurrentHP) || Member.CurrentHP < -1.0f || Member.Gold < 0)
        {
            return false;
        }
        Slots.Add(Member.SlotIndex);
        if (!Member.bCreated && !Member.Appearance.IsEmpty()) return false;
        if ((!Member.bCreated || !Member.bHasSkillLoadout) && (Member.Gold != 0 || !Member.Skills.IsEmpty() || !Member.Items.IsEmpty())) return false;
        if (Save->ItemShopState.SchemaVersion == 0 && !Member.Items.IsEmpty()) return false;
        FText MemberError;
        if (!RunEquipmentRules::Validate(Member, MemberError))
        {
            if (!MemberError.IsEmpty()) OutError = MemberError;
            return false;
        }
        for (const FRunItemDefinition& Item : Member.Items)
        {
            const FRunItemDefinition* CatalogItem = Save->ItemShopState.Catalog.FindByPredicate([&Item](const FRunItemDefinition& Candidate) { return Candidate.Asset == Item.Asset; });
            if (!CatalogItem || !RunItemShopCatalog::IsSameBaseDefinition(*CatalogItem, Item)) return false;
            if (FrozenWeaponRules ? !RunWeaponSkillRules::ValidateGeneratedCopy(Item, *FrozenWeaponRules, OutError) : !RunItemShopCatalog::IsSameDefinition(*CatalogItem, Item)) return false;
            if (FrozenWeaponRules)
            {
                if (OwnedItemIds.Contains(Item.ItemInstanceId)) return false;
                OwnedItemIds.Add(Item.ItemInstanceId);
            }
        }
        if (Save->WeaponSkillAcquisitionVersion == 1 && Member.bCreated)
        {
            TArray<FSoftObjectPath> ExpectedSkills;
            if (Member.InnateSkills.IsEmpty() || !RunEquipmentRules::BuildEquippedSkills(Member, ExpectedSkills, OutError) || ExpectedSkills != Member.Skills) return false;
        }
        else if (!Member.InnateSkills.IsEmpty()) return false;
        if (Member.bCreated)
        {
            FProfessionDefinition Definition;
            const bool bInitialHP = Member.CurrentHP == -1.0f && Save->Phase == ERunPhase::Map && Save->CompletedNodes.IsEmpty();
            if (Member.CharacterName.ToString().TrimStartAndEnd().IsEmpty() || (Member.CurrentHP < 0.0f && !bInitialHP))
            {
                return false;
            }
            FText ProfessionError;
            if (!Catalog->ResolveProfession(Member.ClassId, Definition, ProfessionError))
            {
                OutError = FText::Format(NSLOCTEXT("RunCheckpoint", "SavedProfessionUnsupported", "저장된 직업 '{0}'을 현재 직업 설정으로 불러올 수 없습니다. 저장 원본을 유지합니다. {1}"), FText::FromName(Member.ClassId), ProfessionError);
                return false;
            }
            if (Save->TargetRun.LevelDesign.SchemaVersion == 1)
            {
                UTargetRunDefinitionDataAsset::ApplyGrowth(Save->TargetRun, Save->CompletedNodes.Num(), Definition);
                if (!UnitDataRules::IsValidHealth(Definition.MaxHP, Member.CurrentHP)) return false;
            }
            if (!Catalog->ValidateMemberAppearance(Member, MemberError))
            {
                if (!MemberError.IsEmpty()) OutError = MemberError;
                return false;
            }
            TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
            FText SkillsError;
            if (!Catalog->ResolveMemberSkills(Member, Skills, SkillsError))
            {
                OutError = SkillsError;
                return false;
            }
            if (Save->SkillShopState.SchemaVersion == 1 && !Member.bHasSkillLoadout) return false;
            ++Created;
            Living += Member.CurrentHP != 0.0f ? 1 : 0;
        }
    }
    if (Save->TargetRun.LevelDesign.SchemaVersion == 1 && Save->TargetRun.LevelDesign.PartySize != Created) return false;
    const bool bCombat = Save->Phase == ERunPhase::Combat;
    if (!RunProgressRules::ValidatePhase(Progress, Created > 0, Living > 0) || (bCombat && !Format.bSupportsCombat) || (Format.bRequiresCombat && !bCombat))
    {
        return false;
    }
    if (bCombat)
    {
        const FCombatCheckpointData& Checkpoint = Save->CombatCheckpoint;
        if (Save->TargetRun.SchemaVersion == 1)
        {
            const bool bSnapshot = Save->CompletedNodes.Num() % 2 != 0;
            if (Checkpoint.bHasOpponentSnapshot != bSnapshot) return false;
            if (bSnapshot && (Checkpoint.OpponentCatalog != Save->TargetRun.OpponentCatalog || !FPartySnapshot::StaticStruct()->CompareScriptStruct(&Checkpoint.OpponentSnapshot, &Save->TargetRun.Groups[Save->CompletedNodes.Num() / 2].Opponent, 0))) return false;
            if (!bSnapshot && Save->TargetRun.LevelDesign.SchemaVersion == 1)
            {
                // Match immutable enemy values to the frozen roster while preserving live combat costs and damage.
                // 전투 중 비용과 피해를 유지하며 적의 불변 값을 고정 편성과 대조합니다.
                const TArray<FRunMonsterDefinition>& Roster = Save->TargetRun.Groups[Save->CompletedNodes.Num() / 2].EnemyRoster;
                int32 EnemyIndex = 0;
                for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
                {
                    if (Unit.Team != ETeam::Enemy) continue;
                    if (!Roster.IsValidIndex(EnemyIndex)) return false;
                    const FRunMonsterDefinition& Monster = Roster[EnemyIndex++];
                    if (Unit.UnitClass != Monster.UnitClass || Unit.MaxHP != Monster.MaxHP || Unit.MaxAP != Monster.AP || Unit.MaxSubAP != Monster.SAP || Unit.Speed != Monster.Speed || Unit.MoveRange != Monster.MoveRange || Unit.Skills.Num() != 1 || Unit.Skills[0] != Monster.Skill) return false;
                }
                if (EnemyIndex != Roster.Num()) return false;
            }
        }
        if ((Format.bRequiresCurrentCheckpoint && Checkpoint.SchemaVersion != UCombatCheckpointLibrary::CurrentSchemaVersion) || (Format.bRejectsCurrentCheckpoint && Checkpoint.SchemaVersion == UCombatCheckpointLibrary::CurrentSchemaVersion)) return false;
        if (!FRunIdentityData::StaticStruct()->CompareScriptStruct(&Save->Identity, &Checkpoint.Identity, 0) || Checkpoint.NodeId != Save->CurrentNode || Checkpoint.EncounterId != Save->CurrentEncounter)
        {
            return false;
        }
        FText CheckpointError;
        if (!UCombatCheckpointLibrary::Validate(Checkpoint, Save->Party, CheckpointError))
        {
            if (!CheckpointError.IsEmpty()) OutError = CheckpointError;
            return false;
        }
        for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
        {
            if (Unit.Team == ETeam::Player)
            {
                const FRunPartyMember* Member = Save->Party.FindByPredicate([&Unit](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.SlotIndex == Unit.PartySlot; });
                if (!Member || Member->CurrentHP != Unit.HP)
                {
                    OutError = NSLOCTEXT("RunCheckpoint", "PartyHP", "전투 체크포인트와 파티의 체력이 일치하지 않습니다.");
                    return false;
                }
                if (Member->Consumables.Num() != Unit.Consumables.Num()) return false;
                for (int32 Index = 0; Index < Member->Consumables.Num(); ++Index)
                {
                    if (!FRunConsumableStack::StaticStruct()->CompareScriptStruct(&Member->Consumables[Index], &Unit.Consumables[Index], 0)) return false;
                }
                if (Member->bHasSkillLoadout && Member->Skills != Unit.Skills)
                {
                    OutError = NSLOCTEXT("RunCheckpoint", "PartySkills", "전투 체크포인트와 캐릭터의 습득 스킬이 일치하지 않습니다.");
                    return false;
                }
                EPartyControlMode ExpectedMode = EPartyControlMode::Human;
                if (bManaged && (!URunParticipationLibrary::ResolveControlMode(Save->Participation, Save->Identity, Save->Party, Unit.CharacterId, ExpectedMode, OutError) || Unit.PartyControlMode != ExpectedMode))
                {
                    OutError = NSLOCTEXT("RunCheckpoint", "ManagedMode", "전투 조작 방식이 관리 Run의 영구 참여 상태와 일치하지 않습니다.");
                    return false;
                }
            }
        }
    }
    OutError = FText::GetEmpty();
    return true;
}

URunSaveGame* URunStateSubsystem::CreateSaveData() const
{
    URunSaveGame* Save = NewObject<URunSaveGame>();
    Save->Version = FRunSaveFormat::Select(bManagedRun, RunIdentity.Origin, Phase);
    Save->Identity = RunIdentity;
    Save->Participation = Participation;
    Save->EncounterProgress = EncounterProgress;
    Save->TargetRun = TargetRun;
    Save->SkillShopState = SkillShopState;
    Save->ItemShopState = ItemShopState;
    Save->WeaponSkillAcquisitionVersion = WeaponSkillAcquisitionVersion;
    Save->WeaponSkillRules = WeaponSkillRules;
    Save->GoldRewardState = GoldRewardState;
    Save->Party = PartyMembers;
    Save->Nodes = Nodes;
    Save->CompletedNodes = CompletedNodes;
    Save->CurrentNode = CurrentNodeId;
    Save->CurrentEncounter = CurrentEncounterId;
    Save->Phase = Phase;
    Save->Result = LastResult;
    Save->Catalog = FSoftObjectPath(PartyDefinition);
    if (Phase == ERunPhase::Combat) Save->CombatCheckpoint = CombatCheckpoint;
    return Save;
}

bool URunStateSubsystem::WriteSaveData(URunSaveGame* Save, FText& OutError)
{
    if (!ValidateSave(Save, OutError)) return false;
    if (!bManagedRun)
    {
        if (FRunSaveFormat::IsManaged(Save->Version)) return false;
        return FRunCheckpointStorage::Save(Save, SaveSlot, OutError);
    }
    if (!FRunSaveFormat::IsManaged(Save->Version) || !HasManagedLease() || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Save->Identity, &RunIdentity, 0) || !FRunParticipationData::StaticStruct()->CompareScriptStruct(&Save->Participation, &Participation, 0))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedLease", "현재 관리 Run Host의 유효한 실행 lease가 있어야 저장할 수 있습니다.");
        return false;
    }
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save, Payload))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedSerialize", "관리 Run을 저장 데이터로 변환하지 못했습니다.");
        return false;
    }
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Commit(*ManagedLease, ManagedStamp.Revision, Payload, Record, OutError) != ERunAuthorityResult::Success) return false;
    ManagedStamp = Record.Stamp;
    return true;
}

bool URunStateSubsystem::SaveCheckpoint(FText& OutError)
{
    if (Phase == ERunPhase::Combat)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ExplicitBoundary", "전투는 준비 계획과 상태를 함께 검증한 명시적 체크포인트로만 저장할 수 있습니다.");
        SaveError = OutError;
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    if (!WriteSaveData(Save.Get(), OutError))
    {
        SaveError = OutError;
        return false;
    }
    SaveError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::CommitSaveCandidate(URunSaveGame* Save, FText& OutError, bool bRequirePersistence)
{
    // Phase changes choose their envelope before validation; noncombat boundaries never retain a combat payload.
    // 단계 변경은 검증 전에 저장 형식을 선택하며 비전투 경계에 전투 본문을 남기지 않습니다.
    Save->Version = FRunSaveFormat::Select(bManagedRun, Save->Identity.Origin, Save->Phase);
    if (Save->Phase != ERunPhase::Combat) Save->CombatCheckpoint = FCombatCheckpointData();
    // Preserve memory-only runs while publishing persistent mutations only after validation and writing succeed.
    // 메모리 전용 Run의 동작을 유지하며 영속 변경은 검증과 저장이 성공한 뒤에만 공개합니다.
    if ((bCheckpointSaving || bRequirePersistence) && !WriteSaveData(Save, OutError))
    {
        SaveError = OutError;
        return false;
    }
    ApplySaveData(Save, Save->Phase != ERunPhase::Combat);
    SaveError = FText::GetEmpty();
    OutError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::CommitCombatCheckpoint(const FCombatCheckpointData& Checkpoint, FText& OutError)
{
    OutError = NSLOCTEXT("RunCheckpoint", "PlanningBoundary", "현재 전투의 유효한 준비 계획과 저장 권한이 필요합니다. 저장 실패 시 준비 완료를 확정하지 않습니다.");
    SaveError = OutError;
    if (!bCheckpointSaving || !CanMutateManagedRun() || Phase != ERunPhase::Combat || LastResult != ECombatResult::None || Checkpoint.SchemaVersion != UCombatCheckpointLibrary::CurrentSchemaVersion || Checkpoint.NodeId != CurrentNodeId || Checkpoint.EncounterId != CurrentEncounterId || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&RunIdentity, &Checkpoint.Identity, 0)) return false;
    if (Checkpoint.Revision != CombatCheckpoint.Revision + 1 || (HasCombatCheckpoint() && Checkpoint.AttemptId != CombatCheckpoint.AttemptId)) return false;
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    Save->CombatCheckpoint = Checkpoint;
    if (IsTargetRun() && CompletedNodes.Num() % 2 != 0)
    {
        Save->CombatCheckpoint.bHasOpponentSnapshot = true;
        Save->CombatCheckpoint.OpponentCatalog = TargetRun.OpponentCatalog;
        Save->CombatCheckpoint.OpponentSnapshot = TargetRun.Groups[CompletedNodes.Num() / 2].Opponent;
    }
    for (FRunPartyMember& Member : Save->Party)
    {
        if (!Member.bCreated) continue;
        const FCombatCheckpointUnit* Unit = Checkpoint.Units.FindByPredicate([&Member](const FCombatCheckpointUnit& Entry) { return Entry.Team == ETeam::Player && Entry.PartySlot == Member.SlotIndex; });
        if (Unit)
        {
            Member.CurrentHP = Unit->HP;
            Member.Consumables = Unit->Consumables;
        }
    }
    return CommitSaveCandidate(Save.Get(), OutError, true);
}

bool URunStateSubsystem::ValidateCheckpointHost(const FRunAccountId& AccountId, FText& OutError) const
{
    OutError = NSLOCTEXT("RunCheckpoint", "Host", "기존 Host만 이 전투 체크포인트를 복구할 수 있습니다.");
    const bool bLegacyOffline = RunIdentity.Origin == ERunIdentityOrigin::LegacyOffline;
    const bool bValidCaller = bLegacyOffline ? AccountId.IsEmpty() && GetWorld() && GetWorld()->GetNetMode() == NM_Standalone : !AccountId.IsEmpty() && AccountId == RunIdentity.HostAccountId;
    if (!HasCombatCheckpoint() || !bValidCaller || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&RunIdentity, &CombatCheckpoint.Identity, 0))
    {
        return false;
    }
    if (!URunIdentityLibrary::ValidateIdentity(RunIdentity, PartyMembers, OutError))
    {
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::CanContinueSavedRun(FText& OutError) const
{
    return CanContinueSavedRunInternal(false, OutError);
}

bool URunStateSubsystem::CanContinueStandaloneSavedRun(FText& OutError) const
{
    return CanContinueSavedRunInternal(true, OutError);
}

bool URunStateSubsystem::CanContinueSavedRunInternal(bool bStandaloneOnly, FText& OutError) const
{
    if (bManagedRun || ManagedLease)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedActive", "현재 관리 세션을 먼저 닫아야 다른 진행을 불러올 수 있습니다.");
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError)));
    return ValidateContinuableSave(Save.Get(), bStandaloneOnly, OutError);
}

bool URunStateSubsystem::ValidateContinuableSave(const URunSaveGame* Save, bool bStandaloneOnly, FText& OutError) const
{
    if (!Save)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "InvalidSave", "이어할 저장이 없거나 Run 저장 파일이 아닙니다.");
        return false;
    }
    if (!ValidateSave(Save, OutError))
    {
        return false;
    }
    if (Save->Phase == ERunPhase::Combat && Save->CombatCheckpoint.SchemaVersion != UCombatCheckpointLibrary::CurrentSchemaVersion)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "RetiredCombatSave", "기존 순차 턴 전투 저장은 준비 계획 체크포인트로 복구할 수 없습니다. 기존 저장 파일은 보존됩니다.");
        return false;
    }
    if (FRunSaveFormat::IsManaged(Save->Version))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedResumeRequired", "관리 Run은 기준 저장소에서 실행 lease를 획득하는 명시적 재개를 사용해야 합니다.");
        return false;
    }
    if (Save->Phase == ERunPhase::Defeat || Save->Phase == ERunPhase::Complete)
    {
        OutError = FText::FromString(TEXT("종료된 진행입니다. 새 게임을 시작해 주세요."));
        return false;
    }
    if (bStandaloneOnly && Save->Identity.Origin != ERunIdentityOrigin::LegacyOffline && (Save->Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Save->Identity.OriginalParticipants.Num() != 1))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "SessionRequired", "계정 연결 또는 협동 세션이 필요한 저장입니다. 현재 싱글플레이 이어하기 대신 기존 Host와 원래 참가자가 연결된 세션에서 복원해야 합니다.");
        return false;
    }
    return true;
}

bool URunStateSubsystem::LoadCheckpoint(FText& OutError)
{
    return LoadCheckpointInternal(false, OutError);
}

bool URunStateSubsystem::LoadStandaloneCheckpoint(FText& OutError)
{
    return LoadCheckpointInternal(true, OutError);
}

bool URunStateSubsystem::GetStandaloneSurrenderToken(FString& OutToken, FText& OutError) const
{
    OutToken.Reset();
    if (bManagedRun || ManagedLease || (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone))
    {
        OutError = NSLOCTEXT("RunSurrender", "Session", "현재 싱글플레이 메뉴에서 이어갈 수 있는 진행만 포기할 수 있습니다.");
        return false;
    }
    if ((Phase != ERunPhase::None || !PartyMembers.IsEmpty()) && RunIdentity.Origin != ERunIdentityOrigin::LegacyOffline && (RunIdentity.Origin != ERunIdentityOrigin::LocalDevelopment || RunIdentity.OriginalParticipants.Num() != 1))
    {
        OutError = NSLOCTEXT("RunSurrender", "ActiveIdentity", "협동 또는 계정 세션의 진행은 싱글플레이 메뉴에서 포기할 수 없습니다.");
        return false;
    }
    FString Token;
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError, &Token)));
    if (!ValidateContinuableSave(Save.Get(), true, OutError)) return false;
    OutToken = MoveTemp(Token);
    return true;
}

bool URunStateSubsystem::SurrenderStandaloneSavedRun(const FString& ExpectedToken, FText& OutError)
{
    FString CurrentToken;
    if (!GetStandaloneSurrenderToken(CurrentToken, OutError)) return false;
    if (ExpectedToken.IsEmpty() || CurrentToken != ExpectedToken)
    {
        OutError = NSLOCTEXT("RunSurrender", "Changed", "확인 후 저장이 변경되었습니다. 최신 진행을 확인한 뒤 다시 포기해 주세요.");
        return false;
    }
    if (!FRunCheckpointStorage::DeleteIfUnchanged(SaveSlot, ExpectedToken, OutError)) return false;
    // Publish cleared memory only after the confirmed file is deleted, preventing automatic save resurrection.
    // 확인한 파일이 삭제된 뒤에만 메모리 초기화를 공개하여 자동 저장으로 진행이 되살아나지 않게 합니다.
    bCheckpointSaving = false;
    RunIdentity = FRunIdentityData();
    Participation = FRunParticipationData();
    EncounterProgress = FRunEncounterProgress();
    SkillShopState = FRunSkillShopState();
    ItemShopState = FRunItemShopState();
    TargetRun = FRunTargetState();
    WeaponSkillAcquisitionVersion = 0;
    WeaponSkillRules = FRunWeaponSkillRulesState();
    GoldRewardState = FRunGoldRewardState();
    PendingGoldRewardState = FRunGoldRewardState();
    PartyMembers.Reset();
    Nodes.Reset();
    CompletedNodes.Reset();
    CurrentNodeId = CurrentEncounterId = NAME_None;
    Phase = ERunPhase::None;
    LastResult = ECombatResult::None;
    CombatCheckpoint = FCombatCheckpointData();
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::LoadCheckpointInternal(bool bStandaloneOnly, FText& OutError)
{
    if (bManagedRun || ManagedLease)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedActive", "현재 관리 세션을 먼저 닫아야 다른 진행을 불러올 수 있습니다.");
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError)));
    // Recheck the very same loaded object before changing memory, even if the menu checked an older file.
    // 메뉴가 이전 파일을 검사했더라도 메모리를 바꾸기 전에 방금 읽은 동일 객체를 다시 검증합니다.
    if (!ValidateContinuableSave(Save.Get(), bStandaloneOnly, OutError))
    {
        return false;
    }
    ApplySaveData(Save.Get());
    SaveError = FText::GetEmpty();
    bCheckpointSaving = true;
    OnRunStateChanged.Broadcast();
    return true;
}

void URunStateSubsystem::ApplySaveData(const URunSaveGame* Save, bool bResetPendingReward)
{
    RunIdentity = Save->Identity;
    Participation = Save->Participation;
    EncounterProgress = Save->EncounterProgress;
    TargetRun = Save->TargetRun;
    SkillShopState = Save->SkillShopState;
    ItemShopState = Save->ItemShopState;
    WeaponSkillAcquisitionVersion = Save->WeaponSkillAcquisitionVersion;
    WeaponSkillRules = Save->WeaponSkillRules;
    GoldRewardState = Save->GoldRewardState;
    if (bResetPendingReward) PendingGoldRewardState = FRunGoldRewardState();
    bManagedRun = FRunSaveFormat::IsManaged(Save->Version);
    PartyMembers = Save->Party;
    // Upgrade only ordinary single-player selection in memory; the next normal save retains it.
    // 일반 싱글플레이 선택만 메모리에서 보완하며 다음 정상 저장에 유지합니다.
    if (!bManagedRun && RunIdentity.Origin == ERunIdentityOrigin::LocalDevelopment && RunIdentity.OriginalParticipants.Num() == 1)
    {
        int32 PlayerSlot = INDEX_NONE;
        FText SelectionError;
        if (URunParticipationLibrary::ResolveStandalonePlayerSlot(PartyMembers, PlayerSlot, SelectionError))
        {
            for (FRunPartyMember& Member : PartyMembers) Member.bPlayerControlled = Member.bCreated && Member.SlotIndex == PlayerSlot;
        }
    }
    Nodes = Save->Nodes;
    CompletedNodes = Save->CompletedNodes;
    CurrentNodeId = Save->CurrentNode;
    CurrentEncounterId = Save->CurrentEncounter;
    Phase = Save->Phase;
    LastResult = Save->Result;
    CombatCheckpoint = Save->CombatCheckpoint;
    PartyDefinition = Cast<UPartyDefinitionDataAsset>(Save->Catalog.ResolveObject());
}

bool URunStateSubsystem::ConfigureLocalDevelopmentCaller(const FLocalDevelopmentCallerContext& Context, FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "CallerContext", "유효한 개발용 호출자 문맥이 필요하며 같은 GameInstance에서 계정이나 저장소를 변경할 수 없습니다.");
    if (!IsValidLocalCaller(Context)) return false;
    if (bHasLocalCallerContext && (LocalCallerContext.StoreNamespace != Context.StoreNamespace.ToLower() || LocalCallerContext.AccountId != Context.AccountId)) return false;
    LocalCallerContext = Context;
    LocalCallerContext.StoreNamespace = Context.StoreNamespace.ToLower();
    bHasLocalCallerContext = true;
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::HasManagedLease() const
{
    return bManagedRun && bHasLocalCallerContext && ManagedLease && ManagedLease->IsValid() && ManagedLease->GetStamp() == ManagedStamp && ManagedStamp.RunId == RunIdentity.RunId && ManagedStamp.HostEpoch == RunIdentity.HostEpoch && LocalCallerContext.AccountId == RunIdentity.HostAccountId;
}

bool URunStateSubsystem::CanMutateManagedRun() const { return !bManagedRun || HasManagedLease(); }

URunSaveGame* URunStateSubsystem::CreateInitialSaveData(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError) const
{
    URunSaveGame* Save = NewObject<URunSaveGame>();
    const URunEncounterPoolDataAsset* Pool = PartyDefinition && PartyDefinition->RunEncounterPool ? PartyDefinition->RunEncounterPool.Get() : GetDefault<URunEncounterPoolDataAsset>();
    if (!Pool->BuildFixedOffers(Save->EncounterProgress.Offers, OutError)) return nullptr;
    if (!Pool->BuildSkillShop(Save->SkillShopState, OutError)) return nullptr;
    if (!RunItemShopCatalog::Load(Save->ItemShopState.Catalog, OutError)) return nullptr;
    Save->ItemShopState.SchemaVersion = 1;
    if (!Pool->ValidateGoldRewardRange(OutError)) return nullptr;
    Save->GoldRewardState.SchemaVersion = 1;
    Save->EncounterProgress.SchemaVersion = 1;
    Save->Identity = Identity;
    Save->Party = Members;
    if (Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Identity.OriginalParticipants.Num() == 1)
    {
        int32 PlayerSlot = INDEX_NONE;
        if (!URunParticipationLibrary::ResolveStandalonePlayerSlot(Save->Party, PlayerSlot, OutError)) return nullptr;
        for (FRunPartyMember& Member : Save->Party) Member.bPlayerControlled = Member.bCreated && Member.SlotIndex == PlayerSlot;
    }
    Save->Party.Sort([](const FRunPartyMember& Left, const FRunPartyMember& Right) { return Left.SlotIndex < Right.SlotIndex; });
    for (FRunPartyMember& Member : Save->Party)
    {
        if (!Member.bCreated && !Member.Appearance.IsEmpty())
        {
            OutError = NSLOCTEXT("RunCheckpoint", "EmptySlotAppearance", "빈 캐릭터 슬롯에 의상을 저장할 수 없습니다.");
            return nullptr;
        }
        Member.CurrentHP = -1.0f;
        Member.Gold = 0;
        Member.bHasSkillLoadout = Member.bCreated;
        Member.Skills.Reset();
        Member.InnateSkills.Reset();
        Member.Items.Reset();
        Member.Equipment = FRunEquipmentState();
        Member.Consumables.Reset();
        if (Member.bCreated)
        {
            const UPartyDefinitionDataAsset* Catalog = PartyDefinition ? PartyDefinition.Get() : GetDefault<UPartyDefinitionDataAsset>();
            TArray<TObjectPtr<USkillDefinitionDataAsset>> Skills;
            if (!Catalog->ValidateMemberAppearance(Member, OutError)) return nullptr;
            if (!Catalog->ResolveStartingSkills(Member.ClassId, Skills, OutError)) return nullptr;
            for (USkillDefinitionDataAsset* Skill : Skills) Member.Skills.Add(FSoftObjectPath(Skill));
            RunEquipmentRules::InitializeStartingEquipment(Member, Save->ItemShopState.Catalog);
            const bool bSinglePlayer = Identity.Origin == ERunIdentityOrigin::LocalDevelopment && Identity.OriginalParticipants.Num() == 1;
            if (!bSinglePlayer || Member.bPlayerControlled) Member.Gold = Pool->StartingGold;
        }
    }
    Save->Nodes = RunProgressRules::GetPrototypeRoute().Nodes;
    Save->Phase = ERunPhase::Map;
    Save->Catalog = FSoftObjectPath(PartyDefinition);
    return Save;
}

bool URunStateSubsystem::CreateManagedRun(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "CreateContext", "관리 Run은 고정된 개발용 최초 Host가 번호를 가진 원래 참가자 2~4명으로 새로 생성해야 합니다.");
    if (!bHasLocalCallerContext || bManagedRun || ManagedLease || Phase == ERunPhase::Combat || Phase == ERunPhase::Preparing || Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Identity.SchemaVersion != URunIdentityLibrary::CurrentSchemaVersion || Identity.HostEpoch != 1 || Identity.HostAccountId != LocalCallerContext.AccountId || Identity.OriginalParticipants.Num() < 2 || Identity.OriginalParticipants.Num() > 4 || Identity.RunId == RunIdentity.RunId) return false;
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError)) return false;
    TStrongObjectPtr<URunSaveGame> Save(CreateInitialSaveData(Members, Identity, OutError));
    if (!Save) return false;
    Save->Version = FRunSaveFormat::Select(true, Identity.Origin, Save->Phase);
    for (const FRunParticipantData& Participant : Identity.OriginalParticipants)
    {
        Save->Participation.HumanParticipants.Add(Participant.AccountId);
    }
    if (!ValidateSave(Save.Get(), OutError)) return false;
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Payload))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedSerialize", "관리 Run을 저장 데이터로 변환하지 못했습니다.");
        return false;
    }
    TUniquePtr<FLocalRunAuthorityLease> Lease;
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Create(Identity.RunId, Identity.HostEpoch, Payload, Lease, Record, OutError) != ERunAuthorityResult::Success) return false;
    ApplySaveData(Save.Get());
    ManagedLease = MoveTemp(Lease);
    ManagedStamp = Record.Stamp;
    ManagedResumeTarget = Identity.RunId;
    bManagedResumePending = false;
    bCheckpointSaving = true;
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ReadManagedSave(FGuid RunId, FRunAuthorityRecordData& OutRecord, TStrongObjectPtr<URunSaveGame>& OutSave, FText& OutError) const
{
    OutError = NSLOCTEXT("ManagedRun", "ReadContext", "관리 Run을 조회하려면 원래 참가자로 배정된 개발용 호출자 문맥이 필요합니다.");
    if (!bHasLocalCallerContext || !RunId.IsValid()) return false;
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Read(RunId, Record, OutError) != ERunAuthorityResult::Success) return false;
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Record.Payload)));
    if (Save && !RunContentMigration::RemoveDeletedSkills(*Save, OutError)) return false;
    if (!Save || Save->GetClass() != URunSaveGame::StaticClass() || !FRunSaveFormat::IsManaged(Save->Version) || !ValidateSave(Save.Get(), OutError))
    {
        OutError = NSLOCTEXT("ManagedRun", "InvalidPayload", "기준 저장소 본문이 유효한 개발용 v4 관리 Run이 아닙니다.");
        return false;
    }
    if (Record.Stamp.RunId != Save->Identity.RunId || Record.Stamp.HostEpoch != Save->Identity.HostEpoch || !URunIdentityLibrary::IsOriginalParticipant(Save->Identity, LocalCallerContext.AccountId))
    {
        OutError = NSLOCTEXT("ManagedRun", "RecordIdentity", "기준 저장의 Run·Host 세대 또는 조회자의 원래 참가 기록이 일치하지 않습니다.");
        return false;
    }
    OutSave = MoveTemp(Save);
    OutRecord = MoveTemp(Record);
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::ReadManagedRun(FGuid RunId, FManagedRunPreview& OutPreview, FText& OutError) const
{
    FRunAuthorityRecordData Record;
    TStrongObjectPtr<URunSaveGame> Save;
    if (!ReadManagedSave(RunId, Record, Save, OutError)) return false;
    FManagedRunPreview Preview;
    Preview.Stamp = Record.Stamp;
    Preview.Identity = Save->Identity;
    Preview.Participation = Save->Participation;
    Preview.Phase = Save->Phase;
    Preview.bHasCombatCheckpoint = Save->CombatCheckpoint.Revision > 0;
    Preview.CompletedNodeCount = Save->CompletedNodes.Num();
    Preview.TotalNodeCount = Save->Nodes.Num();
    OutPreview = MoveTemp(Preview);
    return true;
}

bool URunStateSubsystem::SetManagedResumeTarget(FGuid RunId, FText& OutError)
{
    FManagedRunPreview Preview;
    if (!ReadManagedRun(RunId, Preview, OutError)) return false;
    if (bManagedRun && RunId != RunIdentity.RunId)
    {
        OutError = NSLOCTEXT("ManagedRun", "ActiveTarget", "활성 관리 Run을 닫기 전에는 재개 대상을 바꿀 수 없습니다.");
        return false;
    }
    ManagedResumeTarget = RunId;
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ResumeManagedRun(const FRunAuthorityStamp& ExpectedStamp, const TArray<FRunAccountId>& HumanParticipants, FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "ResumeContext", "이전 실행을 닫고 최신 확정 기록으로 명시적으로 재개해야 합니다.");
    if (!bHasLocalCallerContext || bManagedRun || ManagedLease || Phase == ERunPhase::Combat || Phase == ERunPhase::Preparing || !ExpectedStamp.IsValid() || ExpectedStamp.HostEpoch == MAX_int32 || ExpectedStamp.Revision == MAX_int64) return false;
    FRunAuthorityRecordData Previous;
    TStrongObjectPtr<URunSaveGame> Save;
    if (!ReadManagedSave(ExpectedStamp.RunId, Previous, Save, OutError)) return false;
    if (Previous.Stamp != ExpectedStamp || Save->Phase == ERunPhase::Complete || Save->Phase == ERunPhase::Defeat)
    {
        OutError = NSLOCTEXT("ManagedRun", "StaleOrEnded", "기록이 변경되었거나 종료된 Run입니다. 최신 기록을 다시 확인해 주세요.");
        return false;
    }
    // Reject old combat payloads before changing participants, Host epochs, leases or stored bytes.
    // 참가자·Host 세대·lease·저장 바이트를 바꾸기 전에 기존 전투 본문을 거절합니다.
    if (Save->Phase == ERunPhase::Combat && Save->CombatCheckpoint.SchemaVersion != UCombatCheckpointLibrary::CurrentSchemaVersion)
    {
        OutError = NSLOCTEXT("ManagedRun", "RetiredCombatSave", "기존 순차 턴 관리 전투는 준비 계획 체크포인트로 재개할 수 없습니다. 기준 저장과 실행 권한은 변경하지 않습니다.");
        return false;
    }
    FRunParticipationData NextParticipation;
    NextParticipation.HumanParticipants = HumanParticipants;
    FRunIdentityData NextIdentity = Save->Identity;
    NextIdentity.HostAccountId = LocalCallerContext.AccountId;
    ++NextIdentity.HostEpoch;
    if (!URunParticipationLibrary::ValidateTransition(Save->Participation, Save->Identity, NextParticipation, NextIdentity, Save->Party, OutError)) return false;
    Save->Identity = NextIdentity;
    Save->Participation = NextParticipation;
    if (Save->Phase == ERunPhase::Combat)
    {
        Save->CombatCheckpoint.Identity = NextIdentity;
        for (FCombatCheckpointUnit& Unit : Save->CombatCheckpoint.Units)
        {
            if (Unit.Team == ETeam::Player && !URunParticipationLibrary::ResolveControlMode(NextParticipation, NextIdentity, Save->Party, Unit.CharacterId, Unit.PartyControlMode, OutError)) return false;
            if (Unit.Team == ETeam::Player && Unit.PartyControlMode == EPartyControlMode::ServerAI)
            {
                FCombatCheckpointRoundPlan* Plan = Save->CombatCheckpoint.RoundPlans.FindByPredicate([&Unit](const FCombatCheckpointRoundPlan& Entry) { return Entry.UnitId == Unit.RoundUnitId; });
                if (!Plan) return false;
                Plan->bReady = true;
            }
        }
    }
    if (!ValidateSave(Save.Get(), OutError)) return false;
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Payload))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedSerialize", "관리 Run을 저장 데이터로 변환하지 못했습니다.");
        return false;
    }
    TUniquePtr<FLocalRunAuthorityLease> Lease;
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Acquire(ExpectedStamp, NextIdentity.HostEpoch, Payload, Lease, Record, OutError) != ERunAuthorityResult::Success) return false;
    ApplySaveData(Save.Get());
    ManagedLease = MoveTemp(Lease);
    ManagedStamp = Record.Stamp;
    ManagedResumeTarget = ExpectedStamp.RunId;
    bManagedResumePending = true;
    bCheckpointSaving = true;
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ConfirmManagedResumeStarted(FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "ConfirmLease", "재개를 확정하려면 현재 Host의 실행 lease가 필요합니다.");
    if (!HasManagedLease()) return false;
    ClearManagedMenuTravel();
    const bool bWasPending = bManagedResumePending;
    bManagedResumePending = false;
    OutError = FText::GetEmpty();
    if (bWasPending) OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::BeginManagedMenuTravel(FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "TravelContext", "현재 GameInstance의 대기 중인 관리 Run만 메뉴에서 이동할 수 있습니다.");
    UGameInstance* Instance = GetGameInstance();
    const FWorldContext* Context = Instance ? Instance->GetWorldContext() : nullptr;
    UEngine* Engine = Instance ? Instance->GetEngine() : nullptr;
    if (!bManagedResumePending || !HasManagedLease() || !Engine || !Context || Context->OwningGameInstance != Instance || !Context->World() || Context->World()->GetNetMode() != NM_Standalone) return false;
    ClearManagedMenuTravel();
    ManagedTravelSessionId = ManagedStamp.SessionId;
    ManagedTravelContextHandle = Context->ContextHandle;
    ManagedTravelEngine = Engine;
    ManagedTravelFailureHandle = Engine->OnTravelFailure().AddUObject(this, &URunStateSubsystem::HandleManagedTravelFailure);
    OutError = FText::GetEmpty();
    return true;
}

void URunStateSubsystem::ClearManagedMenuTravel()
{
    if (ManagedTravelEngine.IsValid() && ManagedTravelFailureHandle.IsValid()) ManagedTravelEngine->OnTravelFailure().Remove(ManagedTravelFailureHandle);
    ManagedTravelFailureHandle.Reset();
    ManagedTravelEngine.Reset();
    ManagedTravelSessionId.Invalidate();
    ManagedTravelContextHandle = NAME_None;
}

void URunStateSubsystem::HandleManagedTravelFailure(UWorld* World, ETravelFailure::Type, const FString&)
{
    UGameInstance* Instance = GetGameInstance();
    const FWorldContext* Context = Instance ? Instance->GetWorldContext() : nullptr;
    if (!bManagedResumePending || !HasManagedLease() || !ManagedTravelSessionId.IsValid() || ManagedTravelSessionId != ManagedStamp.SessionId || !IsValid(World) || World->GetGameInstance() != Instance || !Context || Context->ContextHandle != ManagedTravelContextHandle || Context->World() != World) return;
    // Travel may already have replaced the menu controller; keep the committed Host and AI record for retry.
    // 이동이 메뉴 컨트롤러를 이미 교체했을 수 있으므로 확정된 Host와 AI 기록을 재시도용으로 유지합니다.
    CloseManagedRun();
}

void URunStateSubsystem::CloseManagedRun()
{
    ClearManagedMenuTravel();
    if (!bManagedRun && !ManagedLease) return;
    ManagedLease.Reset();
    ManagedStamp = FRunAuthorityStamp();
    bManagedRun = false;
    bManagedResumePending = false;
    bCheckpointSaving = false;
    RunIdentity = FRunIdentityData();
    Participation = FRunParticipationData();
    EncounterProgress = FRunEncounterProgress();
    TargetRun = FRunTargetState();
    SkillShopState = FRunSkillShopState();
    ItemShopState = FRunItemShopState();
    WeaponSkillAcquisitionVersion = 0;
    WeaponSkillRules = FRunWeaponSkillRulesState();
    GoldRewardState = FRunGoldRewardState();
    PendingGoldRewardState = FRunGoldRewardState();
    PartyMembers.Reset();
    Nodes.Reset();
    CompletedNodes.Reset();
    CurrentNodeId = NAME_None;
    CurrentEncounterId = NAME_None;
    Phase = ERunPhase::None;
    LastResult = ECombatResult::None;
    CombatCheckpoint = FCombatCheckpointData();
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
}

bool URunStateSubsystem::InitializeRun(const TArray<FRunPartyMember>& Members, FText& OutError)
{
    return InitializeStandaloneRun(Members, false, OutError);
}

bool URunStateSubsystem::InitializeTargetRun(const TArray<FRunPartyMember>& Members, FText& OutError)
{
    if (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
    {
        OutError = NSLOCTEXT("TargetRun", "Standalone", "목표 Run의 Snapshot 전투는 현재 로컬 싱글플레이에서만 사용할 수 있습니다.");
        return false;
    }
    return InitializeStandaloneRun(Members, true, OutError);
}

bool URunStateSubsystem::InitializeStandaloneRun(const TArray<FRunPartyMember>& Members, bool bTarget, FText& OutError)
{
    int32 PlayerSlot = INDEX_NONE;
    if (!URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, PlayerSlot, OutError)) return false;
    // Standalone runs use a per-run development identity until an authenticated provider is integrated.
    // 인증 공급자 연동 전까지 싱글플레이는 Run마다 별도의 개발용 식별자를 사용합니다.
    FRunIdentityData Identity;
    Identity.SchemaVersion = URunIdentityLibrary::CurrentSchemaVersion;
    Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
    Identity.RunId = FGuid::NewGuid();
    Identity.HostEpoch = 1;
    FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
    Participant.JoinOrdinal = 1;
    Participant.AccountId.Provider = TEXT("Development");
    Participant.AccountId.Subject = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Identity.HostAccountId = Participant.AccountId;
    TArray<FRunPartyMember> OwnedMembers = Members;
    for (FRunPartyMember& Member : OwnedMembers)
    {
        Member.bPlayerControlled = Member.bCreated && Member.SlotIndex == PlayerSlot;
        Member.CharacterId = Member.bCreated ? FGuid::NewGuid() : FGuid();
        Member.OwnerAccountId = Member.bCreated ? Participant.AccountId : FRunAccountId();
    }
    return InitializeIdentifiedRun(OwnedMembers, Identity, bTarget, OutError);
}

bool URunStateSubsystem::InitializeRunWithIdentity(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError)
{
    return InitializeIdentifiedRun(Members, Identity, false, OutError);
}

bool URunStateSubsystem::InitializeIdentifiedRun(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, bool bTarget, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (bManagedRun || ManagedLease)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedActive", "현재 관리 세션을 먼저 닫아야 다른 진행을 불러올 수 있습니다.");
        return false;
    }
    TSet<int32> UsedSlots;
    int32 CreatedCount = 0;

    if (Members.Num() > 4)
    {
        OutError = FText::FromString(TEXT("A party supports up to four slots. / 파티는 최대 4개 슬롯입니다."));
        return false;
    }

    for (const FRunPartyMember& Member : Members)
    {
        if (Member.SlotIndex < 0 || Member.SlotIndex >= 4 || UsedSlots.Contains(Member.SlotIndex))
        {
            OutError = FText::FromString(TEXT("Party slot indices must be unique and between 0 and 3. / 파티 슬롯 번호는 0~3이며 중복될 수 없습니다."));
            return false;
        }

        UsedSlots.Add(Member.SlotIndex);

        if (Member.bCreated)
        {
            if (Member.ClassId.IsNone() || Member.CharacterName.ToString().TrimStartAndEnd().IsEmpty())
            {
                OutError = FText::FromString(TEXT("Created characters need a name and class. / 생성한 캐릭터의 이름과 직업이 필요합니다."));
                return false;
            }

            ++CreatedCount;
        }
    }

    if (CreatedCount == 0)
    {
        OutError = FText::FromString(TEXT("Create at least one character to start. / 캐릭터를 1명 이상 생성해 주세요."));
        return false;
    }

    if (Identity.Origin == ERunIdentityOrigin::LegacyOffline)
    {
        OutError = FText::FromString(TEXT("새 진행에는 Run·참가자·캐릭터 식별 정보가 필요합니다."));
        return false;
    }
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError))
    {
        return false;
    }
    if (RunIdentity.RunId.IsValid() && RunIdentity.RunId == Identity.RunId)
    {
        OutError = FText::FromString(TEXT("현재 Run의 참가자와 소유권을 새 게임 생성으로 교체할 수 없습니다. 복원은 이어하기를 사용해 주세요."));
        return false;
    }

    TStrongObjectPtr<URunSaveGame> Save(CreateInitialSaveData(Members, Identity, OutError));
    if (!Save) return false;
    if (bTarget)
    {
        if (!ConfigureTargetRun(Save.Get(), OutError) || !ValidateSave(Save.Get(), OutError)) return false;
        return CommitSaveCandidate(Save.Get(), OutError);
    }
    ApplySaveData(Save.Get());
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::CanStartNode(FName NodeId) const
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Map || !Nodes.IsValidIndex(CompletedNodes.Num()))
    {
        return false;
    }

    return Nodes[CompletedNodes.Num()].NodeId == NodeId && !CompletedNodes.Contains(NodeId);
}

bool URunStateSubsystem::BeginEncounter(FName NodeId)
{
    if (!CanStartNode(NodeId))
    {
        return false;
    }

    const FRunNodeDefinition& Node = Nodes[CompletedNodes.Num()];
    GoldRewardState = FRunGoldRewardState();
    if (!IsTargetRun() && SkillShopState.SchemaVersion == 1) GoldRewardState.SchemaVersion = 1;
    PendingGoldRewardState = FRunGoldRewardState();
    CurrentNodeId = Node.NodeId;
    CurrentEncounterId = Node.EncounterId;
    CombatCheckpoint = FCombatCheckpointData();
    LastResult = ECombatResult::None;
    Phase = ERunPhase::Preparing;
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::MarkCombatStarted()
{
    if (!CanMutateManagedRun() || Phase != ERunPhase::Preparing)
    {
        return false;
    }

    Phase = ERunPhase::Combat;
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::CompleteEncounter(ECombatResult Result)
{
    return CompleteEncounter(Result, {});
}

bool URunStateSubsystem::CompleteEncounter(ECombatResult Result, const TMap<int32, float>& FinalPartyHP)
{
    return CompleteEncounter(Result, FinalPartyHP, {});
}

bool URunStateSubsystem::CompleteEncounter(ECombatResult Result, const TMap<int32, float>& FinalPartyHP, const TMap<int32, TArray<FRunConsumableStack>>& FinalConsumables)
{
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_Client) || !CanMutateManagedRun() || Phase != ERunPhase::Combat || (Result != ECombatResult::Victory && Result != ECombatResult::Defeat))
    {
        return false;
    }

    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    for (const TPair<int32, float>& Entry : FinalPartyHP)
    {
        FRunPartyMember* Member = Save->Party.FindByPredicate([&Entry](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.SlotIndex == Entry.Key; });
        if (!Member || !FMath::IsFinite(Entry.Value) || Entry.Value < 0.0f)
        {
            SaveError = NSLOCTEXT("RunCheckpoint", "FinalPartyHP", "전투 결과의 캐릭터 슬롯 또는 최종 체력이 유효하지 않습니다. 기존 파티와 저장을 유지합니다.");
            return false;
        }
        Member->CurrentHP = Entry.Value;
    }
    for (const TPair<int32, TArray<FRunConsumableStack>>& Entry : FinalConsumables)
    {
        FRunPartyMember* Member = Save->Party.FindByPredicate([&Entry](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.SlotIndex == Entry.Key; });
        if (!Member) return false;
        Member->Consumables = Entry.Value;
    }
    if (!ValidateRecoverySave(Save.Get(), SaveError)) return false;
    if (Result == ECombatResult::Victory && Save->TargetRun.LevelDesign.SchemaVersion == 1 && CompletedNodes.Num() % 2 == 0)
    {
        // Apply rest inside the unpublished result only after checking both saved and final HP against the old cap.
        // 이전 상한으로 저장 체력과 최종 체력을 확인한 뒤 미공개 결과 안에서만 휴식 회복을 적용합니다.
        const int32 GroupIndex = CompletedNodes.Num() / 2;
        const UPartyDefinitionDataAsset* Catalog = Cast<UPartyDefinitionDataAsset>(Save->Catalog.TryLoad());
        if (!Catalog || !Save->TargetRun.LevelDesign.Rules.IsValidIndex(GroupIndex)) return false;
        const float RestHP = Save->TargetRun.LevelDesign.Rules[GroupIndex].RestHP;
        if (!UnitDataRules::IsValidAttribute(RestHP)) return false;
        for (FRunPartyMember& Member : Save->Party)
        {
            if (!Member.bCreated) continue;
            const FRunPartyMember* Previous = PartyMembers.FindByPredicate([&Member](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.SlotIndex == Member.SlotIndex; });
            FProfessionDefinition Profession;
            if (!Previous || !Catalog->ResolveProfession(Member.ClassId, Profession, SaveError)) return false;
            FProfessionDefinition Current = Profession;
            UTargetRunDefinitionDataAsset::ApplyGrowth(Save->TargetRun, Save->CompletedNodes.Num(), Current);
            if (!UnitDataRules::IsValidHealth(Current.MaxHP, Previous->CurrentHP) || !UnitDataRules::IsValidHealth(Current.MaxHP, Member.CurrentHP))
            {
                SaveError = NSLOCTEXT("RunLevelDesign", "RestInvalidHP", "휴식 회복 전 체력이 현재 최대 체력을 벗어났습니다. 기존 파티와 저장을 유지합니다.");
                return false;
            }
            UTargetRunDefinitionDataAsset::ApplyGrowth(Save->TargetRun, Save->CompletedNodes.Num() + 1, Profession);
            if (!UnitDataRules::IsValidMaxHP(Profession.MaxHP)) return false;
            if (Member.CurrentHP > 0.0f) Member.CurrentHP = FMath::Min(Profession.MaxHP, Member.CurrentHP + RestHP);
        }
    }
    if (Result == ECombatResult::Victory && IsTargetRun())
    {
        Save->GoldRewardState = FRunGoldRewardState();
        if (CompletedNodes.Num() % 2 == 0)
        {
            if (UsesWeaponSkills())
            {
                if (PendingGoldRewardState.NodeId != CurrentNodeId || PendingGoldRewardState.SchemaVersion != 2)
                {
                    FRandomStream RewardRandom(FMath::Rand());
                    if (!RunCombatRewards::Build(CurrentNodeId, ItemShopState, WeaponSkillRules, TargetRun.Groups[CompletedNodes.Num() / 2].GoldChoices, RewardRandom, PendingGoldRewardState, SaveError)) return false;
                }
                Save->GoldRewardState = PendingGoldRewardState;
            }
            else
            {
                Save->GoldRewardState.SchemaVersion = 1;
                Save->GoldRewardState.NodeId = CurrentNodeId;
                Save->GoldRewardState.GoldChoices = TargetRun.Groups[CompletedNodes.Num() / 2].GoldChoices;
            }
        }
    }
    else if (Result == ECombatResult::Victory && (GoldRewardState.SchemaVersion == 1 || SkillShopState.SchemaVersion == 1))
    {
        if (PendingGoldRewardState.NodeId != CurrentNodeId)
        {
            const URunEncounterPoolDataAsset* Pool = PartyDefinition && PartyDefinition->RunEncounterPool ? PartyDefinition->RunEncounterPool.Get() : GetDefault<URunEncounterPoolDataAsset>();
            if (!Pool->BuildGoldRewards(CurrentNodeId, PendingGoldRewardState, SaveError)) return false;
        }
        Save->GoldRewardState = PendingGoldRewardState;
    }
    Save->Result = Result;

    if (Result == ECombatResult::Victory)
    {
        Save->CompletedNodes.AddUnique(CurrentNodeId);
        Save->Phase = ERunPhase::Result;
    }
    else
    {
        Save->Phase = ERunPhase::Defeat;
    }

    // Failed publication preserves combat and the unpublished roll for an identical retry.
    // 공개 실패 시 동일한 재시도를 위해 전투와 미공개 추첨 결과를 유지합니다.
    return CommitSaveCandidate(Save.Get(), SaveError);
}

bool URunStateSubsystem::AbortEncounter()
{
    if (!CanMutateManagedRun() || HasCombatCheckpoint() || (Phase != ERunPhase::Preparing && (Phase != ERunPhase::Combat || LastResult != ECombatResult::None)))
    {
        return false;
    }

    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    Save->CurrentNode = NAME_None;
    Save->CurrentEncounter = NAME_None;
    Save->Phase = ERunPhase::Map;
    return CommitSaveCandidate(Save.Get(), SaveError);
}

TArray<FGuid> URunStateSubsystem::GetGoldRewardRecipientIds() const
{
    if ((GoldRewardState.SchemaVersion != 1 && GoldRewardState.SchemaVersion != 2) || GoldRewardState.NodeId.IsNone()) return {};
    return ResolveGoldRewardRecipients(RunIdentity, Participation, PartyMembers, bManagedRun);
}

bool URunStateSubsystem::CanContinueAfterRewards() const
{
    if (Phase != ERunPhase::Result || LastResult != ECombatResult::Victory) return false;
    if (GoldRewardState.SchemaVersion == 0) return true;
    const int32 ChoiceCount = GoldRewardState.SchemaVersion == 2 ? GoldRewardState.ItemChoices.Num() : GoldRewardState.GoldChoices.Num();
    if ((GoldRewardState.SchemaVersion != 1 && GoldRewardState.SchemaVersion != 2) || GoldRewardState.NodeId != CurrentNodeId || ChoiceCount != 3) return false;
    for (const FGuid& CharacterId : GetGoldRewardRecipientIds())
    {
        if (!GoldRewardState.Claims.ContainsByPredicate([CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; })) return false;
    }
    return true;
}

bool URunStateSubsystem::SelectGoldReward(const FRunAccountId& AccountId, FGuid CharacterId, FName ExpectedNodeId, int32 ChoiceIndex, FText& OutError)
{
    OutError = NSLOCTEXT("RunGoldReward", "Unavailable", "현재 전투 보상을 선택할 수 없습니다.");
    const bool bItemReward = GoldRewardState.SchemaVersion == 2;
    const bool bValidChoice = bItemReward ? GoldRewardState.ItemChoices.IsValidIndex(ChoiceIndex) : GoldRewardState.GoldChoices.IsValidIndex(ChoiceIndex);
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_Client) || !CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Result || LastResult != ECombatResult::Victory || (GoldRewardState.SchemaVersion != 1 && !bItemReward) || ExpectedNodeId != CurrentNodeId || GoldRewardState.NodeId != ExpectedNodeId || !bValidChoice) return false;
    OutError = NSLOCTEXT("RunGoldReward", "OwnCharacterOnly", "본인이 직접 조작하는 캐릭터의 보상만 선택할 수 있습니다.");
    if (!GetGoldRewardRecipientIds().Contains(CharacterId) || !URunIdentityLibrary::IsCharacterOwner(RunIdentity, PartyMembers, CharacterId, AccountId)) return false;
    OutError = NSLOCTEXT("RunGoldReward", "AlreadyClaimed", "이 캐릭터는 전투 보상을 이미 받았습니다.");
    if (GoldRewardState.Claims.ContainsByPredicate([CharacterId](const FRunGoldRewardClaim& Claim) { return Claim.CharacterId == CharacterId; })) return false;
    const FRunPartyMember* Member = PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId; });
    const int32 Amount = bItemReward ? GoldRewardState.BonusGold : GoldRewardState.GoldChoices[ChoiceIndex];
    OutError = NSLOCTEXT("RunGoldReward", "GoldOverflow", "보유 골드가 최대값을 초과하여 보상을 받을 수 없습니다.");
    if (!Member || Member->Gold < 0 || Amount <= 0 || Member->Gold > MAX_int32 - Amount) return false;
    // Persist the selected copy, common bonus and receipt together before exposing the completed choice.
    // 선택 완료를 공개하기 전에 선택 사본·공통 골드·수령 내역을 한 번에 저장합니다.
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    FRunPartyMember* RewardedMember = Save->Party.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId; });
    RewardedMember->Gold += Amount;
    if (bItemReward) RewardedMember->Items.Add(GoldRewardState.ItemChoices[ChoiceIndex]);
    FRunGoldRewardClaim& Claim = Save->GoldRewardState.Claims.AddDefaulted_GetRef();
    Claim.CharacterId = CharacterId;
    Claim.ChoiceIndex = ChoiceIndex;
    return CommitSaveCandidate(Save.Get(), OutError);
}

bool URunStateSubsystem::ContinueRun()
{
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_Client) || !CanMutateManagedRun() || bManagedResumePending || !CanContinueAfterRewards())
    {
        return false;
    }

    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    Save->CurrentEncounter = NAME_None;
    const FRunRouteDefinition* Route = RunProgressRules::GetRouteForNodes(Save->Nodes);
    if (!Route) return false;
    if (IsTargetRun() && Save->CompletedNodes.Num() < Save->Nodes.Num())
    {
        Save->EncounterProgress.SelectedEncounterId = NAME_None;
        Save->EncounterProgress.bCompleted = false;
        Save->EncounterProgress.AfterCompletedNodeCount = Save->CompletedNodes.Num();
        Save->EncounterProgress.VisitIndex = 0;
        if (!UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, Save->CompletedNodes.Num(), 0, Save->EncounterProgress.Offers)) return false;
    }
    if (Route->bRepeatEncounters && Save->EncounterProgress.SchemaVersion == 1 && Save->CompletedNodes.Num() >= Route->EncounterAfterCompletedNodes && Save->CompletedNodes.Num() < Route->Nodes.Num())
    {
        // Reset the next visit in the same durable transaction as Continue while retaining purchased content.
        // 구매한 콘텐츠는 유지하며 Continue와 같은 저장 트랜잭션에서 다음 방문을 초기화합니다.
        Save->EncounterProgress.SelectedEncounterId = NAME_None;
        Save->EncounterProgress.bCompleted = false;
        Save->EncounterProgress.AfterCompletedNodeCount = Save->CompletedNodes.Num();
    }
    Save->Phase = RunProgressRules::GetContinuationPhase(*Route, Save->CompletedNodes.Num(), Save->EncounterProgress);
    return CommitSaveCandidate(Save.Get(), SaveError);
}

bool URunStateSubsystem::SelectRunEncounter(FName EncounterId)
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::EncounterChoice || EncounterProgress.bCompleted || !EncounterProgress.SelectedEncounterId.IsNone()) return false;
    const FRunEncounterOffer* Offer = EncounterProgress.Offers.FindByPredicate([EncounterId](const FRunEncounterOffer& Candidate) { return Candidate.EncounterId == EncounterId; });
    if (!Offer || !(IsTargetRun() ? Offer->IsSupportedEncounter() : Offer->IsSupportedShop())) return false;
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    Save->EncounterProgress.SelectedEncounterId = EncounterId;
    Save->Phase = ERunPhase::Shop;
    if (Offer->IsService())
    {
        if (Save->TargetRun.Recovery.Revision == MAX_int32) return false;
        ++Save->TargetRun.Recovery.Revision;
    }
    const FRunWeaponSkillRulesState* FrozenWeaponRules = Save->WeaponSkillRules.SchemaVersion == 1 ? &Save->WeaponSkillRules : nullptr;
    if (Offer->IsItemShop() && Save->ItemShopState.SchemaVersion == 1)
    {
        const bool bRolled = Save->ItemShopState.SelectionVersion == 1 ? RunItemShopCatalog::BeginVisit(Save->ItemShopState, Offer->EncounterId, Offer->ItemQuery, Offer->ItemStockPolicyVersion, SaveError, FrozenWeaponRules) : RunItemShopCatalog::Roll(Save->ItemShopState, false, FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag()), SaveError, FrozenWeaponRules);
        if (!bRolled) return false;
    }
    if (!Offer->IsItemShop() && !Offer->IsService() && URunEncounterPoolDataAsset::GetSkillShopOfferCount(Save->SkillShopState) > 0 && !URunEncounterPoolDataAsset::RollSkillShop(Save->SkillShopState, true, SaveError)) return false;
    return CommitSaveCandidate(Save.Get(), SaveError);
}

bool URunStateSubsystem::PurchaseShopOffer(const FRunAccountId& BuyerAccountId, FGuid CharacterId, FName OfferId, FText& OutError, int32 ExpectedShopRevision)
{
    OutError = NSLOCTEXT("RunSkillShop", "Unavailable", "현재 상점에서 구매할 수 없습니다.");
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_Client) || !CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Shop || EncounterProgress.bCompleted || EncounterProgress.SelectedEncounterId.IsNone()) return false;
    const bool bItemShop = EncounterProgress.IsItemShop();
    if (const FRunEncounterOffer* Selected = EncounterProgress.FindSelectedOffer(); Selected && Selected->IsService()) return PurchaseRecoveryOffer(BuyerAccountId, CharacterId, OfferId, OutError, ExpectedShopRevision);
    if (!bItemShop && (UsesWeaponSkills() || SkillShopState.SchemaVersion != 1)) return false;
    const bool bItemReroll = bItemShop && OfferId == FRunItemShopState::GetRerollOfferId();
    const bool bSkillReroll = !bItemShop && OfferId == FRunSkillShopState::GetRerollOfferId();
    const FRunItemShopOffer* ItemOffer = bItemShop ? ItemShopState.Offers.FindByPredicate([OfferId](const FRunItemShopOffer& Candidate) { return Candidate.OfferId == OfferId; }) : nullptr;
    if (bItemShop)
    {
        OutError = NSLOCTEXT("RunItemShop", "ChangedStock", "상점 상품이 변경되었거나 구매할 수 없습니다. 최신 목록에서 다시 선택하세요.");
        if (ItemShopState.SchemaVersion != 1 || ItemShopState.Revision <= 0 || ItemShopState.Revision == MAX_int32 || ExpectedShopRevision != ItemShopState.Revision || (!bItemReroll && (!ItemOffer || ItemOffer->bSold))) return false;
    }
    else if (SkillShopState.Revision > 0)
    {
        OutError = NSLOCTEXT("RunSkillShop", "ChangedStock", "스킬 상점이 변경되었습니다. 최신 목록에서 다시 선택하세요.");
        if (SkillShopState.Revision == MAX_int32 || ExpectedShopRevision != SkillShopState.Revision) return false;
    }
    if (bSkillReroll && (URunEncounterPoolDataAsset::GetSkillShopOfferCount(SkillShopState) <= 0 || SkillShopState.Revision <= 0 || SkillShopState.RerollPrice == MAX_int32)) return false;
    const bool bRecovery = !bItemShop && OfferId == FRunSkillShopState::GetRecoveryOfferId();
    const FRunSkillShopOffer* Offer = SkillShopState.Offers.FindByPredicate([OfferId](const FRunSkillShopOffer& Candidate) { return Candidate.OfferId == OfferId; });
    if (!bItemShop && !bRecovery && !bSkillReroll && !Offer) return false;
    const int32 Price = bItemShop ? (bItemReroll ? ItemShopState.RerollPrice : ItemOffer->Item.Price) : bSkillReroll ? SkillShopState.RerollPrice : bRecovery ? SkillShopState.Recovery.Price : Offer->Price;
    if (Price <= 0) return false;
    const FRunPartyMember* Member = PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.CharacterId == CharacterId; });
    OutError = NSLOCTEXT("RunSkillShop", "OwnCharacterOnly", "본인이 직접 조작하는 생존 캐릭터만 구매할 수 있습니다.");
    if (!Member || !Member->bHasSkillLoadout || !FMath::IsFinite(Member->CurrentHP) || Member->CurrentHP <= 0.0f || !URunIdentityLibrary::IsCharacterOwner(RunIdentity, PartyMembers, CharacterId, BuyerAccountId)) return false;
    if (bManagedRun)
    {
        EPartyControlMode Mode = EPartyControlMode::ServerAI;
        FText ParticipationError;
        if (!URunParticipationLibrary::ResolveControlMode(Participation, RunIdentity, PartyMembers, CharacterId, Mode, ParticipationError))
        {
            OutError = ParticipationError;
            return false;
        }
        if (Mode != EPartyControlMode::Human) return false;
    }
    else if (RunIdentity.Origin == ERunIdentityOrigin::LocalDevelopment && RunIdentity.OriginalParticipants.Num() == 1 && !Member->bPlayerControlled)
    {
        return false;
    }
    float RecoveredHP = Member->CurrentHP;
    if (bRecovery)
    {
        FProfessionDefinition Profession;
        if (!ResolveMemberProfession(*Member, Profession, OutError)) return false;
        if (Member->CurrentHP >= Profession.MaxHP)
        {
            OutError = NSLOCTEXT("RunSkillShop", "AlreadyFullHP", "HP가 이미 가득 차 있습니다.");
            return false;
        }
        RecoveredHP = Profession.MaxHP;
    }
    else if (!bItemShop && !bSkillReroll)
    {
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer->Skill.TryLoad());
        FCombatRoundSkill Definition;
        OutError = NSLOCTEXT("RunSkillShop", "InvalidSkill", "구매할 스킬 데이터가 유효하지 않습니다.");
        if (!IsValid(Skill) || !Skill->ResolveRoundSkill(Definition, OutError)) return false;
        for (const FSoftObjectPath& OwnedPath : Member->Skills)
        {
            const USkillDefinitionDataAsset* Owned = Cast<USkillDefinitionDataAsset>(OwnedPath.TryLoad());
            FCombatRoundSkill OwnedDefinition;
            if (!IsValid(Owned) || !Owned->ResolveRoundSkill(OwnedDefinition, OutError)) return false;
            if (OwnedDefinition.SkillId == Definition.SkillId)
            {
                OutError = NSLOCTEXT("RunSkillShop", "AlreadyOwned", "이미 습득한 스킬입니다.");
                return false;
            }
        }
    }
    if (Member->Gold < Price)
    {
        OutError = NSLOCTEXT("RunSkillShop", "InsufficientGold", "골드가 부족합니다.");
        return false;
    }
    // Commit the balance and purchase effect together before publishing either change.
    // 잔액과 구매 효과를 함께 저장한 뒤 두 변경을 공개합니다.
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    FRunPartyMember* PurchasedMember = Save->Party.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId; });
    PurchasedMember->Gold -= Price;
    if (bItemReroll)
    {
        const FRunWeaponSkillRulesState* FrozenWeaponRules = Save->WeaponSkillRules.SchemaVersion == 1 ? &Save->WeaponSkillRules : nullptr;
        if (!RunItemShopCatalog::Reroll(Save->ItemShopState, OutError, FrozenWeaponRules)) return false;
    }
    else if (bSkillReroll)
    {
        if (!URunEncounterPoolDataAsset::RollSkillShop(Save->SkillShopState, false, OutError)) return false;
    }
    else if (bItemShop)
    {
        PurchasedMember->Items.Add(ItemOffer->Item);
        FRunItemShopOffer* PurchasedOffer = Save->ItemShopState.Offers.FindByPredicate([OfferId](const FRunItemShopOffer& Candidate) { return Candidate.OfferId == OfferId; });
        PurchasedOffer->bSold = true;
        ++Save->ItemShopState.Revision;
    }
    else if (bRecovery) PurchasedMember->CurrentHP = RecoveredHP;
    else PurchasedMember->Skills.Add(Offer->Skill);
    if (!bItemShop && !bSkillReroll && Save->SkillShopState.Revision > 0) ++Save->SkillShopState.Revision;
    return CommitSaveCandidate(Save.Get(), OutError);
}

bool URunStateSubsystem::CanChangeEquipment(const FRunAccountId& AccountId, FGuid CharacterId, FText& OutError) const
{
    OutError = NSLOCTEXT("RunEquipment", "ShopOnly", "장비 변경은 상점 페이즈에서만 가능합니다.");
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_Client) || !CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Shop || EncounterProgress.bCompleted) return false;
    const FRunPartyMember* Member = PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Entry) { return Entry.bCreated && Entry.CharacterId == CharacterId; });
    OutError = NSLOCTEXT("RunEquipment", "OwnerOnly", "본인이 직접 조작하는 생존 캐릭터의 장비만 변경할 수 있습니다.");
    if (!Member || !Member->bHasSkillLoadout || !FMath::IsFinite(Member->CurrentHP) || Member->CurrentHP <= 0.0f || !URunIdentityLibrary::IsCharacterOwner(RunIdentity, PartyMembers, CharacterId, AccountId)) return false;
    if (bManagedRun)
    {
        EPartyControlMode Mode = EPartyControlMode::ServerAI;
        if (!URunParticipationLibrary::ResolveControlMode(Participation, RunIdentity, PartyMembers, CharacterId, Mode, OutError) || Mode != EPartyControlMode::Human) return false;
    }
    else if (RunIdentity.Origin == ERunIdentityOrigin::LocalDevelopment && RunIdentity.OriginalParticipants.Num() == 1 && !Member->bPlayerControlled) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::ChangeEquipment(const FRunAccountId& AccountId, const FRunEquipmentCommand& Command, FText& OutError)
{
    if (!CanChangeEquipment(AccountId, Command.CharacterId, OutError)) return false;
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    FRunPartyMember* Member = Save->Party.FindByPredicate([&Command](const FRunPartyMember& Entry) { return Entry.CharacterId == Command.CharacterId; });
    if (!Member || !RunEquipmentRules::Apply(*Member, Command, OutError)) return false;
    TArray<FRunEquipmentVisual> Visuals;
    if (!RunEquipmentRules::BuildVisuals(*Member, Visuals, OutError)) return false;
    // Commit the complete loadout before publishing the new equipment to UI or future combat actors.
    // UI 또는 다음 전투 액터에 공개하기 전에 전체 장착 구성을 먼저 저장합니다.
    return CommitSaveCandidate(Save.Get(), OutError);
}

bool URunStateSubsystem::LeaveRunEncounter()
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Shop || EncounterProgress.bCompleted || EncounterProgress.SelectedEncounterId.IsNone()) return false;
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    Save->EncounterProgress.bCompleted = true;
    Save->Phase = ERunPhase::Map;
    if (IsTargetRun())
    {
        Save->TargetRun.CompletedEncounterChoices.Add(Save->EncounterProgress.SelectedEncounterId);
        if (Save->EncounterProgress.VisitIndex < 2)
        {
            ++Save->EncounterProgress.VisitIndex;
            Save->EncounterProgress.SelectedEncounterId = NAME_None;
            Save->EncounterProgress.bCompleted = false;
            Save->Phase = ERunPhase::EncounterChoice;
            if (!UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, Save->CompletedNodes.Num(), Save->EncounterProgress.VisitIndex, Save->EncounterProgress.Offers)) return false;
        }
    }
    return CommitSaveCandidate(Save.Get(), SaveError);
}

void URunStateSubsystem::UpdatePartyMemberHP(int32 SlotIndex, float CurrentHP)
{
    if (!CanMutateManagedRun() || Phase != ERunPhase::Combat || !FMath::IsFinite(CurrentHP))
    {
        return;
    }

    for (FRunPartyMember& Member : PartyMembers)
    {
        if (Member.SlotIndex == SlotIndex && Member.bCreated)
        {
            Member.CurrentHP = FMath::Max(0.0f, CurrentHP);
            return;
        }
    }
}
