#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunDungeonPlan.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunEncounterPool.h"
#include "Game/Run/RunLevelDesign.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Unit/UnitDataRules.h"

bool URunStateSubsystem::GetCurrentCombatArenaId(FName& OutArenaId, FText& OutError) const
{
    OutError = NSLOCTEXT("RunPveDifficulty", "InactiveArena", "전투 준비 또는 진행 중에만 현재 전투 무대를 해석할 수 있습니다.");
    if (Phase != ERunPhase::Preparing && Phase != ERunPhase::Combat) return false;
    if (IsTargetRun()) return RunPveDifficulty::ResolveArena(TargetRun, CompletedNodes.Num(), OutArenaId, OutError);
    OutArenaId = NAME_None;
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::ConfigureTargetRun(URunSaveGame* Save, FText& OutError) const
{
    const UTargetRunDefinitionDataAsset* Definition = PartyDefinition && PartyDefinition->TargetRunDefinition ? PartyDefinition->TargetRunDefinition.Get() : GetDefault<UTargetRunDefinitionDataAsset>();
    if (!Save || !Definition->BuildState(Save->TargetRun, OutError)) return false;
    // Freeze CSV difficulty only for the native route; explicit designer assets retain their own groups.
    // 명시적인 제작자 에셋의 편성을 유지하며 기본 경로에만 CSV 난이도를 고정합니다.
    if (!PartyDefinition || !PartyDefinition->TargetRunDefinition)
    {
        int32 PartySize = 0;
        for (const FRunPartyMember& Member : Save->Party) PartySize += Member.bCreated ? 1 : 0;
        if (!RunLevelDesign::Load(Save->TargetRun, PartySize, OutError)) return false;
        if (!RunPveDifficulty::Load(Save->TargetRun.PveDifficulty, OutError)) return false;
    }
    // Freeze the new acquisition policy only for new ordinary Runs, preserving existing saved routes.
    // 기존 저장 경로를 보존하며 새 일반 Run에만 새 획득 정책을 고정합니다.
    Save->WeaponSkillAcquisitionVersion = 1;
    const URunWeaponSkillRulesDataAsset* WeaponRules = PartyDefinition && PartyDefinition->WeaponSkillRules ? PartyDefinition->WeaponSkillRules.Get() : GetDefault<URunWeaponSkillRulesDataAsset>();
    if (!WeaponRules->BuildState(Save->WeaponSkillRules, OutError)) return false;
    if (!RunItemRarityProbabilities::Load(Save->ItemShopState.RarityProbabilities, OutError)) return false;
    if (!RunEncounterPool::Load(Save->TargetRun, OutError)) return false;
    Save->ItemShopState.SelectionVersion = 1;
    Save->SkillShopState = FRunSkillShopState();
    Save->TargetRun.EncounterPool.RemoveAll([](const FRunEncounterOffer& Offer) { return Offer.GetResolvedTag().MatchesTag(FRunEncounterOffer::GetSkillShopTag()); });
    Save->Nodes = RunProgressRules::GetTargetRoute().Nodes;
    if (Save->TargetRun.LevelDesign.SchemaVersion == 1)
    {
        for (int32 Index = 0; Index < Save->Nodes.Num(); ++Index)
        {
            const FRunLevelRule& Rule = Save->TargetRun.LevelDesign.Rules[Index / 2];
            Save->Nodes[Index].DisplayName = FText::Format(NSLOCTEXT("RunLevelDesign", "NodeName", "{0} · {1}/10 · {2}"), Rule.Name, FText::AsNumber(Index / 2 + 1), FText::FromString(Index % 2 == 0 ? TEXT("PvE") : TEXT("로컬 Snapshot")));
        }
    }
    Save->EncounterProgress = FRunEncounterProgress();
    Save->EncounterProgress.SchemaVersion = 2;
    Save->EncounterProgress.AfterCompletedNodeCount = 0;
    if (!UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, 0, 0, Save->EncounterProgress.Offers)) return false;
    Save->GoldRewardState = FRunGoldRewardState();
    Save->Phase = ERunPhase::EncounterChoice;
    const UPartyDefinitionDataAsset* Catalog = PartyDefinition ? PartyDefinition.Get() : GetDefault<UPartyDefinitionDataAsset>();
    FRandomStream ItemRandom(FMath::Rand());
    for (FRunPartyMember& Member : Save->Party)
    {
        if (!Member.bCreated) continue;
        FProfessionDefinition Profession;
        if (!Catalog->ResolveProfession(Member.ClassId, Profession, OutError)) return false;
        Member.CurrentHP = Profession.MaxHP;
        Member.InnateSkills = Member.Skills;
        for (FRunItemDefinition& Item : Member.Items)
        {
            FRunItemDefinition Generated;
            if (!RunWeaponSkillRules::Generate(Item, Save->WeaponSkillRules, ItemRandom, Generated, OutError)) return false;
            Item = MoveTemp(Generated);
        }
        if (!RunEquipmentRules::BuildEquippedSkills(Member, Member.Skills, OutError)) return false;
    }
    if (!ConfigureRecoveryRun(Save, OutError)) return false;
    return RunDungeonPlan::Build(*Save, Save->DungeonState, OutError);
}

bool URunStateSubsystem::ResolveMemberProfession(const FRunPartyMember& Member, FProfessionDefinition& OutProfession, FText& OutError) const
{
    const UPartyDefinitionDataAsset* Catalog = PartyDefinition ? PartyDefinition.Get() : GetDefault<UPartyDefinitionDataAsset>();
    if (!Catalog->ResolveProfession(Member.ClassId, OutProfession, OutError)) return false;
    if (IsTargetRun()) UTargetRunDefinitionDataAsset::ApplyGrowth(TargetRun, CompletedNodes.Num(), OutProfession);
    if (!UnitDataRules::IsValidMaxHP(OutProfession.MaxHP) || !UnitDataRules::IsValidSpeed(OutProfession.Speed))
    {
        OutError = NSLOCTEXT("TargetRun", "GrowthLimits", "성장 적용 후 캐릭터 능력치가 허용 범위를 벗어났습니다.");
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
}
