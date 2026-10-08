#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunEncounterPool.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Unit/UnitDataRules.h"

bool URunStateSubsystem::ConfigureTargetRun(URunSaveGame* Save, FText& OutError) const
{
    const UTargetRunDefinitionDataAsset* Definition = PartyDefinition && PartyDefinition->TargetRunDefinition ? PartyDefinition->TargetRunDefinition.Get() : GetDefault<UTargetRunDefinitionDataAsset>();
    if (!Save || !Definition->BuildState(Save->TargetRun, OutError)) return false;
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
    return ConfigureRecoveryRun(Save, OutError);
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
